#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (C) 2026 Ahmet Efe Nezli
"""JK BMS → JSON servisi. Panoyu gerçek batarya verisiyle besler.

BLE'den JK'yi okur, `55 AA EB 90` cerceve turu 0x02'yi cozer ve
http://0.0.0.0:<port>/bms adresinde JSON yayinlar.

    python3 jk_servis.py --mac AA:BB:CC:DD:EE:FF --port 8091

⚠ BLE ayni anda TEK istemci kabul eder: bu servis calisirken telefondaki
JK uygulamasi bagalanamaz (ve tersi).
"""
import argparse, asyncio, json, struct, sys, threading, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

BAS      = b"\x55\xaa\xeb\x90"
CERCEVE  = 300            # veri govdesi; son bayt saglama
HUCRE_N  = 16             # 16S paket

def jk_komut(kod):
    c = bytearray(b"\xaa\x55\x90\xeb"); c.append(kod & 0xFF)
    c.extend(b"\x00" * 14); c.append(sum(c) & 0xFF)
    return bytes(c)

def u16(b, i):  return struct.unpack_from("<H", b, i)[0]
def i16(b, i):  return struct.unpack_from("<h", b, i)[0]
def u32(b, i):  return struct.unpack_from("<I", b, i)[0]
def i32(b, i):  return struct.unpack_from("<i", b, i)[0]

def coz(c):
    """0x02 (hucre bilgisi) cercevesini coz. Ofsetler JK 55AAEB90 protokolu."""
    if len(c) < 190 or c[:4] != BAS or c[4] != 0x02:
        return None
    h = [u16(c, 6 + 2 * i) for i in range(HUCRE_N)]          # mV
    gecerli = [v for v in h if 1000 < v < 5000]
    if not gecerli:
        return None
    return {
        "hucre_mv":  h,
        "bms_ort":   u16(c, 74),
        "bms_denge": u16(c, 76),
        "bms_encok": max(gecerli),
        "bms_enaz":  min(gecerli),
        "v48":       round(u32(c, 150) / 1000.0, 2),          # mV → V
        "bms_guc":   round(u32(c, 154) / 1000.0, 1),          # mW → W
        "bms_i":     round(i32(c, 158) / 1000.0, 2),          # mA → A
        "bms_tmos":  round(i16(c, 144) / 10.0, 1),
        "bms_t1":    round(i16(c, 162) / 10.0, 1),
        "bms_t2":    round(i16(c, 164) / 10.0, 1),
        "bms_soc":   c[173],
        "bms_kalan": round(i32(c, 174) / 1000.0, 2),          # mAh → Ah
        "bms_tam":   round(u32(c, 178) / 1000.0, 2),
        "bms_dongu": u32(c, 182),
    }

DURUM = {"bagli": False, "yas": None, "hata": None, "cerceve": 0, "bozuk": 0}
KILIT = threading.Lock()

class Ucu(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.rstrip("/") not in ("/bms", ""):
            self.send_error(404); return
        with KILIT:
            g = dict(DURUM)
        if g.get("son"):
            g["yas"] = round(time.time() - g.pop("son_t", time.time()), 2)
        govde = json.dumps(g, ensure_ascii=False).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Access-Control-Allow-Origin", "*")   # pano baska porttan cekiyor
        self.send_header("Content-Length", str(len(govde)))
        self.end_headers(); self.wfile.write(govde)
    def log_message(self, *a): pass

async def oku(mac):
    from bleak import BleakClient, BleakScanner
    UUID = "0000ffe1-0000-1000-8000-00805f9b34fb"
    bekle = 2
    while True:
        tampon = bytearray()
        dogrudan = False
        try:
            # ⚠ BlueZ'de adrese DOGRUDAN baglanmak, cihaz onbellekte yoksa
            # BleakDeviceNotFoundError verir. Once tarayip cihaz nesnesini al.
            hedef = await BleakScanner.find_device_by_address(mac, timeout=12)

            # 🔴 3 Eylul 2026: tarama tek basina YETMIYOR. JK, bir istemci
            # bagliyken reklam yayinlamayi birakiyor. Bu servis SIGKILL ile
            # olurse (`systemctl restart` tam olarak bunu yapiyor) BlueZ
            # baglantiyi acik tutuyor: cihaz "Connected: yes" gorunur ama
            # taramada YOKTUR, ve servis bir daha asla baglanamaz.
            # Cikis yolu: taramada bulunamazsa adresin kendisiyle devam et.
            # BleakClient duz adresi kabul ediyor ve bayat baglanti BlueZ'in
            # nesne agacinda durdugu icin bu yol tam da o durumda calisir.
            # Cihaz gercekten yoksa asagidaki baglanma hata verir; mesaj
            # ikisini ayirt edebilmek icin hangi yolun denendigini soyler.
            dogrudan = hedef is None
            if dogrudan:
                hedef = mac

            async with BleakClient(hedef, timeout=20) as c:
                def geldi(_, v):
                    tampon.extend(v)
                    while True:
                        i = tampon.find(BAS)
                        if i < 0:
                            if len(tampon) > 4096: del tampon[:-8]
                            return
                        if len(tampon) - i < CERCEVE: 
                            if i: del tampon[:i]
                            return
                        cer = bytes(tampon[i:i + CERCEVE]); del tampon[:i + CERCEVE]
                        d = coz(cer)
                        with KILIT:
                            if d:
                                DURUM.update(d)
                                DURUM["cerceve"] += 1
                                DURUM["son"] = True; DURUM["son_t"] = time.time()
                                DURUM["hata"] = None
                            else:
                                DURUM["bozuk"] += 1
                await c.start_notify(UUID, geldi)
                with KILIT:
                    DURUM["bagli"] = True; DURUM["hata"] = None
                bekle = 2
                await c.write_gatt_char(UUID, jk_komut(0x97), response=False)
                await asyncio.sleep(0.3)
                while c.is_connected:
                    await c.write_gatt_char(UUID, jk_komut(0x96), response=False)
                    await asyncio.sleep(1.0)
        except Exception as e:
            with KILIT:
                DURUM["bagli"] = False
                DURUM["hata"] = "%s: %s%s" % (
                    type(e).__name__, e,
                    "  [taramada yoktu, adrese dogrudan baglanmayi denedi]"
                    if dogrudan else "")
            await asyncio.sleep(bekle)
            bekle = min(bekle * 2, 30)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mac", required=True)
    ap.add_argument("--port", type=int, default=8091)
    n = ap.parse_args()
    s = ThreadingHTTPServer(("0.0.0.0", n.port), Ucu)
    threading.Thread(target=s.serve_forever, daemon=True).start()
    print("JK servisi: http://0.0.0.0:%d/bms   mac=%s" % (n.port, n.mac), flush=True)
    asyncio.run(oku(n.mac))

if __name__ == "__main__":
    main()
