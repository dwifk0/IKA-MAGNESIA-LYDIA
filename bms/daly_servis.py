#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (C) 2026 Ahmet Efe Nezli
"""
DALY (elektronik bataryası) → HTTP JSON uç. `jk_servis.py`'nin kardeşi.

Neden ayrı süreç: `jk_servis` ana bataryayı okuyor ve o BMS her zaman açık
değil (ana güç kesikken hiç görünmüyor). İkisini tek sürece koymak, biri
yokken ötekinin de yeniden bağlanma döngüsünde beklemesi demekti.

🔴 Protokol ve taşıyıcı `bms_oku.py`'den ALINIYOR, kopyalanmıyor. DALY çerçeve
çözümleyicisi tek yerde kalsın: iki kopya arasında ölçek katsayısı ayrışırsa
sahada hangi sayının doğru olduğu anlaşılmaz.

Kullanım:
    python3 daly_servis.py --mac AA:BB:CC:DD:EE:FF --port 8093

Uç:  GET /bms  →  {"bagli":true,"yas":1.2,"gerilim_V":14.0,"soc_yuzde":87,...}

⚠ BLE'de bu BMS aynı anda TEK istemci kabul eder. Bu servis çalışırken
telefondaki DALY uygulaması bağlanamaz; uygulamayı kullanacaksan servisi
durdur.
"""

import argparse
import asyncio
import datetime
import json
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import bms_oku as B                                            # noqa: E402

# Veri bu kadar saniyedir gelmiyorsa bağlantı ölü sayılır. `gozcu` yeniden
# bağlanmayı kendi yönetiyor; buradaki eşik yalnız panoya "taze mi" demek için.
BAYAT_S = 15.0

DURUM = {"bagli": False, "yas": None, "hata": None, "cerceve": 0}
KILIT = threading.Lock()


class Ucu(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.rstrip("/") not in ("/bms", ""):
            self.send_error(404)
            return
        with KILIT:
            g = dict(DURUM)
        govde = json.dumps(g, ensure_ascii=False).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        # Pano başka porttan çekiyor (statik pano :8080), CORS şart.
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(govde)))
        self.end_headers()
        self.wfile.write(govde)

    def log_message(self, *a):
        pass


class BleTasiyiciTaramali(B.BleTasiyici):
    """
    `BleTasiyici` + bağlanmadan önce ZORUNLU tarama.

    🔴 BlueZ'de adrese DOĞRUDAN bağlanmak, cihaz önbellekte yoksa
    `BleakDeviceNotFoundError` veriyor. `bms_oku.py --daly-ble` sahada
    çalışmıştı çünkü hemen öncesinde `bms_tara.py` taraması önbelleği
    doldurmuştu; tek başına başlatılan bir serviste o tarama yok ve
    bağlantı hiç kurulmuyor. Aynı ders `jk_servis.py`'de de yazılı.

    Tarama cihazı bulamazsa ham adresle devam ediliyor: BMS'ler bir istemci
    bağlıyken reklam yayınlamayı bırakıyor, yani "taramada yok" her zaman
    "cihaz yok" demek değil — bayat bir BlueZ bağlantısı da olabilir ve o
    durumda ham adres yolu çalışır.
    """

    async def baglan(self):
        from bleak import BleakScanner
        hedef = await BleakScanner.find_device_by_address(self.adres, timeout=12)
        if hedef is not None:
            self.adres = hedef        # BleakClient BLEDevice nesnesini kabul ediyor
        await super().baglan()


class SessizKayit:
    """CSV tutmuyoruz — kayıt `bms_oku.py`'nin işi, burası yalnız canlı uç."""

    def yaz(self, satir):
        pass


async def yansit(durum, aralik=0.5):
    """`DalyDurum`u HTTP sözlüğüne kopyalar. Tek yazar burası."""
    while True:
        simdi = datetime.datetime.now()
        with KILIT:
            DURUM.update(durum.d)
            DURUM["cerceve"] = durum.cerceve
            if durum.son is None:
                DURUM["bagli"] = False
                DURUM["yas"] = None
                DURUM["hata"] = "henüz veri gelmedi"
            else:
                yas = (simdi - durum.son).total_seconds()
                DURUM["yas"] = round(yas, 1)
                DURUM["bagli"] = yas < BAYAT_S
                DURUM["hata"] = None if yas < BAYAT_S else (
                    "veri %.0f sn'dir gelmiyor" % yas)
        await asyncio.sleep(aralik)


async def calis(mac, aralik):
    durum = B.DalyDurum()
    kayit = SessizKayit()
    gorevler = [
        B.gozcu("DALY · BLE", lambda: BleTasiyiciTaramali(mac, B.DALY_BLE_ADAYLAR),
                B.daly_dongu, durum, kayit, aralik),
        yansit(durum),
    ]
    await asyncio.gather(*gorevler)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mac", required=True)
    ap.add_argument("--port", type=int, default=8093)
    ap.add_argument("--aralik", type=float, default=2.0)
    n = ap.parse_args()
    s = ThreadingHTTPServer(("0.0.0.0", n.port), Ucu)
    threading.Thread(target=s.serve_forever, daemon=True).start()
    print("DALY servisi: http://0.0.0.0:%d/bms   mac=%s" % (n.port, n.mac),
          flush=True)
    asyncio.run(calis(n.mac, n.aralik))


if __name__ == "__main__":
    main()
