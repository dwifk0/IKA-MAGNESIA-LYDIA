#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
# Copyright (C) 2026 Ahmet Efe Nezli
"""
JK Smart BMS — BLE keşif ve ham çerçeve dökümü.

Ana bataryada (16S5P LiFePO4, 51,2 V 30 Ah) BMS'in dışarı çıkmış kablolu
portu YOK: yalnız şarj jakı ve XT90 çıkışı var. Veriye tek erişim yolu
Bluetooth. Bu betik o yolun ilk adımı.

Ne yapar:
  1. Çevredeki BLE cihazlarını tarar, JK olma ihtimali yüksek olanı işaretler.
  2. Seçilen cihaza bağlanır, GATT servis/karakteristik ağacını döker.
  3. Bildirim (notify) karakteristiğine abone olur, gelen HAM çerçeveleri
     hem ekrana hem dosyaya yazar.
  4. Çerçeve başlığından protokol sürümünü tahmin eder.

Neden ham döküm: JK'nın sahada iki farklı çerçeve formatı dolaşıyor
(eski 4E 57 "NW", yeni 55 AA EB 90). Hangisinin geldiği etiketten değil,
ancak canlı veriden anlaşılır. Çözümleyiciyi ondan sonra yazacağız —
tahmine göre yazılan ayrıştırıcı sessizce yanlış sayı üretir.

Kurulum (Jetson, internet varken bir kez):
    pip3 install bleak

Kullanım:
    python3 bms_tara.py                  # yalnız tara, listele
    python3 bms_tara.py --adres AA:BB:.. # bağlan ve 30 sn dinle
    python3 bms_tara.py --adres .. --sure 120 --uyandir

UYARI: BLE aynı anda TEK istemci kabul eder. Bu betik bağlıyken telefondaki
uygulama bağlanamaz; tersi de geçerli. Denemeden önce uygulamayı kapat.
"""

import argparse
import asyncio
import datetime
import sys
from pathlib import Path

try:
    from bleak import BleakScanner, BleakClient
except ImportError:
    sys.exit("bleak yok. Kur:  pip3 install bleak")

# JK BMS'lerde yaygın olan seri-port benzeri servis. Başka UUID de
# çıkabilir; bu yüzden aşağıda tüm ağaç yine de dökülüyor.
JK_SERVIS = "0000ffe0-0000-1000-8000-00805f9b34fb"
JK_KARAKTER = "0000ffe1-0000-1000-8000-00805f9b34fb"

# Cihaz adında bunlardan biri geçiyorsa aday sayılır.
AD_IPUCU = ("jk", "jk-bms", "jkbms", "bms")

# İki protokol sürümünün başlangıç imzaları.
IMZA = {
    b"\x4e\x57": "eski JK protokolü (4E 57 'NW')",
    b"\x55\xaa\xeb\x90": "yeni JK protokolü (55 AA EB 90)",
}

# Bazı modeller abone olunca kendiliğinden yayına başlar, bazıları bir
# istek bekler. --uyandir verilirse ikisi de sırayla denenir.
# Bu baytlar açık kaynak uygulamalardan; DOĞRULANMADI, amaç yalnızca
# sessiz kalan bir cihazı konuşturmak.
UYANDIR = [
    ("yeni · hücre bilgisi",
     bytes([0xAA, 0x55, 0x90, 0xEB, 0x96, 0x00] + [0x00] * 13 + [0x11])),
    ("yeni · cihaz bilgisi",
     bytes([0xAA, 0x55, 0x90, 0xEB, 0x97, 0x00] + [0x00] * 13 + [0x12])),
    ("eski · tümünü oku",
     bytes([0x4E, 0x57, 0x00, 0x13, 0x00, 0x00, 0x00, 0x00, 0x06, 0x03,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x68, 0x00, 0x00, 0x01, 0x29])),
]


def zaman():
    return datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]


def imza_bul(veri: bytes):
    for on, ad in IMZA.items():
        if veri.startswith(on):
            return ad
    return None


async def tara(sure):
    print(f"BLE taranıyor ({sure} sn)…\n")
    cihazlar = await BleakScanner.discover(timeout=sure)
    if not cihazlar:
        print("Hiçbir cihaz görünmedi. Bluetooth açık mı? "
              "(sudo systemctl status bluetooth)")
        return
    cihazlar.sort(key=lambda d: (d.name or "").lower())
    print(f"{'ADRES':<20} {'AD':<26} ADAY")
    for d in cihazlar:
        ad = d.name or "—"
        aday = any(i in ad.lower() for i in AD_IPUCU)
        print(f"{d.address:<20} {ad:<26} {'← JK olabilir' if aday else ''}")
    print("\nAdayı seçip:  python3 bms_tara.py --adres <ADRES>")


async def dinle(adres, sure, uyandir, cikti):
    kayit = open(cikti, "a", encoding="utf-8")
    kayit.write(f"\n===== {datetime.datetime.now().isoformat()} · {adres} =====\n")
    sayac = {"n": 0, "bayt": 0, "imza": None}

    def geldi(_, veri: bytearray):
        b = bytes(veri)
        sayac["n"] += 1
        sayac["bayt"] += len(b)
        if sayac["imza"] is None:
            sayac["imza"] = imza_bul(b)
            if sayac["imza"]:
                print(f"\n>>> PROTOKOL: {sayac['imza']}\n")
                kayit.write(f"# protokol: {sayac['imza']}\n")
        satir = f"{zaman()} [{len(b):3}] {b.hex(' ')}"
        print(satir)
        kayit.write(satir + "\n")
        kayit.flush()

    print(f"{adres} bağlanılıyor…")
    async with BleakClient(adres, timeout=20.0) as c:
        print("bağlandı.\n--- GATT ağacı ---")
        bildirim = []
        for s in c.services:
            print(f"servis {s.uuid}  {s.description}")
            for ch in s.characteristics:
                print(f"   karakter {ch.uuid}  {','.join(ch.properties)}")
                if "notify" in ch.properties or "indicate" in ch.properties:
                    bildirim.append(ch.uuid)
        print("------------------\n")

        if not bildirim:
            print("Bildirim veren karakteristik yok — bu cihaz JK BMS olmayabilir.")
            return
        hedef = JK_KARAKTER if JK_KARAKTER in bildirim else bildirim[0]
        print(f"abone olunuyor: {hedef}\n")
        await c.start_notify(hedef, geldi)

        if uyandir:
            yazilabilir = [ch.uuid for s in c.services for ch in s.characteristics
                           if "write" in ch.properties
                           or "write-without-response" in ch.properties]
            yaz_hedef = hedef if hedef in yazilabilir else (
                yazilabilir[0] if yazilabilir else None)
            if yaz_hedef is None:
                print("Yazılabilir karakteristik yok, uyandırma atlandı.")
            else:
                for ad, paket in UYANDIR:
                    print(f"[uyandır] {ad}: {paket.hex(' ')}")
                    try:
                        await c.write_gatt_char(yaz_hedef, paket, response=False)
                    except Exception as e:                    # noqa: BLE00
                        print(f"   yazılamadı: {e}")
                    await asyncio.sleep(2)

        print(f"{sure} sn dinleniyor… (Ctrl+C ile bitir)\n")
        try:
            await asyncio.sleep(sure)
        except asyncio.CancelledError:
            pass
        await c.stop_notify(hedef)

    print(f"\n{sayac['n']} çerçeve · {sayac['bayt']} bayt · döküm: {cikti}")
    if sayac["n"] == 0:
        print("Hiç çerçeve gelmedi. Sırayla dene: --uyandir ver, "
              "telefondaki uygulamanın kapalı olduğundan emin ol, "
              "BMS'i uygulamadan bir kez uyandır.")
    elif sayac["imza"] is None:
        print("Bilinen iki JK imzası da tutmadı — dökümü birlikte inceleyelim.")


def main():
    a = argparse.ArgumentParser(description="JK BMS · BLE keşif ve ham döküm")
    a.add_argument("--adres", help="BLE adresi; verilmezse yalnız tarar")
    a.add_argument("--sure", type=float, default=30, help="dinleme süresi (sn)")
    a.add_argument("--tara-sure", type=float, default=8, help="tarama süresi (sn)")
    a.add_argument("--uyandir", action="store_true",
                   help="sessiz kalırsa bilinen istek paketlerini yolla")
    a.add_argument("--cikti", default=str(Path(__file__).with_name("bms_dokum.txt")))
    n = a.parse_args()
    try:
        if n.adres:
            asyncio.run(dinle(n.adres, n.sure, n.uyandir, n.cikti))
        else:
            asyncio.run(tara(n.tara_sure))
    except KeyboardInterrupt:
        print("\nkesildi.")


if __name__ == "__main__":
    main()
