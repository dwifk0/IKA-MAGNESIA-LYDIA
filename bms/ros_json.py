#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
# Copyright (C) 2026 Ahmet Efe Nezli
"""
ROS panosunun SSE akışını düz JSON'a çevirir ve CORS ile sunar.

Neden gerekli — iki ayrı engel, ikisi de panoyu susturuyordu:

  1. **CORS.** Statik pano `:8080`'den servis ediliyor, ROS panosu `:8083`'te.
     Farklı köken. `:8083/telemetri` `Access-Control-Allow-Origin` göndermezse
     tarayıcı hem `fetch` hem `EventSource` isteğini sessizce düşürür ve
     panoda "BAĞLANTI YOK" yazar — sunucu gayet ayakta olduğu hâlde.
  2. **Biçim.** `:8083/telemetri` bir SSE akışı (`data: {...}` satırları),
     düz JSON değil. Panonun bütün kaynak mimarisi `fetch().json()` üstüne
     kurulu; tek uç için ayrı bir mekanizma taşımak gereksiz.

Bu köprü ikisini birden çözüyor ve **bizim tarafımızda** duruyor: yazılım
ekibinin sunucusuna dokunmadan, onların bir sonraki güncellemesinde
kaybolmayacak biçimde.

    python3 ros_json.py --kaynak http://127.0.0.1:8083/telemetri --port 8094

Uç:  GET /telemetri  →  son çerçeve + "yas" (kaç saniyedir taze değil)
"""

import argparse
import json
import threading
import time
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

SON = {"bagli": False, "yas": None, "hata": "henüz veri gelmedi"}
KILIT = threading.Lock()

# Bu kadar saniyedir çerçeve gelmiyorsa akış ölü sayılır.
BAYAT_S = 5.0


class Ucu(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.rstrip("/") not in ("/telemetri", ""):
            self.send_error(404)
            return
        with KILIT:
            g = dict(SON)
            t = g.pop("_t", None)
        if t is not None:
            yas = time.time() - t
            g["yas"] = round(yas, 2)
            g["bagli"] = yas < BAYAT_S
        govde = json.dumps(g, ensure_ascii=False).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(govde)))
        self.end_headers()
        self.wfile.write(govde)

    def log_message(self, *a):
        pass


def akis(kaynak):
    """SSE'yi satır satır oku. Kopunca geri gel — sahada Jetson yeniden
    başlatılınca panonun elle yenilenmesi gerekmesin."""
    bekle = 2
    while True:
        try:
            with urllib.request.urlopen(kaynak, timeout=10) as r:
                bekle = 2
                for ham in r:
                    satir = ham.decode("utf-8", "replace").strip()
                    if not satir.startswith("data:"):
                        continue
                    try:
                        d = json.loads(satir[5:].strip())
                    except ValueError:
                        continue
                    with KILIT:
                        # ⚠ `sistem` ayrı iş parçacığından geliyor; clear()
                        # onu da silerdi ve pano her çerçevede bir yanıp
                        # sönerdi. Saklanıp geri konuyor.
                        sis = SON.get("sistem")
                        SON.clear()
                        SON.update(d)
                        if sis is not None:
                            SON["sistem"] = sis
                        SON["_t"] = time.time()
                        SON["hata"] = None
        except Exception as e:                                   # noqa: BLE001
            with KILIT:
                SON["hata"] = "%s: %s" % (type(e).__name__, e)
            time.sleep(bekle)
            bekle = min(bekle * 2, 30)


# ─────────────────────────────────────────────────────────────────────────────
# GERÇEK SİSTEM BİLGİSİ
#
# Panoda disk/RAM/çalışma süresi/süreç listesi ELLE YAZILMIŞ sabit tablodan
# geliyordu: "disk 28,8 GB" (gerçekte 456 GB), "yolo_detection_node %42"
# (o düğüm koşmuyor bile). Köprü zaten Jetson'da çalıştığı için bunları
# gerçekten okumak bedava — dışarıdan kütüphane de gerekmiyor, /proc yeter.
# ─────────────────────────────────────────────────────────────────────────────

import os

SAAT = os.sysconf("SC_CLK_TCK")
_onceki_cpu = {}          # pid → (utime+stime, an)


def _pid_bilgi(pid):
    try:
        with open("/proc/%s/stat" % pid) as f:
            alan = f.read().rsplit(") ", 1)[1].split()
        tik = int(alan[11]) + int(alan[12])          # utime + stime
        with open("/proc/%s/status" % pid) as f:
            rss = 0
            for satir in f:
                if satir.startswith("VmRSS:"):
                    rss = int(satir.split()[1]) // 1024
                    break
        with open("/proc/%s/cmdline" % pid, "rb") as f:
            komut = f.read().replace(b"\0", b" ").decode("utf-8", "replace").strip()
    except (OSError, IndexError, ValueError):
        return None
    return tik, rss, komut


def surecler(en_cok=8):
    """En çok CPU yiyen süreçler. CPU%% iki örnek arasındaki farktan."""
    simdi = time.time()
    liste = []
    for pid in os.listdir("/proc"):
        if not pid.isdigit():
            continue
        b = _pid_bilgi(pid)
        if not b:
            continue
        tik, rss, komut = b
        onceki = _onceki_cpu.get(pid)
        _onceki_cpu[pid] = (tik, simdi)
        if not onceki or simdi <= onceki[1]:
            continue
        cpu = 100.0 * (tik - onceki[0]) / SAAT / (simdi - onceki[1])
        if not komut:
            continue
        ad = komut.split()[0].rsplit("/", 1)[-1]
        # ros2 run <paket> <dugum> → düğüm adını göster, "python3" değil
        parca = komut.split()
        if "ros2" in ad or ad.startswith("python"):
            for i, x in enumerate(parca):
                if x.endswith("/teknofest_ika") or x == "teknofest_ika":
                    if i + 1 < len(parca):
                        ad = parca[i + 1]
                    break
            else:
                if len(parca) > 1:
                    ad = parca[-1].rsplit("/", 1)[-1] or ad
        liste.append({"pid": int(pid), "ad": ad[:40], "cpu": round(cpu, 1),
                      "ram": rss, "komut": komut[:160]})
    liste.sort(key=lambda x: x["cpu"], reverse=True)
    return liste[:en_cok]


def sistem():
    """Disk, RAM, yük, ağ, çalışma süresi — hepsi ölçülmüş."""
    d = {}
    try:
        with open("/proc/uptime") as f:
            d["ayakta_s"] = int(float(f.read().split()[0]))
    except OSError:
        pass
    try:
        with open("/proc/loadavg") as f:
            a = f.read().split()
            d["yuk"] = [float(a[0]), float(a[1]), float(a[2])]
    except (OSError, ValueError):
        pass
    try:
        st = os.statvfs("/")
        d["disk_gb"] = round(st.f_blocks * st.f_frsize / 1e9, 1)
        d["disk_dolu_gb"] = round((st.f_blocks - st.f_bfree) * st.f_frsize / 1e9, 1)
    except OSError:
        pass
    try:
        with open("/proc/meminfo") as f:
            m = {}
            for satir in f:
                k, _, v = satir.partition(":")
                m[k] = int(v.split()[0])
        d["ram_mb"] = round(m["MemTotal"] / 1024)
        d["ram_dolu_mb"] = round((m["MemTotal"] - m["MemAvailable"]) / 1024)
        if "SwapTotal" in m:
            d["takas_mb"] = round(m["SwapTotal"] / 1024)
            d["takas_dolu_mb"] = round((m["SwapTotal"] - m.get("SwapFree", 0)) / 1024)
    except (OSError, KeyError, ValueError):
        pass
    d["surecler"] = surecler()
    return d


def sistem_dongu(aralik=2.0):
    while True:
        try:
            s = sistem()
            with KILIT:
                SON["sistem"] = s
        except Exception:                                        # noqa: BLE001
            pass
        time.sleep(aralik)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--kaynak", default="http://127.0.0.1:8083/telemetri")
    ap.add_argument("--port", type=int, default=8094)
    n = ap.parse_args()
    threading.Thread(target=akis, args=(n.kaynak,), daemon=True).start()
    threading.Thread(target=sistem_dongu, daemon=True).start()
    s = ThreadingHTTPServer(("0.0.0.0", n.port), Ucu)
    print("ROS köprüsü: http://0.0.0.0:%d/telemetri   ← %s" % (n.port, n.kaynak),
          flush=True)
    s.serve_forever()


if __name__ == "__main__":
    main()
