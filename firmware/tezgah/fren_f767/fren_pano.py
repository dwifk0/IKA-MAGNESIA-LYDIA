#!/usr/bin/env python3
"""
Fren aktuatoru + BTS7960B tezgah panosu — NUCLEO-F767ZI.

    python.exe fren_pano.py --port COM6
    python.exe fren_pano.py --demo

⚠ Yuklemeden once bu panoyu KAPAT — seri portu tutuyor.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from tezgah_pano import Alan, Dugme, Pano       # noqa: E402


def demo(n):
    """Uydurma veri — sadece arayuz denemesi icin."""
    pwm = min(190, n % 220)
    akim = 480 + (pwm * 8 if pwm > 150 else 0)
    return {"pwm": pwm, "yon": 1, "komut": 1000, "ters": 0,
            "akim": akim, "akimmv": akim * 3300 // 4095,
            "akimtepe": 1840, "minpwm": pwm, "minbul": 0,
            "stroke": 0, "sayac": n * 100}


Pano(
    baslik="Fren aktuatoru — BTS7960B tezgah (F767ZI)",
    altbaslik=("Frende konum sensoru YOK ve ACS712 cikarildi — tek koruma "
               "<b>BRAKE_STALL_MS</b>, o da 2500 ms <b>tahmin</b>. "
               "Sira: 1) yon  2) PWM_MIN  3) stroke suresi. "
               "🔴 F767 farki: girisler <b>3,3 V</b> mantikla suruluyor "
               "(Mega'da 5 V'tu) — bu test onu da yokluyor. "
               "⚠ B+ 12 V hattina, 48 V modulu oldurur. "
               "⚠ RPWM/LPWM/EN ucune de 10k pull-down sart."),
    alanlar=[
        Alan("pwm", "PWM", alt="0-190 (tavan %75, aktuator 12 V)", grafik=True),
        Alan("yon", "Yon", alt="+1 uygula · -1 serbest · 0 dur"),
        Alan("ters", "FREN_TERS", alt="'uygula' freni biraktiysa cevir",
             iyi="v===0 ? null : true"),
        Alan("akim", "Akim (ham ADC)", grafik=True,
             alt="BTS7960B IS ucu, 12 bit 0-4095 — yukselisi stall demektir"),
        Alan("akimmv", "IS gerilimi", birim="mV",
             alt="~0,118 V/A (1k ile) · 3300'e dayanirsa bolucu sart"),
        Alan("akimtepe", "Akim tepesi", alt="olcum boyunca gorulen en yuksek (ham)"),
        Alan("minpwm", "PWM tarayici", alt="arama sirasinda tirmanan deger"),
        Alan("minbul", "BRAKE_PWM_MIN", alt="bulundugunda config.h'ye yazilacak",
             iyi="v>0 ? true : null"),
        Alan("sayac", "Stroke sayaci", birim="ms", alt="olcum surerken sayar"),
        Alan("stroke", "TAM STROKE", birim="ms",
             alt="BRAKE_STALL_MS bundan UZUN olmali",
             iyi="v>0 ? true : null"),
    ],
    dugmeler=[
        Dugme("f", "FREN UYGULA", sayi=True, varsayilan=1000, en_az=0,
              en_cok=1000, aciklama="binde (1000 = tam)"),
        Dugme("s", "SERBEST BIRAK", sayi=True, varsayilan=1000, en_az=0,
              en_cok=1000),
        Dugme("d", "DUR", tehlike=True,
              aciklama="aktuator bulundugu yerde kalir (vidali)"),
        Dugme("r", "FREN_TERS cevir", aciklama="1. olcum: yon dogru mu"),
        Dugme("m", "PWM_MIN ara", aciklama="2. olcum: sifirdan tirmanir"),
        Dugme("k", "KIMILDADI", aciklama="ilk hareket aninda bas"),
        Dugme("t", "Stroke olc", aciklama="3. olcum: tam gucte sureyi say"),
        Dugme("u", "DAYANDI", aciklama="uca dayanip durdugunda bas"),
        Dugme("a", "Akim okumayi ac/kapa", aciklama="IS uclari A0/A1'e bagliysa (PA3 / PC0)"),
        Dugme("z", "Sayaclari sifirla"),
    ],
    varsayilan_port="COM6",
    http=8775,
    demo_uretec=demo,
).calistir()
