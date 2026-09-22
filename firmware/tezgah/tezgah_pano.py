#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
# Copyright (C) 2026 Ahmet Efe Nezli
"""
Tezgah testleri icin ORTAK canli pano motoru.

`enkoder_saglik_stm32/enkoder_pano.py` her seferinde bastan yazilmasin diye
oradaki pano genellestirildi. Her test artik yalnizca KENDI alanlarini ve
dugmelerini tanimliyor, arayuzu/sunucuyu/seri kopruyu buradan aliyor.

Kullanim (test panosunun icinden):

    from tezgah_pano import Alan, Dugme, Pano

    Pano(
        baslik="BNO055 — F767ZI",
        alanlar=[Alan("yaw", "YAW", birim="derece", olcek=10, grafik=True)],
        dugmeler=[Dugme("s", "Sifirla")],
        varsayilan_port="COM15",
    ).calistir()

FIRMWARE PROTOKOLU — tek satir, ASCII:

    D yaw=1234 roll=-56 kalib=3            <- olcum satiri (D bosluk ile baslar)
    baska her sey                          <- ham kutuge dusen mesaj

Ondalik YOK. newlib-nano'da `%f` kapali (bkz. ika-tezgah-blackpill notu):
sayilari tam sayi gonder, olceklemeyi pano yapar (`olcek=10` -> 1234 = 123,4).

⚠ PANO PORTU TUTAR. Karta yukleme yapmadan once panoyu KAPAT, yoksa
arduino-cli "erisim reddedildi" alir.
"""

import re
import argparse
import json
import sys
import threading
import time
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

# Windows konsolu Turkce kod sayfasinda (cp1254) aciliyor ve panolarin
# bastigi ⚠ gibi karakterlerde UnicodeEncodeError ile cokuyordu.
# WSL'den PYTHONIOENCODING vermek ise ise yaramiyor (WSLENV olmadan
# ortam degiskeni Windows surecine gecmiyor) — cozum burada.
try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

try:
    import serial
except ImportError:
    raise SystemExit("pyserial yok:  python.exe -m pip install pyserial")


# ─────────────────────────────────────────────────────────────────────────────
# Test tanimi
# ─────────────────────────────────────────────────────────────────────────────

# "D " + anahtar= deseni. Ikili cerceve artiklarindan ayirt etmek icin
# anahtarin da eslesmesi sart: rastgele bir ikili baytin "D " uretmesi
# mumkun, ardindan gecerli bir "anahtar=" uretmesi pratikte degil.
_D_BASLANGIC = re.compile(r"D (?=[a-z][a-z0-9_]*=)")


class Alan:
    """Firmware'in `D` satirinda gonderdigi tek bir olcum.

    ad     : `D` satirindaki anahtar (`yaw=123` icin "yaw")
    etiket : kutucukta gorunen isim
    birim  : kutucugun altina yazilir
    olcek  : gelen tam sayi buna BOLUNUR (10 -> onda bir, 100 -> yuzde bir)
    basamak: gosterilecek ondalik basamak sayisi
    grafik : True ise zaman grafigine bir seri olarak eklenir
    iyi    : (deger) -> True/False/None. True yesil, False kirmizi, None notr.
             Kartin "calisiyor mu" sorusunu kutucuk RENGIYLE cevaplar.
    """

    def __init__(self, ad, etiket, birim="", olcek=1, basamak=None,
                 grafik=False, iyi=None, alt=""):
        self.ad = ad
        self.etiket = etiket
        self.birim = birim
        self.olcek = olcek
        if basamak is None:
            basamak = 0 if olcek == 1 else len(str(int(olcek))) - 1
        self.basamak = basamak
        self.grafik = grafik
        self.iyi = iyi
        self.alt = alt

    def sozluk(self):
        return {"ad": self.ad, "etiket": self.etiket, "birim": self.birim,
                "olcek": self.olcek, "basamak": self.basamak,
                "grafik": self.grafik, "alt": self.alt}


class Dugme:
    """Sayfadaki bir dugme. Basilinca `komut` seri porta satir olarak gider.

    sayi=True ise yaninda bir sayi kutusu cikar ve komut "k:<sayi>" olur.
    """

    def __init__(self, komut, etiket, sayi=False, varsayilan=1,
                 en_az=None, en_cok=None, tehlike=False, aciklama="",
                 ayirac=":", yerel=None):
        # 🔴 yerel: verilirse dugme KARTA HICBIR SEY GONDERMEZ, bu fonksiyon
        # panonun icinde cagrilir (orn. sayimi pano tarafinda sifirlamak).
        # 11 Eylul 2026: enk_pano'nun "Sayaci sifirla" dugmesi karta `z`
        # yolluyordu; tezgah firmware'inde `z` enkoderi sifirliyor ama ANA
        # firmware'de "direksiyon: burasi yeni sifir" demek. Aracta basildi
        # ve direksiyon referansini kaydirdi. Ayni harf iki firmware'de iki is.
        self.yerel = yerel
        # 🔴 ayirac: sayili komutun govdeden nasil ayrilacagi. Varsayilan ":"
        # ("s:1200"). AYAR komutu ise "a:<kimlik>,<deger>" biciminde, yani
        # sayi VIRGULLE ekleniyor — `Dugme("a:8", sayi=True, ayirac=",")`.
        # Sabit ":" varsayimi "a:8:170" uretiyordu ve kart bunu REDDEDIYORDU;
        # dugme calisiyormus gibi gorunup hicbir sey yapmiyordu.
        self.ayirac = ayirac
        self.komut = komut
        self.etiket = etiket
        self.sayi = sayi
        self.varsayilan = varsayilan
        self.en_az = en_az
        self.en_cok = en_cok
        self.tehlike = tehlike          # kirmizi basar (dur / acil)
        self.aciklama = aciklama

    def sozluk(self):
        return {"komut": self.komut, "etiket": self.etiket, "sayi": self.sayi,
                "varsayilan": self.varsayilan, "en_az": self.en_az,
                "en_cok": self.en_cok, "tehlike": self.tehlike,
                "aciklama": self.aciklama}


# ─────────────────────────────────────────────────────────────────────────────
# Durum
# ─────────────────────────────────────────────────────────────────────────────

class Durum:
    def __init__(self, alanlar, pencere=900, donustur=None):
        self.kilit = threading.Lock()
        self.alanlar = alanlar
        # donustur(cift) -> cift: gelen olcumu saklamadan ONCE degistirir
        # (pano tarafi sifir noktasi, turetilmis alan). Grafik de bunu gorur.
        self.donustur = donustur
        self.son = {a.ad: 0 for a in alanlar}
        self.gecmis = deque(maxlen=pencere)
        self.log = deque(maxlen=300)
        self.baglanti = "bekliyor"
        self.goruldu = 0.0
        self.satir_sayisi = 0
        self.basla_t = time.time()

    def veri_geldi(self, cift):
        if self.donustur:
            cift = self.donustur(dict(cift))
        with self.kilit:
            for k, v in cift.items():
                self.son[k] = v
            nokta = [round((time.time() - self.basla_t) * 1000)]
            nokta += [self.son.get(a.ad, 0) for a in self.alanlar if a.grafik]
            self.gecmis.append(nokta)
            self.goruldu = time.time()
            self.satir_sayisi += 1

    def log_ekle(self, satir):
        with self.kilit:
            self.log.append({"z": time.strftime("%H:%M:%S"), "s": satir})

    def anlik(self):
        with self.kilit:
            taze = (time.time() - self.goruldu) < 2.0 if self.goruldu else False
            return {"son": dict(self.son), "gecmis": list(self.gecmis),
                    "log": list(self.log), "baglanti": self.baglanti,
                    "taze": taze, "satir": self.satir_sayisi}


# ─────────────────────────────────────────────────────────────────────────────
# Seri kopru
# ─────────────────────────────────────────────────────────────────────────────

class Kopru:
    """Seri portu acik tutar; kart resetlenip port koparsa yeniden baglanir."""

    def __init__(self, durum, port, baud):
        self.durum = durum
        self.port = port
        self.baud = baud
        self.sp = None
        self.yazma_kilidi = threading.Lock()
        self.dur = threading.Event()

    def basla(self):
        threading.Thread(target=self._dongu, daemon=True).start()

    def komut(self, k):
        with self.yazma_kilidi:
            if self.sp and self.sp.is_open:
                self.sp.write((k + "\n").encode())
                self.durum.log_ekle("> " + k)
                return True
        return False

    def _dongu(self):
        while not self.dur.is_set():
            sp = None
            try:
                self.durum.baglanti = "aciliyor: " + self.port
                sp = serial.Serial(self.port, self.baud, timeout=1)
                with self.yazma_kilidi:
                    self.sp = sp
                self.durum.baglanti = "bagli: " + self.port
                self.durum.log_ekle("--- port acildi: " + self.port + " ---")
                tampon = b""
                while not self.dur.is_set():
                    veri = sp.read(256)
                    if veri:
                        tampon += veri
                        while b"\n" in tampon:
                            ham, tampon = tampon.split(b"\n", 1)
                            # 🔴 4 Eylul 2026 — F767'de ayni portta IKILI
                            # telemetri (0xAA cerceveleri) ve bu ASCII satiri
                            # birlikte akiyor. Ikili baytlar satirin ONUNE
                            # yapisiyor, satir artik "D " ile BASLAMIYOR ve
                            # eskiden sessizce atiliyordu: pano butun alanlari
                            # 0 gosteriyor, "kart olmus" sanilliyordu.
                            # Cozum: 0x80 ustu ve kontrol baytlarini at, sonra
                            # "D <anahtar>=" desenini satirin ICINDE ara.
                            # ⚠ Yalniz ASCII basan kartlar icin bu bir NO-OP:
                            # desen zaten 0. konumda bulunur.
                            temiz = bytes(b for b in ham if 32 <= b < 127)
                            self._satir(temiz.decode("ascii", "replace").strip())
            except Exception as e:
                self.durum.baglanti = "KOPUK: " + str(e)
                time.sleep(2)
            finally:
                # Kart resetlenince port kopar. sp kapatilmazsa Windows
                # tutamagi birakmaz ve yeniden baglanma "erisim reddedildi"
                # ile sonsuza kadar takilir.
                with self.yazma_kilidi:
                    self.sp = None
                if sp is not None:
                    try:
                        sp.close()
                    except Exception:
                        pass

    def _satir(self, s):
        if not s:
            return
        # Satirin basindaki ikili artiklari kirp (yukaridaki gerekce).
        m = _D_BASLANGIC.search(s)
        if m and m.start() > 0:
            s = s[m.start():]
        if s.startswith("D ") or s == "D":
            cift = {}
            for parca in s[2:].split():
                if "=" not in parca:
                    continue
                k, _, v = parca.partition("=")
                try:
                    cift[k] = int(v)
                except ValueError:
                    continue
            if cift:
                self.durum.veri_geldi(cift)
                return
        # 🔴 4 Eylul 2026 — eskiden "D " olmayan HER satir loga yaziliyordu.
        # F767'de ayni porttan ikili telemetri de aktigi icin log, ikili
        # cerceve artiklariyla (`0U11U44U62U...`) doluyor ve firmware'in
        # komut cevaplari ("# ELLE KIP ACIK", "# once 'e' ile elle kipi ac")
        # o cöpün arasinda KAYBOLUYORDU. Komut aslinda calisiyor, kullanici
        # calistigini goremiyor -- teshisi tamamen yanlis yone sokan bir kusur.
        #
        # Olcut: firmware'in insana yazdigi her satir '#' ile basliyor
        # (main.cpp, Serial.println(F("# ..."))). Geri kalan ASCII gurultusu
        # bilgi degil.
        # ⚠ '#' ile baslamayan mesaj basan bir kart cikarsa burasi genisletilir;
        # bugun butun tezgah firmware'leri bu kurala uyuyor.
        if s.startswith("#"):
            self.durum.log_ekle(s)


class SahteKopru:
    """--demo: kart yokken arayuzu denemek icin uydurma veri uretir.

    ⚠ HICBIR SEKILDE gercek olcum degildir. Pano ustunde de "DEMO" yazar.
    """

    def __init__(self, durum, uretec):
        self.durum = durum
        self.uretec = uretec
        self.dur = threading.Event()
        self.n = 0

    def basla(self):
        self.durum.baglanti = "DEMO — kart yok, veri uydurma"
        threading.Thread(target=self._dongu, daemon=True).start()

    def komut(self, k):
        self.durum.log_ekle("> " + k + "   (demo: hicbir yere gitmedi)")
        return True

    def _dongu(self):
        while not self.dur.is_set():
            self.durum.veri_geldi(self.uretec(self.n))
            self.n += 1
            time.sleep(0.1)


# ─────────────────────────────────────────────────────────────────────────────
# HTTP
# ─────────────────────────────────────────────────────────────────────────────

def _sunucu_yap(durum, kopru, sayfa, portno, gecerli_komutlar, yerel_komutlar=None):
    yerel_komutlar = yerel_komutlar or {}

    class Islem(BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def _bas(self, kod, tur, govde):
            # 🔴 4 Eylul 2026 — istemci yanit ortasinda giderse (tarayici sekmesi
            # kapandi, curl zaman asimina ugradi) burasi BrokenPipeError
            # atiyordu ve PANO TUMUYLE OLUYORDU. Tezgahta bunun bedeli
            # olcumun ortasinda sessizce sunucuyu kaybetmek. Kopan tek istemci
            # panoyu dusurmemeli.
            try:
                self.send_response(kod)
                self.send_header("Content-Type", tur)
                self.send_header("Content-Length", str(len(govde)))
                self.send_header("Cache-Control", "no-store")
                self.end_headers()
                self.wfile.write(govde)
            except (BrokenPipeError, ConnectionResetError):
                pass

        def handle_one_request(self):
            # Ayni gerekce: baglanti kopmasi is parcacigini gurultuyle
            # sonlandirmasin.
            try:
                BaseHTTPRequestHandler.handle_one_request(self)
            except (BrokenPipeError, ConnectionResetError):
                self.close_connection = True

        def do_GET(self):
            u = urlparse(self.path)
            if u.path == "/":
                self._bas(200, "text/html; charset=utf-8", sayfa.encode())
            elif u.path == "/veri":
                self._bas(200, "application/json",
                          json.dumps(durum.anlik()).encode())
            elif u.path == "/komut":
                k = (parse_qs(u.query).get("k") or [""])[0]
                # Yerel dugme: karta gitmez, panonun icinde calisir.
                if k in yerel_komutlar:
                    b = yerel_komutlar[k]
                    b.yerel()
                    durum.log_ekle("> (pano) " + b.etiket + " — karta komut gitmedi")
                    self._bas(200, "application/json", b'{"ok": true}')
                    return
                # 🔴 Dogrulama ONCE TAM ESLESMEYE bakiyor. Eskiden yalniz
                # "iki nokta oncesi" araniyordu: `Dugme("a:8,180")` gibi hazir
                # bir komutta kok "a" cikiyor, o da gecerli listede olmadigi
                # icin komut sessizce REDDEDILIYORDU ({"ok": false}) — dugmeler
                # calisiyormus gibi gorunup hicbir sey yapmiyordu.
                ok = False
                if k in gecerli_komutlar:
                    ok = kopru.komut(k)
                    self._bas(200, "application/json",
                              json.dumps({"ok": ok}).encode())
                    return
                # Sayili komut: govde gecerli listede, kalani sayi olmali.
                # Ayirac ":" ya da "," olabilir (ayar komutu virgul kullaniyor).
                kok = None
                for ay in (":", ","):
                    if ay in k and k.rsplit(ay, 1)[0] in gecerli_komutlar:
                        kok = k.rsplit(ay, 1)[0]
                        break
                if kok is not None:
                    # Govdeden sonraki kisim SAYI olmali — porta rastgele
                    # metin gitmesin. Ayar komutunda ("a:8,170") govde "a:8",
                    # kalan "170"; sayili surus komutunda ("s:1200") govde
                    # "s", kalan "1200".
                    arg = k[len(kok) + 1:]
                    try:
                        int(arg)
                        ok = kopru.komut(k)
                    except ValueError:
                        ok = False
                self._bas(200, "application/json", json.dumps({"ok": ok}).encode())
            else:
                self._bas(404, "text/plain", b"yok")

    return ThreadingHTTPServer(("0.0.0.0", portno), Islem)


# ─────────────────────────────────────────────────────────────────────────────
# Sayfa
# ─────────────────────────────────────────────────────────────────────────────

SAYFA = r"""<!doctype html>
<html lang="tr"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>__BASLIK__</title>
<style>
:root{
  --paper:#F4F6F5; --surface:#FFFFFF; --surface-2:#EAEEEC;
  --ink:#131A17; --ink-2:#4C5854; --ink-3:#727E79;
  --line:#D3DAD6; --accent:#1A6BC7;
  --iyi:#1E7B3C; --iyi-bg:#E3F2E7;
  --kotu:#B3251B; --kotu-bg:#FBE6E3;
}
@media (prefers-color-scheme:dark){:root{
  --paper:#0B100F; --surface:#141B19; --surface-2:#1C2422;
  --ink:#E3E9E5; --ink-2:#9DA9A4; --ink-3:#7C8883;
  --line:#2A3431; --accent:#3B86D8;
  --iyi:#3FA35A; --iyi-bg:#14271A;
  --kotu:#D9564A; --kotu-bg:#2C1614;
}}
*{box-sizing:border-box}
body{margin:0;background:var(--paper);color:var(--ink);
  font-family:"Segoe UI",system-ui,-apple-system,sans-serif;font-size:16px;line-height:1.5}
.wrap{max-width:64rem;margin:0 auto;padding:1.25rem 1rem 4rem;
  display:flex;flex-direction:column;gap:1.1rem}
header{display:flex;flex-wrap:wrap;align-items:baseline;gap:.5rem 1rem}
h1{margin:0;font-size:1.35rem;font-weight:650;letter-spacing:-.015em}
.pill{font-family:ui-monospace,Consolas,monospace;font-size:.72rem;
  padding:.2rem .55rem;border-radius:99px;border:1px solid var(--line);color:var(--ink-2)}
.pill.canli{color:var(--iyi);border-color:var(--iyi);background:var(--iyi-bg)}
.pill.olu{color:var(--kotu);border-color:var(--kotu);background:var(--kotu-bg)}
.not{font-size:.82rem;color:var(--ink-2);margin:0}
.not b{color:var(--kotu)}

.tiles{display:grid;gap:.7rem;grid-template-columns:repeat(auto-fit,minmax(11rem,1fr))}
.tile{background:var(--surface);border:1px solid var(--line);border-radius:10px;
  padding:.8rem .9rem;display:flex;flex-direction:column;gap:.15rem}
.tile .k{font-family:ui-monospace,Consolas,monospace;font-size:.66rem;
  letter-spacing:.1em;text-transform:uppercase;color:var(--ink-3)}
.tile .v{font-size:2rem;font-weight:650;letter-spacing:-.03em;
  font-variant-numeric:tabular-nums;line-height:1.1}
.tile .d{font-size:.78rem;color:var(--ink-2);min-height:1.2em}
.tile.iyi{border-color:var(--iyi);border-width:2px;background:var(--iyi-bg)}
.tile.kotu{border-color:var(--kotu);border-width:2px;background:var(--kotu-bg)}

.panel{background:var(--surface);border:1px solid var(--line);border-radius:10px;
  padding:.9rem 1rem;display:flex;flex-direction:column;gap:.6rem}
.panel h2{margin:0;font-size:.95rem;font-weight:650}
.legend{display:flex;gap:1rem;flex-wrap:wrap;font-size:.75rem;color:var(--ink-2)}
.legend i{display:inline-block;width:.85rem;height:0;border-top:2px solid;
  vertical-align:.25em;margin-right:.35rem}
#cwrap{position:relative}
canvas{display:block;width:100%;height:190px}

.satir{display:flex;flex-wrap:wrap;gap:.6rem;align-items:center}
button{font:inherit;font-size:.87rem;padding:.45rem .9rem;border-radius:7px;
  border:1px solid var(--line);background:var(--surface-2);color:var(--ink);cursor:pointer}
button:hover{border-color:var(--accent);color:var(--accent)}
button:focus-visible{outline:2px solid var(--accent);outline-offset:2px}
button.tehlike{border-color:var(--kotu);color:var(--kotu);background:var(--kotu-bg);font-weight:650}
button.tehlike:hover{background:var(--kotu);color:#fff}
input[type=number]{font:inherit;font-size:.87rem;width:5.5rem;padding:.4rem .5rem;
  border-radius:7px;border:1px solid var(--line);background:var(--surface);color:var(--ink)}
label.kk{display:flex;align-items:center;gap:.4rem;font-size:.87rem}

pre{margin:0;max-height:16rem;overflow:auto;background:var(--surface-2);
  border-radius:8px;padding:.6rem .7rem;font-family:ui-monospace,Consolas,monospace;
  font-size:.76rem;white-space:pre-wrap;word-break:break-word}
</style></head><body><div class="wrap">

<header>
  <h1>__BASLIK__</h1>
  <span class="pill" id="rozet">baglanti bekleniyor</span>
  <span class="pill" id="satir">0 satir</span>
</header>
<p class="not">__ALTBASLIK__</p>

<div class="tiles" id="tiles"></div>

<div class="panel" id="grafikkutu">
  <h2>Zaman grafigi</h2>
  <div class="legend" id="legend"></div>
  <div id="cwrap"><canvas id="c"></canvas></div>
</div>

<div class="panel" id="komutkutu">
  <h2>Komutlar</h2>
  <div class="satir" id="dugmeler"></div>
  <p class="not" id="komutnot"></p>
</div>

<div class="panel">
  <h2>Ham kutuk</h2>
  <pre id="log">bekleniyor...</pre>
</div>

</div><script>
const ALANLAR = __ALANLAR__;
const DUGMELER = __DUGMELER__;
const RENK = ["#1A6BC7","#B3251B","#1E7B3C","#8A5A00","#6B3FA0","#0F7B7B"];

// ── DOM bir kez kurulur, sonra yalniz metin guncellenir ───────────────────
// Her tikte innerHTML basmak girdi odagini ve secimi oldurur, ayrica
// telefonda gorunur sekilde titretir.
const kutucuk = {};
{
  const kap = document.getElementById("tiles");
  for (const a of ALANLAR) {
    const d = document.createElement("div");
    d.className = "tile";
    const k = document.createElement("div"); k.className = "k"; k.textContent = a.etiket;
    const v = document.createElement("div"); v.className = "v"; v.textContent = "—";
    const s = document.createElement("div"); s.className = "d"; s.textContent = a.alt || a.birim;
    d.append(k, v, s);
    kap.append(d);
    kutucuk[a.ad] = {kutu: d, deger: v, alt: s};
  }
}
{
  const kap = document.getElementById("dugmeler");
  const notlar = [];
  for (const b of DUGMELER) {
    if (b.sayi) {
      const l = document.createElement("label"); l.className = "kk";
      const i = document.createElement("input");
      i.type = "number"; i.value = b.varsayilan;
      if (b.en_az !== null) i.min = b.en_az;
      if (b.en_cok !== null) i.max = b.en_cok;
      const btn = document.createElement("button");
      btn.textContent = b.etiket;
      if (b.tehlike) btn.className = "tehlike";
      btn.onclick = () => gonder(b.komut + (b.ayirac || ":") + (i.value || b.varsayilan));
      l.append(i, btn);
      kap.append(l);
    } else {
      const btn = document.createElement("button");
      btn.textContent = b.etiket;
      if (b.tehlike) btn.className = "tehlike";
      btn.onclick = () => gonder(b.komut);
      kap.append(btn);
    }
    if (b.aciklama) notlar.push(b.etiket + ": " + b.aciklama);
  }
  document.getElementById("komutnot").textContent = notlar.join(" · ");
  if (!DUGMELER.length) document.getElementById("komutkutu").style.display = "none";
}
{
  const grafikli = ALANLAR.filter(a => a.grafik);
  const kap = document.getElementById("legend");
  grafikli.forEach((a, i) => {
    const s = document.createElement("span");
    const ic = document.createElement("i");
    ic.style.borderTopColor = RENK[i % RENK.length];
    s.append(ic, document.createTextNode(a.etiket + (a.birim ? " [" + a.birim + "]" : "")));
    kap.append(s);
  });
  if (!grafikli.length) document.getElementById("grafikkutu").style.display = "none";
}

function gonder(k) {
  fetch("/komut?k=" + encodeURIComponent(k)).catch(() => {});
}

function bicim(a, ham) {
  const v = ham / a.olcek;
  return a.basamak > 0 ? v.toFixed(a.basamak) : String(Math.round(v));
}

// ── Grafik ────────────────────────────────────────────────────────────────
const cv = document.getElementById("c");
const cx = cv.getContext("2d");
function ciz(gecmis) {
  // clientWidth gizli sekmede 0 doner; 0 genislikte cizim canvas'i bozuyor.
  const g = cv.clientWidth, y = cv.clientHeight;
  if (!g || !y) return;
  const o = window.devicePixelRatio || 1;
  if (cv.width !== Math.round(g * o) || cv.height !== Math.round(y * o)) {
    cv.width = Math.round(g * o); cv.height = Math.round(y * o);
  }
  cx.setTransform(o, 0, 0, o, 0, 0);
  cx.clearRect(0, 0, g, y);
  const grafikli = ALANLAR.filter(a => a.grafik);
  if (gecmis.length < 2 || !grafikli.length) return;

  const pad = 4;
  for (let s = 0; s < grafikli.length; s++) {
    const a = grafikli[s];
    let en = Infinity, ust = -Infinity;
    for (const n of gecmis) {
      const v = n[s + 1] / a.olcek;
      if (v < en) en = v;
      if (v > ust) ust = v;
    }
    if (ust - en < 1e-9) { ust = en + 1; en -= 1; }
    const marj = (ust - en) * 0.12;
    en -= marj; ust += marj;
    cx.beginPath();
    cx.strokeStyle = RENK[s % RENK.length];
    cx.lineWidth = 1.7;
    gecmis.forEach((n, i) => {
      const px = pad + (i / (gecmis.length - 1)) * (g - 2 * pad);
      const py = y - pad - ((n[s + 1] / a.olcek - en) / (ust - en)) * (y - 2 * pad);
      i ? cx.lineTo(px, py) : cx.moveTo(px, py);
    });
    cx.stroke();
  }
}

// ── Dongu ─────────────────────────────────────────────────────────────────
let sonLogUz = -1;
let sonGecmis = [];
async function tik() {
  let d;
  try {
    d = await (await fetch("/veri")).json();
  } catch (e) {
    document.getElementById("rozet").textContent = "pano sunucusu kapali";
    document.getElementById("rozet").className = "pill olu";
    return;
  }
  const r = document.getElementById("rozet");
  r.textContent = d.baglanti;
  r.className = "pill " + (d.taze ? "canli" : "olu");
  document.getElementById("satir").textContent = d.satir + " satir";

  for (const a of ALANLAR) {
    const t = kutucuk[a.ad];
    const ham = d.son[a.ad];
    t.deger.textContent = (ham === undefined || !d.taze && !d.satir) ? "—" : bicim(a, ham);
    if (a.iyi_kural) {
      const iy = a.iyi_kural(ham);
      t.kutu.className = "tile" + (iy === true ? " iyi" : iy === false ? " kotu" : "");
    }
  }
  sonGecmis = d.gecmis;
  ciz(d.gecmis);

  // Kutugu yalnizca DEGISTIYSE bas — her tik yeniden basmak kaydirmayi
  // sifirliyor ve okumayi imkansiz kiliyor.
  if (d.log.length !== sonLogUz) {
    sonLogUz = d.log.length;
    document.getElementById("log").textContent =
      d.log.map(x => x.z + "  " + x.s).join("\n") || "bekleniyor...";
  }
}
__IYI_KURALLAR__
setInterval(tik, 100);
tik();
// Yeniden boyutlandirmada son veriyle CIZ, bos diziyle degil — yoksa grafik
// bir sonraki tike kadar bembeyaz kaliyor.
window.addEventListener("resize", () => ciz(sonGecmis));
</script></body></html>
"""


# ─────────────────────────────────────────────────────────────────────────────
# Pano
# ─────────────────────────────────────────────────────────────────────────────

class Pano:
    def __init__(self, baslik, alanlar, dugmeler=(), altbaslik="",
                 varsayilan_port="COM7", baud=115200, http=8770,
                 demo_uretec=None, donustur=None):
        self.donustur = donustur
        self.baslik = baslik
        self.alanlar = list(alanlar)
        self.dugmeler = list(dugmeler)
        self.altbaslik = altbaslik
        self.varsayilan_port = varsayilan_port
        self.baud = baud
        self.http = http
        self.demo_uretec = demo_uretec

    def _sayfa(self):
        # `iyi` kurallari Python tarafinda lambda; tarayiciya JS olarak gecer.
        kurallar = []
        for a in self.alanlar:
            if a.iyi:
                kurallar.append(
                    "ALANLAR.find(a=>a.ad===%s).iyi_kural = (v)=>(%s);"
                    % (json.dumps(a.ad), a.iyi))
        return (SAYFA
                .replace("__BASLIK__", self.baslik)
                .replace("__ALTBASLIK__", self.altbaslik)
                .replace("__ALANLAR__", json.dumps([a.sozluk() for a in self.alanlar]))
                .replace("__DUGMELER__", json.dumps([b.sozluk() for b in self.dugmeler]))
                .replace("__IYI_KURALLAR__", "\n".join(kurallar)))

    def calistir(self, ek_argumanlar=None):
        p = argparse.ArgumentParser(description=self.baslik)
        p.add_argument("--port", default=self.varsayilan_port,
                       help="seri port (Windows: COM15, Linux: /dev/ttyACM0)")
        p.add_argument("--baud", type=int, default=self.baud)
        p.add_argument("--http", type=int, default=self.http)
        p.add_argument("--demo", action="store_true",
                       help="kart yokken arayuzu dene (veri UYDURMADIR)")
        if ek_argumanlar:
            ek_argumanlar(p)
        a = p.parse_args()

        durum = Durum(self.alanlar, donustur=self.donustur)
        if a.demo:
            if not self.demo_uretec:
                raise SystemExit("bu panoda demo ureteci tanimli degil")
            kopru = SahteKopru(durum, self.demo_uretec)
        else:
            kopru = Kopru(durum, a.port, a.baud)
        kopru.basla()

        gecerli = {b.komut for b in self.dugmeler if not b.yerel}
        yerel = {b.komut: b for b in self.dugmeler if b.yerel}
        sunucu = _sunucu_yap(durum, kopru, self._sayfa(), a.http, gecerli, yerel)

        print("  " + self.baslik)
        print("  http://localhost:%d      (telefondan: http://<PC-IP>:%d)" % (a.http, a.http))
        print("  port: %s @ %d%s" % (a.port, a.baud, "   [DEMO]" if a.demo else ""))
        print("  ⚠ karta yukleme yapmadan once bu panoyu KAPAT (portu tutuyor)")
        try:
            sunucu.serve_forever()
        except KeyboardInterrupt:
            print("\n  kapaniyor")
        finally:
            kopru.dur.set()
