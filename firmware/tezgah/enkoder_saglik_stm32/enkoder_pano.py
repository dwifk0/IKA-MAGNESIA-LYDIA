#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
# Copyright (C) 2026 Ahmet Efe Nezli
"""
Enkoder saglik testi icin canli pano.

Windows tarafinda calisir (pyserial + stdlib). Seri porttan
"D sayim=.. hatali=.. sonhata=.. hiz=.. A=.. B=.. t=.." satirlarini
okur, tarayiciya canli basar; sayfadaki dugmelerden karta komut yollar.

Ornek:
    python.exe enkoder_pano.py --port COM7
    -> http://localhost:8770  (telefondan: http://<PC-IP>:8770)
"""

import argparse
import json
import re
import threading
import time
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

try:
    import serial
except ImportError:
    raise SystemExit("pyserial yok:  python.exe -m pip install pyserial")

SATIR = re.compile(
    r"D\s+sayim=(-?\d+)\s+hatali=(\d+)\s+sonhata=(\d+)\s+"
    r"hiz10=(-?\d+)\s+A=([01])\s+B=([01])\s+t=(\d+)"
)


class Durum:
    def __init__(self, pencere=600):
        self.kilit = threading.Lock()
        self.son = {"sayim": 0, "hatali": 0, "sonhata": 0,
                    "hiz": 0.0, "A": 0, "B": 0, "t": 0}
        self.gecmis = deque(maxlen=pencere)   # [t, sayim, hiz, hatali]
        self.log = deque(maxlen=300)
        self.baglanti = "bekliyor"
        self.goruldu = 0.0

    def veri_geldi(self, m):
        with self.kilit:
            self.son = {
                "sayim": int(m.group(1)), "hatali": int(m.group(2)),
                "sonhata": int(m.group(3)), "hiz": int(m.group(4)) / 10.0,
                "A": int(m.group(5)), "B": int(m.group(6)),
                "t": int(m.group(7)),
            }
            self.gecmis.append([self.son["t"], self.son["sayim"],
                                self.son["hiz"], self.son["hatali"]])
            self.goruldu = time.time()

    def log_ekle(self, satir):
        with self.kilit:
            self.log.append({"z": time.strftime("%H:%M:%S"), "s": satir})

    def anlik(self):
        with self.kilit:
            taze = (time.time() - self.goruldu) < 2.0 if self.goruldu else False
            return {
                "son": dict(self.son),
                "gecmis": list(self.gecmis),
                "log": list(self.log),
                "baglanti": self.baglanti,
                "taze": taze,
            }


class Kopru:
    """Seri portu acik tutar, koparsa yeniden baglanir."""

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
                            self._satir(ham.decode("ascii", "replace").strip())
            except Exception as e:
                self.durum.baglanti = "KOPUK: " + str(e)
                time.sleep(2)
            finally:
                # Kart resetlenince port kopar ve buraya dusulur. sp
                # kapatilmazsa Windows tutamagi birakmaz ve yeniden
                # baglanma "erisim reddedildi" ile sonsuza kadar takilir.
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
        m = SATIR.match(s)
        if m:
            self.durum.veri_geldi(m)
        else:
            self.durum.log_ekle(s)


class SahteKopru:
    """--demo: kart yokken panoyu denemek icin uydurma veri uretir.

    Elle cevirmeyi taklit eder: hiz sinuzoidal, arada bir hatali gecis.
    HICBIR SEKILDE gercek olcum degildir, sadece arayuz denemesi icindir.
    """

    def __init__(self, durum):
        self.durum = durum
        self.sayim = 0
        self.hatali = 0
        self.dur = threading.Event()

    def basla(self):
        threading.Thread(target=self._dongu, daemon=True).start()

    def komut(self, k):
        if k == "s":
            self.sayim = 0
            self.hatali = 0
        self.durum.log_ekle("[demo] komut: " + k)
        return True

    def _dongu(self):
        import math
        import random
        self.durum.baglanti = "DEMO - gercek veri degil"
        self.durum.log_ekle("=== DEMO MODU: veriler uydurma ===")
        t0 = time.time()
        while not self.dur.is_set():
            gecen = time.time() - t0
            hiz = 900 * math.sin(gecen / 4.0) + random.uniform(-40, 40)
            self.sayim += int(hiz * 0.1)
            # hizlandikca artan darbe kacirma taklidi
            if abs(hiz) > 700 and random.random() < 0.04:
                self.hatali += 1
            with self.durum.kilit:
                self.durum.son = {
                    "sayim": self.sayim, "hatali": self.hatali,
                    "sonhata": 0, "hiz": round(hiz, 1),
                    "A": random.randint(0, 1), "B": random.randint(0, 1),
                    "t": int(gecen * 1000),
                }
                self.durum.gecmis.append([self.durum.son["t"], self.sayim,
                                          round(hiz, 1), self.hatali])
                self.durum.goruldu = time.time()
            time.sleep(0.1)


def sunucu_yap(durum, kopru, sayfa, portno):
    class Islem(BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def _bas(self, kod, tur, govde):
            self.send_response(kod)
            self.send_header("Content-Type", tur)
            self.send_header("Content-Length", str(len(govde)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(govde)

        def do_GET(self):
            u = urlparse(self.path)
            if u.path == "/":
                self._bas(200, "text/html; charset=utf-8", sayfa.encode())
            elif u.path == "/veri":
                self._bas(200, "application/json",
                          json.dumps(durum.anlik()).encode())
            elif u.path == "/komut":
                k = (parse_qs(u.query).get("k") or [""])[0]
                ok = kopru.komut(k) if k in ("s", "d", "p") else False
                self._bas(200, "application/json",
                          json.dumps({"ok": ok}).encode())
            else:
                self._bas(404, "text/plain", b"yok")

    return ThreadingHTTPServer(("0.0.0.0", portno), Islem)


SAYFA = r"""<!doctype html>
<html lang="tr"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Enkoder Panosu</title>
<style>
:root{
  --paper:#F4F6F5; --surface:#FFFFFF; --surface-2:#EAEEEC;
  --ink:#131A17; --ink-2:#4C5854; --ink-3:#727E79;
  --line:#D3DAD6;
  --accent:#1A6BC7;
  --iyi:#1E7B3C; --iyi-bg:#E3F2E7;
  --kotu:#B3251B; --kotu-bg:#FBE6E3;
}
@media (prefers-color-scheme:dark){:root{
  --paper:#0B100F; --surface:#141B19; --surface-2:#1C2422;
  --ink:#E3E9E5; --ink-2:#9DA9A4; --ink-3:#7C8883;
  --line:#2A3431;
  --accent:#3B86D8;
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

.tiles{display:grid;gap:.7rem;grid-template-columns:repeat(auto-fit,minmax(11rem,1fr))}
.tile{background:var(--surface);border:1px solid var(--line);border-radius:10px;
  padding:.8rem .9rem;display:flex;flex-direction:column;gap:.15rem}
.tile .k{font-family:ui-monospace,Consolas,monospace;font-size:.66rem;
  letter-spacing:.1em;text-transform:uppercase;color:var(--ink-3)}
.tile .v{font-size:2rem;font-weight:650;letter-spacing:-.03em;
  font-variant-numeric:tabular-nums;line-height:1.1}
.tile .d{font-size:.78rem;color:var(--ink-2)}
.tile.durum{border-width:2px}
.tile.durum.iyi{border-color:var(--iyi);background:var(--iyi-bg)}
.tile.durum.kotu{border-color:var(--kotu);background:var(--kotu-bg)}
.tile.durum .v{display:flex;align-items:center;gap:.5rem}
.ikon{width:1.5rem;height:1.5rem;flex:none}

.panel{background:var(--surface);border:1px solid var(--line);border-radius:10px;padding:.9rem 1rem;
  display:flex;flex-direction:column;gap:.6rem}
.panel h2{margin:0;font-size:.95rem;font-weight:650}
.panel .alt{font-size:.78rem;color:var(--ink-2);margin:0}
.legend{display:flex;gap:1rem;flex-wrap:wrap;font-size:.75rem;color:var(--ink-2)}
.legend i{display:inline-block;width:.85rem;height:0;border-top:2px solid var(--accent);
  vertical-align:.25em;margin-right:.35rem}
.legend i.tick{border:none;border-left:2px solid var(--kotu);height:.8rem;width:0;vertical-align:-.1em}
#cwrap{position:relative}
canvas{display:block;width:100%;height:170px}
#tip{position:absolute;pointer-events:none;background:var(--surface-2);
  border:1px solid var(--line);border-radius:6px;padding:.3rem .5rem;
  font-family:ui-monospace,Consolas,monospace;font-size:.72rem;white-space:nowrap;
  opacity:0;transition:opacity .1s}

.satir{display:flex;flex-wrap:wrap;gap:.6rem;align-items:center}
button{font:inherit;font-size:.87rem;padding:.45rem .9rem;border-radius:7px;
  border:1px solid var(--line);background:var(--surface-2);color:var(--ink);cursor:pointer}
button:hover{border-color:var(--accent);color:var(--accent)}
button:focus-visible{outline:2px solid var(--accent);outline-offset:2px}
input[type=number]{font:inherit;font-size:.87rem;width:5rem;padding:.4rem .5rem;
  border-radius:7px;border:1px solid var(--line);background:var(--surface);color:var(--ink)}
.chip{font-family:ui-monospace,Consolas,monospace;font-size:.8rem;
  border:1px solid var(--line);border-radius:6px;padding:.3rem .6rem;background:var(--surface-2)}
.chip b{font-weight:700}

table{border-collapse:collapse;width:100%;font-size:.8rem}
th,td{text-align:left;padding:.3rem .5rem;border-bottom:1px solid var(--line)}
th{font-family:ui-monospace,Consolas,monospace;font-size:.66rem;letter-spacing:.08em;
  text-transform:uppercase;color:var(--ink-3);font-weight:500}
td.n{font-variant-numeric:tabular-nums;font-family:ui-monospace,Consolas,monospace}
#logwrap{max-height:13rem;overflow:auto;border:1px solid var(--line);border-radius:8px;background:var(--surface)}
#log{font-family:ui-monospace,Consolas,monospace;font-size:.76rem;margin:0;padding:.6rem .8rem;
  white-space:pre-wrap;color:var(--ink-2)}
</style></head><body>
<div class="wrap">

<header>
  <h1>Enkoder sağlık testi</h1>
  <span class="pill" id="baglanti">bekliyor</span>
  <span class="pill" id="tazelik">veri yok</span>
</header>

<div class="tiles">
  <div class="tile durum" id="hataliTile">
    <span class="k">Hatalı geçiş</span>
    <span class="v"><span class="ikon" id="hataliIkon"></span><span id="hatali">0</span></span>
    <span class="d" id="hataliMetin">veri bekleniyor</span>
  </div>
  <div class="tile">
    <span class="k">Sayım</span>
    <span class="v" id="sayim">0</span>
    <span class="d" id="turMetin">0.000 tur</span>
  </div>
  <div class="tile">
    <span class="k">Hız</span>
    <span class="v" id="hiz">0</span>
    <span class="d">sayım / saniye</span>
  </div>
  <div class="tile">
    <span class="k">Ham seviye</span>
    <span class="v" id="ab">– –</span>
    <span class="d">A ve B ayrı ayrı değişmeli</span>
  </div>
</div>

<div class="panel">
  <h2>Çevirme hızı</h2>
  <p class="alt">Son 60 saniye. Kırmızı çizgiler hatalı geçişin olduğu anı gösterir — hangi hızda kaçırdığı buradan okunur.</p>
  <div class="legend">
    <span><i></i>hız (sayım/sn)</span>
    <span><i class="tick"></i>hatalı geçiş</span>
  </div>
  <div id="cwrap"><canvas id="c"></canvas><div id="tip"></div></div>
</div>

<div class="panel">
  <h2>Tur hesabı</h2>
  <p class="alt">Şafta işaret koy, <b>Sıfırla</b>ya bas, tam tur çevir, kaç tur çevirdiğini gir.</p>
  <div class="satir">
    <label>Çevrilen tur <input type="number" id="turSayisi" value="1" min="1" step="1"></label>
    <span class="chip">tur başına <b id="turBasi">–</b> sayım</span>
    <span class="chip">gerçek <b id="ppr">–</b> P/R</span>
  </div>
  <p class="alt" id="pprNot">300 P/R ise tur başına 1200 sayım beklenir.</p>
</div>

<div class="panel">
  <h2>Komutlar</h2>
  <div class="satir">
    <button onclick="komut('s')">Sıfırla</button>
    <button onclick="komut('d')">Durum yaz</button>
    <button onclick="komut('p')">Ham seviye (5 sn)</button>
  </div>
</div>

<div class="panel">
  <h2>Son örnekler</h2>
  <table><thead><tr><th>t (ms)</th><th>Sayım</th><th>Hız</th><th>Hatalı</th></tr></thead>
  <tbody id="tablo"></tbody></table>
</div>

<div class="panel">
  <h2>Karttan gelen metin</h2>
  <div id="logwrap"><pre id="log"></pre></div>
</div>

</div>
<script>
const $ = (i) => document.getElementById(i);
const IKON_IYI  = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" width="24" height="24"><path d="M20 6 9 17l-5-5"/></svg>';
const IKON_KOTU = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" width="24" height="24"><path d="M18 6 6 18M6 6l12 12"/></svg>';

let gecmis = [], son = null;

function komut(k){ fetch('/komut?k=' + k); }

function boya(){
  const c = $('c'), dpr = window.devicePixelRatio || 1;
  const w = c.clientWidth, h = c.clientHeight;
  c.width = w * dpr; c.height = h * dpr;
  const x = c.getContext('2d'); x.scale(dpr, dpr);
  const cs = getComputedStyle(document.body);
  const renkLine = cs.getPropertyValue('--line').trim();
  const renkAcc  = cs.getPropertyValue('--accent').trim();
  const renkKotu = cs.getPropertyValue('--kotu').trim();
  const renkInk3 = cs.getPropertyValue('--ink-3').trim();

  x.clearRect(0, 0, w, h);
  const pad = {t: 10, r: 8, b: 18, l: 42};
  const pw = w - pad.l - pad.r, ph = h - pad.t - pad.b;
  if (gecmis.length < 2) return;

  let enb = 100;
  for (const g of gecmis) enb = Math.max(enb, Math.abs(g[2]));
  enb = Math.ceil(enb / 100) * 100;

  // izgara — geri planda kalsin
  x.strokeStyle = renkLine; x.lineWidth = 1; x.font = '10px ui-monospace, Consolas, monospace';
  x.fillStyle = renkInk3; x.textAlign = 'right'; x.textBaseline = 'middle';
  for (let i = 0; i <= 2; i++){
    const v = enb - enb * i, yy = pad.t + (ph * i / 2);
    x.beginPath(); x.moveTo(pad.l, yy); x.lineTo(pad.l + pw, yy); x.stroke();
    x.fillText(String(v), pad.l - 6, yy);
  }
  const y0 = pad.t + ph / 2;

  const t1 = gecmis[gecmis.length - 1][0], t0 = t1 - 60000;
  const px = (t) => pad.l + pw * Math.max(0, Math.min(1, (t - t0) / 60000));
  const py = (v) => y0 - (v / enb) * (ph / 2);

  // hatali gecis tikleri — cizginin ALTINDA kalsin
  x.strokeStyle = renkKotu; x.lineWidth = 2;
  for (let i = 1; i < gecmis.length; i++){
    if (gecmis[i][3] > gecmis[i-1][3]){
      const xx = px(gecmis[i][0]);
      x.beginPath(); x.moveTo(xx, pad.t); x.lineTo(xx, pad.t + ph); x.stroke();
    }
  }

  // hiz cizgisi
  x.strokeStyle = renkAcc; x.lineWidth = 2; x.lineJoin = 'round'; x.beginPath();
  let ilk = true;
  for (const g of gecmis){
    if (g[0] < t0) continue;
    const xx = px(g[0]), yy = py(g[2]);
    if (ilk){ x.moveTo(xx, yy); ilk = false; } else x.lineTo(xx, yy);
  }
  x.stroke();

  // vurgulanmis son nokta
  const sg = gecmis[gecmis.length - 1];
  x.fillStyle = renkAcc; x.beginPath();
  x.arc(px(sg[0]), py(sg[2]), 4, 0, Math.PI * 2); x.fill();
}

function turHesabi(){
  if (!son) return;
  const turSay = Math.max(1, parseInt($('turSayisi').value) || 1);
  const turBasi = Math.abs(son.sayim) / turSay;
  $('turMetin').textContent = (son.sayim / 1200).toFixed(3) + ' tur (300 P/R varsayımı)';
  $('turBasi').textContent = son.sayim ? turBasi.toFixed(1) : '–';
  $('ppr').textContent = son.sayim ? (turBasi / 4).toFixed(1) : '–';
}

function tazele(d){
  son = d.son; gecmis = d.gecmis;
  $('baglanti').textContent = d.baglanti;
  const canli = d.taze;
  $('tazelik').textContent = canli ? 'canlı' : 'veri yok';
  $('tazelik').className = 'pill ' + (canli ? 'canli' : 'olu');

  $('sayim').textContent = son.sayim;
  $('hiz').textContent = Math.round(son.hiz);
  $('ab').textContent = canli ? ('A=' + son.A + '  B=' + son.B) : '– –';

  const hatali = son.hatali;
  $('hatali').textContent = hatali;
  const t = $('hataliTile');
  t.className = 'tile durum ' + (canli ? (hatali === 0 ? 'iyi' : 'kotu') : '');
  $('hataliIkon').innerHTML = canli ? (hatali === 0 ? IKON_IYI : IKON_KOTU) : '';
  $('hataliMetin').textContent = !canli ? 'veri bekleniyor'
    : (hatali === 0 ? 'TEMİZ — darbe kaçırmıyor' : 'DARBE KAÇIRIYOR');

  turHesabi();

  const tb = $('tablo'); tb.innerHTML = '';
  for (const g of gecmis.slice(-8).reverse()){
    const tr = document.createElement('tr');
    tr.innerHTML = '<td class="n">' + g[0] + '</td><td class="n">' + g[1] +
                   '</td><td class="n">' + g[2].toFixed(1) + '</td><td class="n">' + g[3] + '</td>';
    tb.appendChild(tr);
  }

  const lg = $('log');
  const metin = d.log.map(r => r.z + '  ' + r.s).join('\n');
  if (lg.textContent !== metin){
    const dip = lg.parentElement.scrollTop + lg.parentElement.clientHeight >= lg.parentElement.scrollHeight - 20;
    lg.textContent = metin;
    if (dip) lg.parentElement.scrollTop = lg.parentElement.scrollHeight;
  }
  boya();
}

// hover okumasi
const cv = $('c'), tip = $('tip');
cv.addEventListener('mousemove', (e) => {
  if (!gecmis.length) return;
  const r = cv.getBoundingClientRect();
  const pad = {l: 42, r: 8};
  const pw = r.width - pad.l - pad.r;
  const t1 = gecmis[gecmis.length - 1][0], t0 = t1 - 60000;
  const t = t0 + 60000 * Math.max(0, Math.min(1, (e.clientX - r.left - pad.l) / pw));
  let en = gecmis[0];
  for (const g of gecmis) if (Math.abs(g[0] - t) < Math.abs(en[0] - t)) en = g;
  tip.textContent = 'hız ' + en[2].toFixed(1) + ' · sayım ' + en[1] + ' · hatalı ' + en[3];
  tip.style.left = Math.min(r.width - tip.offsetWidth - 4, Math.max(0, e.clientX - r.left + 10)) + 'px';
  tip.style.top = '6px';
  tip.style.opacity = '1';
});
cv.addEventListener('mouseleave', () => { tip.style.opacity = '0'; });
window.addEventListener('resize', boya);
$('turSayisi').addEventListener('input', turHesabi);

async function tik(){
  try {
    const r = await fetch('/veri');
    tazele(await r.json());
  } catch (e) {
    $('tazelik').textContent = 'pano sunucusu kapalı';
    $('tazelik').className = 'pill olu';
  }
}
setInterval(tik, 200); tik();
</script></body></html>
"""


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--port", default="COM7")
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--http", type=int, default=8770)
    p.add_argument("--demo", action="store_true",
                   help="kart yokken uydurma veriyle panoyu dene")
    a = p.parse_args()

    durum = Durum()
    kopru = SahteKopru(durum) if a.demo else Kopru(durum, a.port, a.baud)
    kopru.basla()

    srv = sunucu_yap(durum, kopru, SAYFA, a.http)
    print("Pano:  http://localhost:%d" % a.http)
    print("Seri:  %s" % ("DEMO (uydurma veri)" if a.demo else
                         "%s @ %d" % (a.port, a.baud)))
    print("Durdurmak icin Ctrl+C")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        print("\nkapatiliyor")


if __name__ == "__main__":
    main()
