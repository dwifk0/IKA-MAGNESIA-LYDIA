#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (C) 2026 Ahmet Efe Nezli
"""
Kamera KAYIT sunucusu — kayit BU PC'de alinir, Jetson'a yuk binmez.

Panodaki KAYIT dugmesi buraya konusur. Uc kamerayi (`on` · `arka` · `taret`)
aractaki `:8095/kare/<ad>` ucundan ceker ve her birini ayri MP4'e yazar.

    python3 kayit_sunucu.py                      # varsayilan: arac <ARAC_IP>
    python3 kayit_sunucu.py --arac <ARAC_IP>  # USB yolu
    python3 kayit_sunucu.py --fps 8 --klasor ~/ika_kayitlar

Uclar (hepsi CORS acik):
    GET  /durum          → {"kayit": bool, "sure": sn, "kameralar": {...}}
    POST /basla          → kaydi baslatir, klasor adini doner
    POST /dur            → kaydi bitirir, dosyalari doner

🔴 NEDEN JETSON'DA DEGIL: 10 Eylul olcumu — Jetson yuk ortalamasi 7,67 ve
kart 6 cekirdekli, yani zaten fazla abone. Uc video kodlayici eklemek kamera
akisini ve tespit hattini yavaslatirdi. Diskte yer sorun degil (384 GB bos),
sorun CPU. Laptop'ta ikisi de bedava.

⚠ KARE KAYNAGI GORUNTULEME AKISI. `:8095` kareleri 480 px / kalite 50 —
izleme icin ayarlanmis. Kayit da bu cozunurlukte olur. Tam cozunurluk
gerekiyorsa kaynak ROS konusundan ayri bir kayit gerekir (Jetson'da, yukuyle
birlikte).

⚠ Pano ARACTAN sunuluyor (`<ARAC_IP>:8080`), bu sunucu ise burada.
Tarayici ikisine de ulasabilmeli: pano bu adrese `localhost:8790` diye
gidiyor ve WSL'de acilan port Windows'tan da gorunuyor. Panoyu BASKA bir
cihazdan (telefon) acarsan kayit dugmesi calismaz — kaydi baslatan makine
bu PC olmak zorunda.
"""

import argparse
import json
import os
import threading
import time
import http.client
import socket
import subprocess
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import cv2
import numpy as np

KAMERALAR = ("on", "arka", "taret")

# Yazici acilmadan once gercek hizin olculdugu sure. Kisa olursa
# olcum gurultulu, uzun olursa bellekte fazla kare birikir.
OLCUM_SN = 3.0

DURUM = {
    "kayit": False,
    "baslangic": 0.0,
    "klasor": None,
    "kare": {ad: 0 for ad in KAMERALAR},
    "atlanan": {ad: 0 for ad in KAMERALAR},
    "olculen_fps": {ad: None for ad in KAMERALAR},
    "hata": None,
}
KILIT = threading.Lock()
_dur_bayrak = threading.Event()
_isler = []
_son_bildirim = 0.0


# 🔴 KAYNAK ADRESI ACIKCA BAGLANIYOR — yoksa TEK KARE GELMIYOR.
# WSL aynali ag kipinde Windows'un butun kartlarini goruyor ve <ARAC_IP>'ye
# giderken kaynak olarak Ethernet kartindaki OTEKI adresi (<ARAC_IP>, ICS
# artigi) seciyor; paket hicbir yere gitmiyor. Ilk denemede 10 saniyede 0 kare
# yazildi, 4 istek zaman asimina ugradi — belirtisi tam buydu.
# `urllib.request` kaynak adres vermeye izin vermiyor, `http.client` veriyor.
KAYNAK_IP = None


def kaynak_bul(hedef):
    """Hedefle AYNI ag onekindeki yerel adresi bulur. Sabit yazmiyoruz:
    arac hem WiFi agindan hem USB agindan gelebiliyor."""
    onek = hedef.rsplit(".", 1)[0] + "."
    try:
        c = subprocess.run(["ip", "-4", "-o", "addr", "show"],
                           capture_output=True, text=True, timeout=5)
        for satir in c.stdout.splitlines():
            p = satir.split()
            if len(p) > 3 and p[3].startswith(onek):
                return p[3].split("/")[0]
    except Exception:                                            # noqa: BLE001
        pass
    return None


class KareCeken:
    """Tek kameranin kare cekicisi. BAGLANTIYI TEKRAR KULLANIR.

    🔴 Her karede yeni TCP baglantisi acmak kaydi ~4 fps'te tikiyordu: 12 sn'lik
    kayit 5,5 sn'lik dosya uretiyor ve video 2 kat hizli oynuyordu (dosya 8 fps
    yaziyor, gercek yakalama yarisi). El sikismasi kareden pahaliydi.

    ⚠ Bu MJPEG DEGIL. Kamera dersi "uzun omurlu baglanti kullanma" diyordu ve
    sebebi `multipart/x-mixed-replace`in HIC BITMEYEN yaniti — Windows guvenlik
    yazilimi onu yarida kesiyordu. Burada her istek ayri ve TAM bitiyor, yalniz
    soket tekrar kullaniliyor; o tuzak burada yok. Yine de baglanti duserse
    sessizce yenisi aciliyor.
    """

    def __init__(self, arac, ad, zaman_asimi=3.0):
        self.arac, self.ad, self.zaman_asimi = arac, ad, zaman_asimi
        self.c = None

    def _ac(self):
        kw = {"timeout": self.zaman_asimi}
        if KAYNAK_IP:
            kw["source_address"] = (KAYNAK_IP, 0)
        self.c = http.client.HTTPConnection(self.arac, 8095, **kw)

    def kapat(self):
        try:
            if self.c:
                self.c.close()
        except Exception:                                        # noqa: BLE001
            pass
        self.c = None

    def cek(self):
        # Iki deneme: birincisi bayat baglantiya denk gelirse ikincisi taze
        # baglantiyla gider. Ucuncusune gerek yok, o zaman gercekten yok.
        for deneme in (1, 2):
            try:
                if self.c is None:
                    self._ac()
                self.c.request("GET", "/kare/%s" % self.ad)
                r = self.c.getresponse()
                govde = r.read()          # yanit TAMAMEN okunmali, yoksa
                if r.status != 200:       # soket bir sonraki istekte bozulur
                    return None
                return govde
            except (OSError, socket.timeout, http.client.HTTPException):
                self.kapat()
                if deneme == 2:
                    return None
        return None


def kaydedici(arac, ad, klasor, fps):
    """Tek kameranin dongusu. Her kamera KENDI is parcaciginda: biri yavas
    ya da olu olsa otekiler tam hizda yazmaya devam etsin."""
    yol = os.path.join(klasor, "%s.mp4" % ad)
    yazici = None
    tampon = []                 # yazici acilana kadar kareler burada bekler
    t_basla = time.time()
    ceken = KareCeken(arac, ad)
    periyot = 1.0 / float(fps)
    sonraki = time.time()

    while not _dur_bayrak.is_set():
        simdi = time.time()
        if simdi < sonraki:
            time.sleep(min(0.02, sonraki - simdi))
            continue
        sonraki += periyot
        # 🔴 Gecikirsek TAKVIMI ILERLET, birikmis kareleri kovalama. Yoksa
        # bir saniyelik takilma sonrasi surekli "gec kaldim" moduna girip
        # hatti doldurur ve video hizlanmis gibi olur.
        if sonraki < simdi:
            sonraki = simdi + periyot

        ham = ceken.cek()
        if ham is None:
            with KILIT:
                DURUM["atlanan"][ad] += 1
            continue

        kare = cv2.imdecode(np.frombuffer(ham, np.uint8), cv2.IMREAD_COLOR)
        if kare is None:
            with KILIT:
                DURUM["atlanan"][ad] += 1
            continue

        # 🔴 DOSYA GERCEK HIZLA YAZILIYOR, HEDEF HIZLA DEGIL.
        # Hedef 8 fps ama ag gidis-donusu yuzunden gercekte ~5 fps aliniyor.
        # Dosyaya 8 yazmak videoyu 1,6 kat HIZLI oynatiyordu: 12 saniyelik
        # kayit 7 saniyelik dosya oluyordu. Cozum: ilk OLCUM_SN boyunca
        # kareler tamponlanir, gercek hiz olculur, yazici ONUNLA acilir ve
        # tampon bosaltilir. Boylece video gercek zamanla ortusuyor.
        if yazici is None:
            tampon.append(kare)
            gecen = time.time() - t_basla
            if gecen < OLCUM_SN:
                with KILIT:
                    DURUM["kare"][ad] += 1
                continue
            olculen = len(tampon) / gecen if gecen > 0 else float(fps)
            if not (0.5 <= olculen <= 120):      # sacma cikarsa hedefe don
                olculen = float(fps)
            y, g = kare.shape[:2]
            yazici = cv2.VideoWriter(yol, cv2.VideoWriter_fourcc(*"mp4v"),
                                     round(olculen, 2), (g, y))
            if not yazici.isOpened():
                with KILIT:
                    DURUM["hata"] = "VideoWriter acilmadi: %s" % yol
                return
            with KILIT:
                DURUM["olculen_fps"][ad] = round(olculen, 2)
            for k in tampon:
                yazici.write(k)
            del tampon[:]
            continue
        yazici.write(kare)
        with KILIT:
            DURUM["kare"][ad] += 1

    ceken.kapat()
    # Kayit OLCUM_SN dolmadan durdurulduysa yazici hic acilmamis olur ve
    # tampondaki kareler kaybolurdu. Burada hedef hizla acip bosaltiyoruz.
    if yazici is None and tampon:
        gecen = max(0.001, time.time() - t_basla)
        olculen = len(tampon) / gecen
        if not (0.5 <= olculen <= 120):
            olculen = float(fps)
        y, g = tampon[0].shape[:2]
        yazici = cv2.VideoWriter(yol, cv2.VideoWriter_fourcc(*"mp4v"),
                                 round(olculen, 2), (g, y))
        if yazici.isOpened():
            with KILIT:
                DURUM["olculen_fps"][ad] = round(olculen, 2)
            for k in tampon:
                yazici.write(k)
    if yazici is not None:
        yazici.release()


def basla(arac, kok, fps):
    with KILIT:
        if DURUM["kayit"]:
            return False, DURUM["klasor"]
    klasor = os.path.join(os.path.expanduser(kok),
                          datetime.now().strftime("%Y-%m-%d_%H-%M-%S"))
    os.makedirs(klasor, exist_ok=True)
    _dur_bayrak.clear()
    with KILIT:
        DURUM.update(kayit=True, baslangic=time.time(), klasor=klasor,
                     kare={ad: 0 for ad in KAMERALAR},
                     atlanan={ad: 0 for ad in KAMERALAR},
                     olculen_fps={ad: None for ad in KAMERALAR}, hata=None)
    del _isler[:]
    for ad in KAMERALAR:
        t = threading.Thread(target=kaydedici, args=(arac, ad, klasor, fps),
                             daemon=True)
        t.start()
        _isler.append(t)
    return True, klasor


def dur():
    with KILIT:
        if not DURUM["kayit"]:
            return None
        klasor = DURUM["klasor"]
    _dur_bayrak.set()
    for t in _isler:
        t.join(timeout=5)
    with KILIT:
        DURUM["kayit"] = False
    dosyalar = []
    for ad in KAMERALAR:
        y = os.path.join(klasor, "%s.mp4" % ad)
        if os.path.exists(y):
            dosyalar.append({"ad": ad, "dosya": y, "bayt": os.path.getsize(y)})
    return {"klasor": klasor, "dosyalar": dosyalar}


class Ucu(BaseHTTPRequestHandler):
    arac = "<ARAC_IP>"
    kok = "~/ika_kayitlar"
    fps = 8

    def _json(self, nesne, kod=200):
        g = json.dumps(nesne).encode()
        self.send_response(kod)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(g)))
        self.end_headers()
        self.wfile.write(g)

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def _durum(self):
        with KILIT:
            d = dict(DURUM)
            d["kare"] = dict(DURUM["kare"])
            d["atlanan"] = dict(DURUM["atlanan"])
            d["olculen_fps"] = dict(DURUM["olculen_fps"])
        d["sure"] = round(time.time() - d["baslangic"], 1) if d["kayit"] else 0
        d["arac"] = self.arac
        d["fps"] = self.fps
        d["kaynak"] = KAYNAK_IP
        return d

    def do_GET(self):
        if self.path.split("?")[0].strip("/") in ("durum", ""):
            return self._json(self._durum())
        self.send_error(404)

    def do_POST(self):
        yol = self.path.split("?")[0].strip("/")
        if yol == "basla":
            ok, klasor = basla(self.arac, self.kok, self.fps)
            return self._json({"ok": ok, "klasor": klasor, **self._durum()})
        if yol == "dur":
            s = dur()
            return self._json({"ok": s is not None, "sonuc": s, **self._durum()})
        self.send_error(404)

    def log_message(self, *a):
        pass


def arac_json(arac, yol, govde=None, zaman_asimi=4.0):
    """Aractaki :8095 ucuna JSON istegi. Kaynak adres burada da SART."""
    try:
        kw = {"timeout": zaman_asimi}
        if KAYNAK_IP:
            kw["source_address"] = (KAYNAK_IP, 0)
        c = http.client.HTTPConnection(arac, 8095, **kw)
        try:
            if govde is None:
                c.request("GET", yol)
            else:
                g = json.dumps(govde).encode()
                c.request("POST", yol, g, {"Content-Type": "application/json"})
            r = c.getresponse()
            ham = r.read()
            if r.status != 200:
                return None
            return json.loads(ham.decode("utf-8", "ignore"))
        finally:
            c.close()
    except Exception:                                            # noqa: BLE001
        return None


# 🔴 0,25 sn. Once 1,0 sn idi ve zincirde UC KAT birikiyordu:
#   pano yoklamasi (1 sn) + gozcunun bayragi gormesi (1 sn) +
#   gozcunun durumu geri bildirmesi (1 sn) = 3 saniyeye kadar gecikme.
# Kullanici "cok yavas ve gecikmeli" dedi; hakliydi. Yoklama yerel aga
# yapiliyor ve govde birkac yuz bayt — 0,25 sn'nin maliyeti yok.
def bayrak_gozcusu(arac, kok, fps, periyot=0.25):
    """Aractaki kayit bayragini yoklar; pano acinca baslatir, kapatinca durdurur.

    🔴 NEDEN BOYLE: tarayici bu makinedeki porta ULASAMIYOR — Windows guvenlik
    duvari WSL'de acilan portu her adresten kesiyor (localhost, 127.0.0.1, LAN
    IP'si; ucu de olculdu, hepsi zaman asimi). Yonetici hakki isteyen bir
    firewall kurali eklemek yerine yon cevrildi: pano ARACA yaziyor, biz
    aractan OKUYORUZ. Cikis baglantisi hicbir yerde engellenmiyor.

    Yan kazanc: kayit dugmesi telefondan da calisiyor.
    """
    while True:
        d = arac_json(arac, "/kayit")
        if d is not None:
            istenen = bool(d.get("acik"))
            with KILIT:
                suanki = DURUM["kayit"]
            if istenen and not suanki:
                basla(arac, kok, fps)
            elif not istenen and suanki:
                dur()
            # Kendi durumumuzu araca bildiriyoruz ki pano kare sayilarini
            # gosterebilsin. Pano bize DOGRUDAN soramiyor (yukaridaki sebep).
            with KILIT:
                ozet = {"kayit": DURUM["kayit"],
                        "sure": round(time.time() - DURUM["baslangic"], 1)
                                if DURUM["kayit"] else 0,
                        "kare": dict(DURUM["kare"]),
                        "olculen_fps": dict(DURUM["olculen_fps"]),
                        "klasor": DURUM["klasor"],
                        "hata": DURUM["hata"]}
            # Kayittayken her turda bildir (sure sayaci akici gorunsun);
            # bostayken saniyede bir yeter — arac bosuna mesgul edilmesin.
            global _son_bildirim
            if ozet["kayit"] or (time.time() - _son_bildirim) > 1.0:
                arac_json(arac, "/kayit", {"kaydedici": ozet})
                _son_bildirim = time.time()
        time.sleep(periyot)


def main():
    a = argparse.ArgumentParser()
    a.add_argument("--arac", default="<ARAC_IP>")
    a.add_argument("--port", type=int, default=8790)
    a.add_argument("--fps", type=int, default=8)
    a.add_argument("--klasor", default="~/ika_kayitlar")
    a.add_argument("--kaynak", default=None,
                   help="baglanirken kullanilacak yerel IP (bos = otomatik bul)")
    n = a.parse_args()
    Ucu.arac, Ucu.kok, Ucu.fps = n.arac, n.klasor, n.fps
    global KAYNAK_IP
    KAYNAK_IP = n.kaynak or kaynak_bul(n.arac)
    print("kaynak adres: %s" % (KAYNAK_IP or "otomatik (bulunamadi)"), flush=True)
    os.makedirs(os.path.expanduser(n.klasor), exist_ok=True)
    threading.Thread(target=bayrak_gozcusu,
                     args=(n.arac, n.klasor, n.fps), daemon=True).start()
    print("bayrak gozcusu acik: %s:8095/kayit yoklaniyor" % n.arac, flush=True)
    s = ThreadingHTTPServer(("0.0.0.0", n.port), Ucu)
    print("kayit sunucusu: http://localhost:%d   arac=%s  fps=%d  klasor=%s"
          % (n.port, n.arac, n.fps, os.path.expanduser(n.klasor)), flush=True)
    try:
        s.serve_forever()
    except KeyboardInterrupt:
        dur()
        print("\nkapatildi")


if __name__ == "__main__":
    main()
