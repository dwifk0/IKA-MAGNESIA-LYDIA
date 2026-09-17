// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Ahmet Efe Nezli
//
// Kalibrasyon sabitleri — YER TUTUCU.
// Buradaki degerler araca ozeldir ve bu depoda yayimlanmamistir.
// Her birinin nasil olculdugu: docs/KALIBRASYON.md
//
// Kart bu sabitleri derleme aninda degil, calisma aninda 0x09 paketiyle alir
// ve kalici bellekte tutar. Bu dosya yalnizca hangi sabitlerin var oldugunu
// ve hangi birimde beklendigini gosterir.

#pragma once

// --- odometri ---
#define ENK_TEKER_CEVRE_MM      0     // yuvarlanma cevresi [mm x10]  (olculecek)
#define ENK_DISLI_ORANI         0     // enkoder turu : teker turu [x1000]
#define GOST_DARBE_TUR          0     // gosterge darbe/tur [x10]

// --- direksiyon ---
#define DIR_ORAN                0     // kolon derecesi / teker derecesi [x1000]
#define DIR_ISARET             +1     // +1 veya -1 — ilk testte tekeri yerden kes
#define DIR_LIMIT_DER           0     // kolon sinirı [derece]
#define DIR_MERKEZ_HZ           0     // merkeze donus hizi [Hz]

// --- gaz (acik dongu) ---
#define GAZ_KALKIS_MV           0     // aracin hareket ettigi gerilim [mV]
#define GAZ_EGIM_MV_PER_MMS     0     // hiz komutunu gerilime ceviren egim

// --- fren ---
#define FREN_OLU_BOLGE_BINDE    0     // altinda hicbir hareket olmayan komut
#define FREN_STALL_MS           0     // ayni yonde kilitlenme esigi [ms]

// --- taret ---
#define TARET_PAN_MIN           0
#define TARET_PAN_MAX           0
#define TARET_TILT_MIN          0
#define TARET_TILT_MAX          0
#define TARET_JOG_DPS           0     // jog hizi [derece/sn]
#define TARET_EXPO              0     // stick ustel egrisi [0..100]
