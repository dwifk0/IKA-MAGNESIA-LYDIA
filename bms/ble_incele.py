#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (C) 2026 Ahmet Efe Nezli
"""Bir BLE cihazina baglanip GATT servislerini dokum. Sadece OKUR."""
import asyncio, sys
from bleak import BleakClient

ADRES = sys.argv[1]
# JK BMS: 0xFFE0/0xFFE1 · DALY BLE modulleri: FFF0/FFF1/FFF2 ya da 6E40xxxx (Nordic UART)
ILGINC = ("ffe0", "ffe1", "fff0", "fff1", "fff2", "6e40")

async def main():
    print("baglaniliyor:", ADRES)
    try:
        async with BleakClient(ADRES, timeout=20) as c:
            print("BAGLANDI\n")
            for s in c.services:
                print("servis %s  %s" % (s.uuid, s.description))
                for ch in s.characteristics:
                    kisa = ch.uuid[4:8].lower()
                    im = "  << BMS IMZASI" if any(k in ch.uuid.lower() for k in ILGINC) else ""
                    print("   karakteristik %s  %s%s" % (ch.uuid, ",".join(ch.properties), im))
    except Exception as e:
        print("BAGLANAMADI: %s: %s" % (type(e).__name__, e))

asyncio.run(main())
