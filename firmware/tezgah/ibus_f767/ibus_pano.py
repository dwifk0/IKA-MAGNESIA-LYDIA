#!/usr/bin/env python3
"""FlySky iBUS -> F767ZI canli panosu.  http://localhost:8776"""

import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from tezgah_pano import Alan, Dugme, Pano

# 29 Agustos'ta kararlastirilan duzen. Test bunu DOGRULAMAK icin var —
# etiketler beklentiyi yaziyor, olculen baska cikarsa etiket degil GERCEK dogrudur.
KANALLAR = [
    ("ch1",  "CH1  sag yatay",  "direksiyon / taret pan",  True),
    ("ch2",  "CH2  sag dikey",  "GAZ / taret tilt",        True),
    ("ch3",  "CH3  sol dikey",  "FREN",                    True),
    ("ch4",  "CH4  sol yatay",  "bos?",                    False),
    ("ch5",  "CH5",             "?",                       False),
    ("ch6",  "CH6",             "?",                       False),
    ("ch7",  "CH7  SwA",        "YAZILIMSAL KESME",        False),
    ("ch8",  "CH8  SwB",        "taret aktif",             False),
    ("ch9",  "CH9  SwC",        "mod (3 kademe)",          False),
    ("ch10", "CH10",            "?",                       False),
]

alanlar = [Alan(ad, etiket, birim="us", grafik=grafik, alt=alt,
                iyi="v>=980 && v<=2020")
           for ad, etiket, alt, grafik in KANALLAR]

alanlar += [
    Alan("hz", "Cerceve/sn", grafik=True, iyi="v>=100",
         alt="beklenen ~130"),
    Alan("saglam", "Saglam", alt="artmali"),
    Alan("bozuk", "BOZUK", iyi="v===0",
         alt="0 olmali; artiyorsa kablo/gurultu"),
    Alan("sessiz", "Son cerceve", birim="ms", iyi="v<100",
         alt="alici susarsa buyur"),
    Alan("donuk", "DONUK", iyi="v===0",
         alt="1 = cerceve geliyor ama degerler donmus (FAILSAFE)"),
]

Pano(
    baslik="FlySky iBUS — NUCLEO-F767ZI",
    altbaslik=("UART5 RX = PD2 · Kollari TEK TEK oynat, ham kutuk hangi kanalin "
               "oynadigini yazar · FAILSAFE: vericiyi kapat, DONUK 1 olmali"),
    alanlar=alanlar,
    dugmeler=[Dugme("s", "Sayaclari sifirla")],
    varsayilan_port="COM6", http=8776,
).calistir()
