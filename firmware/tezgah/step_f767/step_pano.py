#!/usr/bin/env python3
"""
Direksiyon step motoru tezgah panosu.

    python.exe step_pano.py --port COM6
    python.exe step_pano.py --demo        # kart yokken arayuzu dene

⚠ Yuklemeden once bu panoyu KAPAT — seri portu tutuyor.
"""

import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from tezgah_pano import Alan, Dugme, Pano       # noqa: E402

ADIM_TUR = 16000     # step_mega.ino ile AYNI olmali (1600 mikroadim x 10)


def demo(n):
    """Uydurma veri — sadece arayuz denemesi icin."""
    adim = int(2 * ADIM_TUR * math.sin(n / 40.0))
    return {"adim": adim, "hedef": adim, "tur10": int(adim * 10 / ADIM_TUR),
            "hz": int(1800 * abs(math.cos(n / 40.0))),
            "yon": 1 if math.cos(n / 40.0) > 0 else -1,
            "mesgul": 1, "gitgel": 0, "tavan": 3000, "darbe": n * 40,
            "ivme": 6000, "mikro": 1600, "derecesn": 67}


Pano(
    baslik="Direksiyon step motoru — tezgah",
    altbaslik=("Sayac kac adim GONDERDIGIMIZI bilir, motorun DONDUGUNU bilmez. "
               "<b>Mile bant yapistir</b> — git-gel bitince isaret basladigi yere "
               "donmediyse adim kacti, sayac yalan soyluyor."),
    alanlar=[
        Alan("tur10", "Konum", birim="tur", olcek=10, basamak=1, grafik=True,
             alt="reduktor cikisi, motor degil"),
        Alan("adim", "Adim sayaci", birim="adim"),
        Alan("hz", "Darbe frekansi", birim="Hz", grafik=True),
        Alan("mesgul", "Durum", alt="1 = hareket ediyor",
             iyi="v===0 ? null : true"),
        Alan("gitgel", "Git-gel kalan", birim="bacak",
             alt="0 = test bitti veya calismiyor"),
        Alan("derecesn", "Gercek hiz", birim="derece/sn", grafik=True,
             alt="cikis milinde. ASIL BAKILACAK SAYI bu, Hz degil"),
        Alan("tavan", "Hiz tavani", birim="Hz",
             alt="kacirmanin basladigi deger aranan sayidir"),
        Alan("ivme", "Ivme", birim="Hz/s",
             alt="yuksek tavanlarda gercek sinir budur, tavan degil"),
        Alan("mikro", "Mikroadim", alt="DIP SW5-SW8 ile AYNI olmali"),
        Alan("yon", "Yon", alt="+1 / -1 / 0 · hangisi 'sag' DENEMEYLE bulunur"),
        Alan("darbe", "Toplam darbe", alt="uretilen darbe -- donen mil DEGIL"),
    ],
    dugmeler=[
        Dugme("r", "SAG >", sayi=True, varsayilan=20, en_az=1, en_cok=200,
              aciklama="onda bir tur (20 = 2,0 tur)"),
        Dugme("l", "< SOL", sayi=True, varsayilan=20, en_az=1, en_cok=200),
        Dugme("g", "GIT-GEL (sag 2 / sol 2)", sayi=True, varsayilan=5,
              en_az=1, en_cok=100, aciklama="kac kez tekrarlanacak"),
        Dugme("h", "Hiz tavani", sayi=True, varsayilan=3000, en_az=100,
              en_cok=20000, aciklama="Hz. Kacirmanin basladigi hizi bul."),
        Dugme("i", "Ivme", sayi=True, varsayilan=6000, en_az=500,
              en_cok=200000,
              aciklama="Hz/s. Tavan yuksekken hareket kisa ise sinir BURASI."),
        Dugme("m", "Mikroadim", sayi=True, varsayilan=800, en_az=400,
              en_cok=3200, tehlike=True,
              aciklama="400/800/1600/3200. ONCE DIP'i degistir, sonra bunu."),
        Dugme("d", "DUR", tehlike=True),
        Dugme("z", "Sayaci sifirla", aciklama="burasi sifir kabul edilir"),
    ],
    varsayilan_port="COM6",
    http=8771,
    demo_uretec=demo,
).calistir()
