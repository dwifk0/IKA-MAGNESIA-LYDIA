#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
# Copyright (C) 2026 Ahmet Efe Nezli
"""
Kamera konularini MJPEG olarak sunar. Panodan (`web_dashboard.py`) BAGIMSIZ.

Neden ayri sunucu — panonun akisinda uc yapisal sorun var ve ucu de bizim
kontrolumuzde degil:

  1. **3 saniyelik bayat kare kesmesi.** `KARE_TAZE_S = 3.0`; kare o sure
     tazelenmezse akis KESILIYOR ve tarayici sayfa yenilenmeden geri gelmiyor.
     Burada boyle bir kural YOK: yeni kare yoksa SON kare tekrar gonderiliyor,
     baglanti hic kapanmiyor.
  2. **Pano alti paneli birden JPEG'e ceviriyor.** Jetson yuk altindayken
     (bugun yuk ortalamasi 5,6) yetisemiyor ve kareler bayatliyor. Burasi
     yalniz istenen kamerayi kodluyor.
  3. **Tarayici baglanti siniri.** Pano tek basina alti uzun omurlu baglanti
     aciyor; Chrome host basina ~6 veriyor. Iki akis birakmak bu baskiyi
     kaldiriyor.

    python3 kamera_akis.py --port 8095

Uclar:  /<ad>  (MJPEG)  ·  /kare/<ad>  (tek JPEG)  ·  /liste  (JSON durum)
        /tarama  (LiDAR, 360 kova, JSON)
        /nisan   (boresight — GET oku, POST yaz)
        /kayit   (kayit bayragi — pano yazar, kaydedici yoklar)
        adlar: on · arka · taret
"""
import argparse
import json
import os
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import cv2
import rclpy
from cv_bridge import CvBridge
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import Image, LaserScan

# 🔴 UC kamera birden. Onceki surumde "arka" adi TARET konusuna bagliydi;
# panoda arka kamera diye taret goruntusu gosteriliyordu. Ayrildi.
# --konular ad=/konu,ad=/konu ile disaridan degistirilebilir.
KONULAR = {
    "on":    "/camera/image_raw",
    "arka":  "/camera/arka/image_raw",
    "taret": "/camera/taret/image_raw",
}
KARE = {ad: [None, 0.0] for ad in KONULAR}      # ad -> [jpeg baytlari, an]

# ── LiDAR ──────────────────────────────────────────────────────────────────
# Tarama da BURADAN sunuluyor, ayri bir surec/port acilmadi: sahada ayakta
# tutulacak servis sayisi az olsun. Uc: /tarama
TARAMA_KONU = "/scan"
TARAMA_BIN  = 360                    # bir derecelik kova
TARAMA      = [None, 0.0, 0.0]       # [menzil listesi (m), an, tavan]

# 🔴 YON: pano `a=0` = aracin burnu, ARTAN a SAAT YONUNDE. ROS LaserScan'da
# aci CCW artar ve 0 ileridir. Bu yuzden kova indisi ACININ TERSI aliniyor.
# Montaj donusu varsa tek yerden duzeltilir — asagidaki sabit.
TARAMA_DONUS_DER = 0

# Kare bu kadar saniyedir tazelenmiyorsa "canli" sayilmaz (bkz. _liste).
TAZE_S = 3.0

# ── Kayit bayragi — BULUSMA NOKTASI ────────────────────────────────────────
# 🔴 NEDEN ARACTA: kayit laptopta aliniyor ama tarayici laptoptaki kaydediciye
# ULASAMIYOR — Windows guvenlik duvari WSL'de acilan portu her adresten
# (localhost, 127.0.0.1, LAN IP'si) kesiyor, olculdu. Yonetici hakki isteyen
# firewall kurali yerine yon cevrildi:
#     pano  --POST-->  arac (bu bayrak)  <--yoklama--  laptoptaki kaydedici
# Arac yalnizca bir boolean tutuyor, video islemiyor — CPU yuku sifir.
# Yan kazanc: dugme TELEFONDAN da calisiyor, kaydediciye erisim gerekmiyor.
KAYIT_BAYRAK = {
    "acik": False,          # pano yaziyor
    "istek_an": 0.0,
    "kaydedici": None,      # kaydedici kendi durumunu buraya yaziyor
    "kaydedici_an": 0.0,
}


# ── Boresight (lazer-kamera ofseti) ────────────────────────────────────────
# 🔴 TEK KAYNAK. Ayni dosyayi `taret_otonom.py` de okuyor. Sayiyi panoda ayri
# tutmak, sahada birini guncelleyip otekini unutmak demekti: nisangah bir yeri
# gosterirken taret baska yere nisan alirdi.
# Uc: GET /nisan (oku) · POST /nisan (yaz, JSON govde).
NISAN_YOLLAR = [
    os.path.expanduser("~/taret/boresight.json"),
    os.path.expanduser("~/ika/testler/taret_f767/boresight.json"),
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "boresight.json"),
]
NISAN_VARSAYILAN = {"ofset_x_px": 27.0, "ofset_y_px": -31.0,
                    "kare_g": 1280, "kare_y": 720, "dogrulandi": False}


def nisan_dosya():
    """Var olan ilk yolu dondurur; hicbiri yoksa YAZILACAK yolu dondurur."""
    for y in NISAN_YOLLAR:
        if os.path.exists(y):
            return y
    return NISAN_YOLLAR[0]


def nisan_oku():
    y = nisan_dosya()
    try:
        with open(y, encoding="utf-8") as f:
            d = json.load(f)
        d["_dosya"] = y
        return d
    except Exception as e:                                       # noqa: BLE001
        d = dict(NISAN_VARSAYILAN)
        d["_dosya"] = y
        d["_hata"] = str(e)
        return d
# ── 180 derece cevrilecek kameralar ────────────────────────────────────────
# Taret ve arka kamera araca TERS montajli; goruntude insanlar tavandan
# sarkiyor. Donanimda cevirme kontrolu YOK (v4l2-ctl ile bakildi: icSpring
# 32e6:9211'de yalnizca parlaklik/kontrast var), usb_cam de cevirmiyor.
#
# 🔴 CEVIRME YALNIZ BU GORUNTULEME YOLUNDA. ROS konularina (/camera/...)
#    DOKUNULMUYOR. Sebep: taretin boresight ofseti (+27/-31) sahada TERS
#    goruntuyle olculdu; konuyu cevirmek o ofsetin isaretini de ters cevirir
#    ve taret yanlis yere nisan alir. Tespit (YOLO) de hala ham konudan
#    besleniyor. Ikisini de duzeltmek ayri bir karar — cevir180.py hazir.
#
# --cevir on,arka,taret  ile degistirilir; bos birakmak icin --cevir ""
# 🔴 10 Eylul 2026: TARET BURADAN CIKARILDI. Taret kamerasi araca PTZ gibi
# TERS montajli; cevirme artik KAYNAKTA yapiliyor (ROS'ta `cevir180` dugumu,
# `taret_otonom.py`'de yakalamadan hemen sonra). Burada da cevirmek goruntuyu
# IKI KEZ dondurup basa getirirdi.
# Arka kamera burada kaliyor: onu yalniz insan izliyor, tespit beslemiyor —
# gorunum yolunda cevirmek yeterli ve ucuz.
CEVIR = {"arka"}
KILIT = threading.Lock()
FPS = 12

# 🔴 KARE BOYUTU — asil darbogaz burasi, Jetson degil.
# Olculdu: Windows ile Jetson arasindaki USB hatti ~157 KB/s tasiyor.
# 640x480 kalite 70'te kare ~35 KB; iki kamerada 4,5 fps'te hat zaten dolu ve
# goruntu "donuyor" gibi gorunuyor. 480x360 kalite 50'de kare ~10 KB'a
# duserek ayni hattan uc-dort kat fazla kare geciyor.
# Izleme icin cozunurluk degil AKICILIK onemli; tespit/kayit ROS tarafinda
# tam cozunurlukte devam ediyor, burasi yalniz operator gorunumu.
GENISLIK = 480
KALITE   = 50

# ── Kamera basina cozunurluk/kalite ────────────────────────────────────────
# 🔴 ARKA KAMERA BILEREK KUCUK. Arac icin islevsel degil, sartname icin
# takildi; kimse ondan detay okumuyor. Buyuk tutmak WiFi hattini ve kayit
# hacmini bosuna mesgul ediyordu — ONEMLI olan on ve taret.
# 480x360 kare ~14 KB, 240x180 kare ~5 KB: arka kamera hattan cikardigi yuku
# ucte birine indiriyor ve o pay onemli iki kameraya kaliyor.
#
# Listede olmayan kamera genel GENISLIK/KALITE degerlerini kullanir.
# --kam-ayar on=480:50,arka=240:35  ile disaridan degistirilebilir.
KAM_AYAR = {
    "arka":  (240, 35),
    # 🔴 TARET NISAN ALINAN KAMERA — hatta ayrilan payin buyugu onun.
    # 10 Eylul: kullanici "kalite kotu oldugundan bir tik hassasiyet dusuyor"
    # dedi. Arka kamerayi 240'a indirmek hatta yer acmisti (uc kamera da
    # 8 fps'e cikmisti); o pay buraya veriliyor.
    # 640/70 kare ~28 KB — arka kameranin biraktigi ~8 KB'nin fazlasi, ama
    # olculen 8 fps'te toplam yuk hala eskisinin altinda.
    "taret": (640, 70),
}


def kam_genislik(ad):
    return KAM_AYAR.get(ad, (GENISLIK, KALITE))[0]


def kam_kalite(ad):
    return KAM_AYAR.get(ad, (GENISLIK, KALITE))[1]


class Dinleyici(Node):
    def __init__(self):
        super().__init__("kamera_akis")
        self.k = CvBridge()
        for ad, konu in KONULAR.items():
            self.create_subscription(
                Image, konu, self._yap(ad), qos_profile_sensor_data)
            self.get_logger().info("%s <- %s" % (ad, konu))
        self.create_subscription(
            LaserScan, TARAMA_KONU, self._tarama, qos_profile_sensor_data)
        self.get_logger().info("tarama <- %s" % TARAMA_KONU)

    def _tarama(self, m):
        """LaserScan -> 360 kovaya indirgenmis menzil listesi.

        Ham tarama 400-642 nokta geliyor ve sayisi her karede degisiyor;
        panoya ham dizi gondermek hem hatti hem cizimi gereksiz mesgul eder.
        Her kovaya DUSEN EN KUCUK menzil yaziliyor — engel kaybolmasin diye
        ortalama degil minimum aliniyor."""
        import math
        n = len(m.ranges)
        if not n:
            return
        kova = [None] * TARAMA_BIN
        tavan = m.range_max if m.range_max and m.range_max < 100 else 12.0
        for i, r in enumerate(m.ranges):
            if r is None or not math.isfinite(r):
                continue
            if r < m.range_min or r > tavan:
                continue
            aci = math.degrees(m.angle_min + i * m.angle_increment)
            k = int(round(-aci + TARAMA_DONUS_DER)) % TARAMA_BIN
            if kova[k] is None or r < kova[k]:
                kova[k] = round(float(r), 3)
        with KILIT:
            TARAMA[0] = kova
            TARAMA[1] = time.time()
            TARAMA[2] = round(float(tavan), 2)

    def _yap(self, ad):
        def geldi(m):
            try:
                g = self.k.imgmsg_to_cv2(m, desired_encoding="bgr8")
                if ad in CEVIR:
                    # Kucultmeden ONCE degil SONRA cevirmek de olurdu; burada
                    # olmasi onemsiz (rotate 640x480'de ~1 ms), sirali dursun.
                    g = cv2.rotate(g, cv2.ROTATE_180)
                gen = kam_genislik(ad)
                if gen and g.shape[1] > gen:
                    o = gen / float(g.shape[1])
                    g = cv2.resize(g, (gen, int(g.shape[0] * o)),
                                   interpolation=cv2.INTER_AREA)
                ok, buf = cv2.imencode(".jpg", g,
                                       [int(cv2.IMWRITE_JPEG_QUALITY), kam_kalite(ad)])
                if ok:
                    with KILIT:
                        KARE[ad][0] = buf.tobytes()
                        KARE[ad][1] = time.time()
            except Exception as e:                               # noqa: BLE001
                self.get_logger().error("%s: %s" % (ad, e),
                                        throttle_duration_sec=5.0)
        return geldi


class Ucu(BaseHTTPRequestHandler):
    def do_GET(self):
        yol = self.path.split("?")[0].strip("/")          # sorgu dizesi YOK SAYILIR
        if yol == "kayit":
            return self._kayit_oku()
        if yol == "nisan":
            return self._nisan_oku()
        if yol == "tarama":
            return self._tarama_uc()
        if yol == "liste":
            return self._liste()
        if yol.startswith("kare/"):
            return self._tek(yol[5:])
        if yol in KARE:
            return self._akis(yol)
        self.send_error(404)

    def _kayit_oku(self):
        with KILIT:
            d = dict(KAYIT_BAYRAK)
        simdi = time.time()
        # Kaydedici 10 sn'dir haber vermiyorsa YOK sayilir. Panonun "kayitta"
        # yazip aslinda hicbir sey yazilmamasi en kotu durum — bunu engelliyor.
        d["kaydedici_taze"] = (d["kaydedici_an"] > 0
                               and simdi - d["kaydedici_an"] < 10)
        if not d["kaydedici_taze"]:
            d["kaydedici"] = None
        self._json(d)

    def do_POST(self):
        """/nisan (boresight) ve /kayit (kayit bayragi) yaziyor."""
        yol_p = self.path.split("?")[0].strip("/")
        if yol_p == "kayit":
            try:
                uz = int(self.headers.get("Content-Length") or 0)
                g = json.loads(self.rfile.read(uz) or b"{}")
                with KILIT:
                    if "acik" in g:
                        KAYIT_BAYRAK["acik"] = bool(g["acik"])
                        KAYIT_BAYRAK["istek_an"] = time.time()
                    # Kaydedici kendi durumunu bildiriyor (kare sayilari vb.)
                    if "kaydedici" in g:
                        KAYIT_BAYRAK["kaydedici"] = g["kaydedici"]
                        KAYIT_BAYRAK["kaydedici_an"] = time.time()
                return self._kayit_oku()
            except Exception as e:                               # noqa: BLE001
                return self._json({"hata": str(e)}, 400)
        if yol_p != "nisan":
            return self.send_error(404)
        try:
            uz = int(self.headers.get("Content-Length") or 0)
            g = json.loads(self.rfile.read(uz) or b"{}")
            eski = nisan_oku()
            # 🔴 Yalnizca DORT alan yazilabilir. Dosyadaki aciklama ve olcum
            # notlari korunuyor — pano onlari bilmiyor, ezerse bilgi kaybolur.
            for k in ("ofset_x_px", "ofset_y_px"):
                if k in g:
                    eski[k] = float(g[k])
            for k in ("kare_g", "kare_y"):
                if k in g:
                    eski[k] = int(g[k])
            if "dogrulandi" in g:
                eski["dogrulandi"] = bool(g["dogrulandi"])
            eski.pop("_dosya", None)
            eski.pop("_hata", None)
            y = nisan_dosya()
            kls = os.path.dirname(y)
            if kls:
                os.makedirs(kls, exist_ok=True)
            with open(y, "w", encoding="utf-8") as f:
                json.dump(eski, f, ensure_ascii=False, indent=2)
            self._json(nisan_oku())
        except Exception as e:                                   # noqa: BLE001
            self._json({"hata": str(e)}, 400)

    def do_OPTIONS(self):                    # tarayici POST oncesi bunu sorar
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def _json(self, nesne, kod=200):
        g = json.dumps(nesne).encode()
        self.send_response(kod)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(g)))
        self.end_headers()
        self.wfile.write(g)

    def _nisan_oku(self):
        self._json(nisan_oku())

    def _tarama_uc(self):
        with KILIT:
            kova, an, tavan = TARAMA[0], TARAMA[1], TARAMA[2]
        g = json.dumps({"menzil": kova, "tavan": tavan,
                        "yas": (None if an == 0.0 else round(time.time() - an, 2)),
                        "konu": TARAMA_KONU}).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(g)))
        self.end_headers()
        self.wfile.write(g)

    def _liste(self):
        """Panonun tek istekte butun kameralari gormesi icin. Yayinci hic
        yoksa `kare` false doner — sayfa 503'leri saymak zorunda kalmaz."""
        simdi = time.time()
        with KILIT:
            d = {}
            for ad in KONULAR:
                yas = (None if KARE[ad][1] == 0.0
                       else round(simdi - KARE[ad][1], 2))
                # 🔴 `kare` ARTIK TAZELIK DEMEK. Once yalnizca "elimde bir
                # jpeg var mi" diyordu; akis dururken son kare bellekte
                # kaldigi icin uc, DONMUS bir goruntuyu 20 dakika boyunca
                # "bagli" diye bildirdi. Kamera dugumu USB'den dusunce panoda
                # hicbir sey degismiyordu — sahada en tehlikeli yanilgi.
                d[ad] = {"konu": KONULAR[ad],
                         "kare": KARE[ad][0] is not None
                                 and yas is not None and yas < TAZE_S,
                         "elde": KARE[ad][0] is not None,
                         "yas": yas,
                         "genislik": kam_genislik(ad),
                         "kalite": kam_kalite(ad),
                         "bayt": (len(KARE[ad][0]) if KARE[ad][0] else 0)}
        g = json.dumps({"kameralar": d, "genislik": GENISLIK,
                        "kalite": KALITE, "cevrilen": sorted(CEVIR)}).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(g)))
        self.end_headers()
        self.wfile.write(g)

    def _tek(self, ad):
        if ad not in KARE:
            return self.send_error(404)
        with KILIT:
            j = KARE[ad][0]
        if j is None:
            return self.send_error(503)
        self.send_response(200)
        self.send_header("Content-Type", "image/jpeg")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(j)))
        self.end_headers()
        self.wfile.write(j)

    def _akis(self, ad):
        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Type",
                         "multipart/x-mixed-replace; boundary=sinir")
        self.end_headers()
        son_an = 0.0
        try:
            while True:
                with KILIT:
                    j, an = KARE[ad][0], KARE[ad][1]
                if j is None:
                    time.sleep(0.2)
                    continue
                # 🔴 Yeni kare gelmediyse SON kare yeniden gonderiliyor.
                # Baglantiyi kapatmak yerine tekrar etmek, tarayicinin
                # goruntuyu dusurmemesini garanti ediyor.
                self.wfile.write(b"--sinir\r\nContent-Type: image/jpeg\r\n"
                                 b"Content-Length: " + str(len(j)).encode() +
                                 b"\r\n\r\n" + j + b"\r\n")
                self.wfile.flush()
                son_an = an
                time.sleep(1.0 / FPS)
        except (BrokenPipeError, ConnectionResetError):
            pass          # tarayici sekmeyi kapatti — normal

    def log_message(self, *a):
        pass


def main():
    a = argparse.ArgumentParser()
    a.add_argument("--port", type=int, default=8095)
    a.add_argument("--genislik", type=int, default=GENISLIK)
    a.add_argument("--kalite", type=int, default=KALITE)
    a.add_argument("--kam-ayar", default=None,
                   help="kamera basina cozunurluk/kalite: on=480:50,arka=240:35")
    a.add_argument("--cevir", default=None,
                   help="180 derece cevrilecek kameralar, virgulle (varsayilan: arka,taret)")
    a.add_argument("--konular", default=None,
                   help="ad=/konu,ad=/konu — varsayilani tumuyle degistirir")
    n = a.parse_args()
    if n.konular:
        globals()["KONULAR"] = dict(
            p.split("=", 1) for p in n.konular.split(",") if "=" in p)
        globals()["KARE"] = {ad: [None, 0.0] for ad in globals()["KONULAR"]}
    globals()["GENISLIK"] = n.genislik
    globals()["KALITE"] = n.kalite
    if n.kam_ayar:
        tablo = {}
        for parca in n.kam_ayar.split(","):
            if "=" not in parca:
                continue
            ad, deg = parca.split("=", 1)
            g, _, k = deg.partition(":")
            tablo[ad.strip()] = (int(g), int(k) if k else n.kalite)
        globals()["KAM_AYAR"] = tablo
    if n.cevir is not None:
        globals()["CEVIR"] = {x.strip() for x in n.cevir.split(",") if x.strip()}
    rclpy.init()
    d = Dinleyici()
    s = ThreadingHTTPServer(("0.0.0.0", n.port), Ucu)
    threading.Thread(target=s.serve_forever, daemon=True).start()
    print("kamera akisi: http://0.0.0.0:%d/  →  %s" % (
        n.port, "  ".join("/" + a for a in KONULAR)), flush=True)
    rclpy.spin(d)


if __name__ == "__main__":
    main()
