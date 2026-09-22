#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
# Copyright (C) 2026 Ahmet Efe Nezli
"""Acil stop (PF14) canli panosu.  http://localhost:8774"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from tezgah_pano import Alan, Dugme, Pano

Pano(
    baslik="ACIL STOP — NUCLEO-F767ZI  (PF14)",
    altbaslik=("NC blok: 21->GND · 22->PF14 · INPUT_PULLUP  |  "
               "LOW = birakilmis · HIGH = basili VEYA kablo kopuk  |  "
               "Sirasiyla: bas / birak / SOKETI CEK"),
    alanlar=[
        Alan("ham",    "Ham pin",  alt="1 = HIGH (acik kontak)"),
        Alan("estop",  "E-STOP",   iyi="v===0",
             alt="1 = kesme aktif (basili ya da kablo kopuk)"),
        Alan("mandal", "Mandal",   iyi="v===0",
             alt="bir kez basildi mi — elle acilir"),
        Alan("basma",  "Basma sayisi"),
        Alan("ziplama","Ziplama kenari", iyi="v<=4",
             alt="en kotu gecisteki kenar sayisi"),
        Alan("ziplama_ms","Ziplama suresi", birim="ms", iyi="v<10",
             alt="10 ms ustu ise 100 nF gerekli"),
    ],
    dugmeler=[Dugme("r", "Mandali ac"), Dugme("s", "Sayaclari sifirla")],
    varsayilan_port="COM6", http=8774,
).calistir()
