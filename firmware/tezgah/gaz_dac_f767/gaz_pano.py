#!/usr/bin/env python3
"""Gaz DAC (PA4) panosu.  http://localhost:8777"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from tezgah_pano import Alan, Dugme, Pano

Pano(
    baslik="GAZ — dahili DAC  (PA4 / DAC1_OUT1)",
    altbaslik=("⚠ Kontrolcu BAGLI OLMAMALI · Multimetre: PA4 (CN11-32) <-> GND (CN8-11) · "
               "Once VDDA'yi olc: 3V3 (CN8-7) <-> GND"),
    alanlar=[
        Alan("ham", "Yazilan ham", alt="0..4095"),
        Alan("bekle_mv", "Beklenen", birim="mV", grafik=True,
             alt="VREF 3,30 V varsayimiyla — olculen bununla karsilastirilacak"),
    ],
    dugmeler=[
        Dugme("0", "TABAN (ham 0)",      aciklama="gercek min olculecek, 0 V cikmaz"),
        Dugme("r", "Rolanti 0,80 V"),
        Dugme("d", "%30 guc 1,82 V"),
        Dugme("k", "%45 guc 2,33 V"),
        Dugme("t", "TAVAN (ham 4095)",   tehlike=True,
              aciklama="gercek max olculecek, ~3,1 V bekleniyor"),
        Dugme("h", "Elle ham deger", sayi=True, varsayilan=2000, en_az=0, en_cok=4095),
    ],
    varsayilan_port="COM6", http=8777,
).calistir()
