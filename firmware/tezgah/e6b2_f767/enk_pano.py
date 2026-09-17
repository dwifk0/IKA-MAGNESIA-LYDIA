#!/usr/bin/env python3
"""
E6B2 enkoder tezgah panosu (Nucleo-F767ZI, TIM4 donanim sayici).

    python.exe enk_pano.py --port COM6
    python.exe enk_pano.py --demo

Aracta (ana firmware, Jetson'da — seri_kopru DURDURULMUS olmali):
    python3 enk_pano.py --port /dev/f767 --baud 921600 --http 8773

⚠ Yuklemeden once bu panoyu KAPAT — seri portu tutuyor.
"""

import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from tezgah_pano import Alan, Dugme, Pano       # noqa: E402

SAYIM_TUR = 2400

# 🔴 SAYIM SIFIRLAMA PANODA, KARTTA DEGIL (11 Eylul 2026).
# Eskiden dugme karta `z` yolluyordu. Tezgah firmware'inde `z` enkoderi
# sifirliyor, ama ANA firmware'de `z` = "direksiyon: burasi yeni sifir".
# Aracta basildi ve direksiyon referansini kaydirdi. Artik karta hicbir sey
# gitmiyor: son ham sayim sifir noktasi olarak tutuluyor, gosterilen sayim
# ondan fark. Iki firmware'de de ayni calisir; kartin sayaci hic degismez.
_sifir = {"nokta": 0, "son_ham": 0}


def donustur(cift):
    if "sayim" in cift:
        ham = cift["sayim"]
        _sifir["son_ham"] = ham
        cift["ham"] = ham
        cift["sayim"] = ham - _sifir["nokta"]
        # Konum da sifirdan olculsun. Ana firmware tur100 hic basmiyor;
        # tezgah firmware'i basiyor ama ham sayimdan — ikisinde de buradan.
        cift["tur100"] = int(cift["sayim"] * 100 / SAYIM_TUR)
    return cift


def sayimi_sifirla():
    _sifir["nokta"] = _sifir["son_ham"]


def demo(n):
    """Uydurma veri — sadece arayuz denemesi icin."""
    s = int(SAYIM_TUR * 1.5 * math.sin(n / 50.0))
    return {"sayim": s, "tur100": int(s * 100 / SAYIM_TUR),
            "rpm": int(60 * math.cos(n / 50.0)),
            "hizs": int(2400 * math.cos(n / 50.0)),
            "akanal": 1, "bkanal": 1, "hatali": 0, "z": n // 100}


Pano(
    baslik="E6B2 enkoder — F767ZI tezgah",
    altbaslik=("Sayimin ARTMASI yetmez — <b>tek kanal da sayim artirir.</b> "
               "Sponsorun 300B'si tam boyle elendi: A calisiyordu, B oluydu. "
               "Ters cevirince sayim GERI gitmiyorsa kuadratur yok."),
    alanlar=[
        Alan("tur100", "Konum", birim="tur", olcek=100, basamak=2, grafik=True,
             alt="sifirlamadan beri"),
        Alan("sayim", "Sayim", alt="sifirlamadan beri · 1 tur = 2400 (600 P/R x 4)",
             grafik=True),
        Alan("ham", "Ham sayim", alt="kartin sayaci · sifirlama bunu DEGISTIRMEZ"),
        Alan("rpm", "Devir", birim="rpm", grafik=True,
             alt="E6B2 mekanik limiti 6000 · yalniz tezgah firmware'i"),
        Alan("akanal", "A kanali", alt="1 = hem HIGH hem LOW goruldu",
             iyi="v===1 ? true : false"),
        Alan("bkanal", "B kanali", alt="1 = hem HIGH hem LOW goruldu",
             iyi="v===1 ? true : false"),
        Alan("hatali", "Yasak gecis", alt="GERCEK ihlal — 0 olmali · ornekleme sikken sayildi",
             iyi="v===0 ? null : false"),
        Alan("atlanan", "Yargilanamaz",
             alt="olcum atladigi icin karar verilemeyen gecis · enkoder hakkinda BIR SEY SOYLEMEZ"),
        Alan("bosluk", "En buyuk bosluk", birim="us",
             alt="ornekler arasi en uzun ara · seri satir basarken buyur"),
        Alan("z", "Z indeksi", alt="tam turda 1 artmali"),
        Alan("zara", "Z arasi sayim",
             alt="OTOMATIK OLCEK TESTI: +2400 / -2400 olmali · kucuk sayi = Z gurultusu"),
        Alan("alvl", "A ham seviye", alt="pinin SU ANKI hali · 1 = HIGH"),
        Alan("blvl", "B ham seviye", alt="pinin SU ANKI hali · 1 = HIGH"),
        Alan("zlvl", "Z ham seviye", alt="pinin SU ANKI hali · 1 = HIGH"),
        Alan("amask", "A gorulen", alt="1 yalniz LOW · 2 yalniz HIGH · 3 ikisi de"),
        Alan("bmask", "B gorulen", alt="1 yalniz LOW · 2 yalniz HIGH · 3 ikisi de"),
        Alan("cnt", "TIM4 sayaci", alt="ham donanim sayaci, 0-65535"),
    ],
    dugmeler=[
        Dugme("sifirla", "Sayimi sifirla", yerel=sayimi_sifirla,
              aciklama="panoda sifirlar · karta komut GITMEZ"),
        # ⚠ `h` ve `i` yalniz TEZGAH firmware'inde var; ana firmware'de
        # karsiliklari yok, yok sayilir. `z` BILEREK kaldirildi (yukariya bak).
        Dugme("h", "Saglik sayaclarini sifirla",
              aciklama="kanal gormeleri ve yasak gecis sayisi · yalniz tezgah firmware'i"),
        Dugme("i", "Z araligini olc",
              aciklama="son Z'den beri kac sayim gecti (tam tur = 2400) · yalniz tezgah firmware'i"),
    ],
    varsayilan_port="COM6",
    http=8773,
    demo_uretec=demo,
    donustur=donustur,
).calistir()
