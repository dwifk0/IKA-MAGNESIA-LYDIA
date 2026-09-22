#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
# Copyright (C) 2026 Ahmet Efe Nezli
"""
BNO055 tezgah panosu (Nucleo-F767ZI, UART modu).

    python.exe bno_pano.py --port COM6
    python.exe bno_pano.py --demo

Aracta (ana firmware, Jetson'da — seri_kopru DURDURULMUS olmali):
    python3 bno_pano.py --port /dev/f767 --baud 921600 --http 8772

⚠ Yuklemeden once bu panoyu KAPAT — seri portu tutuyor.
"""

import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from tezgah_pano import Alan, Dugme, Pano       # noqa: E402

# Ana firmware'in `hata` bayragi (config.h HATA_*).
HATA_BNO_YOK = 0x01

# 🔴 YAW SIFIRLAMA PANODA, KARTTA DEGIL (11 Eylul 2026).
# Eskiden dugme karta `z` yolluyordu, "Kimlik dokumu" da `k`. Tezgah
# firmware'inde ikisi BNO komutu; ANA firmware'de `z` = "direksiyon: burasi
# yeni sifir", `k` = "direksiyon: burasi mekanik sinir". Ayni gun enkoder
# panosunun `z`'si aracta direksiyon referansini kaydirdi. Ikisi de kaldirildi.
_yaw = {"sifir": 0, "son_ham": 0}


def donustur(cift):
    # Ana firmware kalibrasyonu TEK bayt yolluyor: cipin CALIB_STAT registeri
    # olduğu gibi (sys<<6 | gyr<<4 | acc<<2 | mag). Tezgah firmware'i dortlu
    # ayri basiyor; ikisi ayni kutulara dussun.
    if "kalib" in cift:
        k = cift["kalib"]
        cift["ksys"] = (k >> 6) & 3
        cift["kgyr"] = (k >> 4) & 3
        cift["kacc"] = (k >> 2) & 3
        cift["kmag"] = k & 3
    # Cip var mi: ana firmware CHIP_ID satiri basmiyor, hata bayragindan.
    if "hata" in cift and "var" not in cift:
        cift["var"] = 0 if cift["hata"] & HATA_BNO_YOK else 1
    if "yaw" in cift:
        ham = cift["yaw"]
        _yaw["son_ham"] = ham
        cift["yawham"] = ham
        # -180..+180 araligina sar: sifirlanan yon 0, sola/saga isaretli.
        cift["yaw"] = (ham - _yaw["sifir"] + 1800) % 3600 - 1800
    return cift


def yaw_sifirla():
    _yaw["sifir"] = _yaw["son_ham"]


def demo(n):
    """Uydurma veri — sadece arayuz denemesi icin."""
    return {
        "yaw": int((n * 3) % 3600), "ham": int((n * 3) % 3600),
        "roll": int(150 * math.sin(n / 30.0)),
        "pitch": int(90 * math.cos(n / 25.0)),
        "ksys": min(3, n // 60), "kgyr": 3,
        "kacc": min(3, n // 40), "kmag": min(3, n // 90),
        "var": 1, "sicak": 27, "syserr": 0, "opr": 12, "dusen": n // 12,
        "hoverrun": n // 12, "hcevapsiz": 0, "hbicim": 0, "hdiger": 0,
        "sonkod": 7,
    }


# Ana firmware'in basmadigi alanlar 0 kalir; 0 kirmizi degil NOTR gorunsun
# (enkoder panosunda A/B kanal kutulari tam boyle sahte alarm verdi).
TEZGAH = " · yalniz tezgah firmware'i"

Pano(
    baslik="BNO055 — F767ZI tezgah",
    altbaslik=("Veri akiyor olmasi calisiyor demek DEGIL. "
               "<b>sys kalibrasyonu 3 olmadan yaw bir tahmindir</b> ve surukleniyor "
               "— EKF'i o hâlde beslemek kirli odometriden beter."),
    alanlar=[
        Alan("yaw", "YAW (bagil)", birim="derece", olcek=10, basamak=1, grafik=True,
             alt="'Yaw sifirla' dedigin yone gore · + / − isaretli"),
        Alan("roll", "ROLL", birim="derece", olcek=10, basamak=1, grafik=True),
        Alan("pitch", "PITCH", birim="derece", olcek=10, basamak=1, grafik=True),
        Alan("yawham", "YAW (cip)", birim="derece", olcek=10, basamak=1,
             alt="cipin kendi yonu · sifirlama bunu DEGISTIRMEZ"),
        Alan("var", "Cip", alt="1 = cip cevap veriyor (ana firmware: hata biti 0x01 kalkik degil)",
             iyi="v===1 ? true : false"),
        # Kalibrasyon dortlusu. sys ayri renklendiriliyor cunku karar veren o.
        Alan("ksys", "Kalib SYS", alt="3 olmadan yaw'a guvenme",
             iyi="v>=3 ? true : false"),
        Alan("kgyr", "Kalib GYRO", alt="3 sn kimildatma",
             iyi="v>=3 ? true : null"),
        Alan("kacc", "Kalib ACC", alt="6 yuzeye yatir",
             iyi="v>=3 ? true : null"),
        Alan("kmag", "Kalib MAG", alt="havada sekiz ciz — 🔴 ksys=3 iken bile 0 olabilir, AYRI bak",
             iyi="v>=3 ? true : false"),
        Alan("opr", "OPR_MODE (geri okundu)",
             alt="12 = NDOF (mag acik) · 8 = IMU (mag KAPALI, kmag hep 0 kalir)" + TEZGAH,
             iyi="v===12 ? true : (v===0 ? null : false)"),
        Alan("syserr", "SYS_ERR", alt="0 olmali" + TEZGAH,
             iyi="v===0 ? null : false"),
        Alan("dusen", "Dusen okuma", alt="aci okumasi kac kez basarisiz" + TEZGAH, grafik=True,
             iyi="v===0 ? null : false"),
        # Hangi hata oldugu dogrudan sebebi soyluyor — tek "dusen" sayisi soylemiyordu.
        Alan("hoverrun", "↳ BUS_OVER_RUN", alt="0x07 — cok hizli sorguluyoruz (cip kusuru)" + TEZGAH,
             iyi="v===0 ? null : false"),
        Alan("hcevapsiz", "↳ cevapsiz", alt="0xFF — kablo / PS1 / besleme" + TEZGAH,
             iyi="v===0 ? null : false"),
        Alan("hbicim", "↳ bozuk cerceve", alt="0xFE/0xFD — sinyal butunlugu, seviye cevirici" + TEZGAH,
             iyi="v===0 ? null : false"),
        Alan("hdiger", "↳ diger", alt="cipin baska hata kodu" + TEZGAH),
        Alan("sonkod", "Son hata kodu", alt="en son gorulen ham kod" + TEZGAH),
        Alan("sicak", "Sicaklik", birim="C", alt=TEZGAH.lstrip(" ·")),
    ],
    dugmeler=[
        Dugme("yawsifir", "Yaw sifirla", yerel=yaw_sifirla,
              aciklama="su anki yon ileri kabul edilir · panoda, karta komut GITMEZ"),
        # ⚠ `n` `i` `r` yalniz TEZGAH firmware'inde var; ana firmware'de
        # karsiliklari yok, yok sayilir. `z` ve `k` BILEREK kaldirildi (yukari).
        Dugme("n", "NDOF modu", aciklama="manyetometre dahil, yaw MUTLAK" + TEZGAH),
        Dugme("i", "IMU modu", aciklama="manyetometresiz, yaw SURUKLENIR" + TEZGAH),
        Dugme("r", "Cipi resetle", tehlike=True,
              aciklama="donanim reset + yeniden kurulum" + TEZGAH),
    ],
    varsayilan_port="COM6",
    http=8772,
    demo_uretec=demo,
    donustur=donustur,
).calistir()
