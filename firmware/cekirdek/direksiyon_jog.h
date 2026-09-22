// SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
// Copyright (C) 2026 Ahmet Efe Nezli
//
// Direksiyon jog profili — kol sapmasini step hedefine ceviren mantik.
//
// Direksiyon bir step motorla suruluyor ve HOMING YOK: kart acildigi konumu
// sifir kabul ediyor. Dolayisiyla isletme kurali kodun bir parcasi:
// arac her acilista TEKERLEKLER DUZ olacak.
//
// Donanimdan bagimsiz: adim uretimi ve kesme cagiran tarafta.

#pragma once
#include <stdint.h>

namespace direksiyon {

struct JogAyar {
    int32_t jog_hz_max;      // kol tam sapmada adim/saniye
    int32_t onde_max;        // hedefin konumun onune gecebilecegi en fazla adim
    int32_t merkez_hz;       // merkeze donus hizi [adim/s]; 0 = kapali
    int32_t merkez_olu;      // merkez olu bandi [adim]
    int32_t limit_adim;      // +/- yumusak sinir [adim]
};

// Kol sapmasini olu bant duzeltmesiyle birlikte hesaplar.
// Olu bant yalnizca kirpilmaz, DISARI TASINIR: bant kenarinda cikti
// sifirdan aniden siçramaz, sifirdan duzgun buyur.
int32_t sapma_duzelt(int32_t ham_sapma, int32_t olu_bant);

// Bir tikte yeni hedefi hesaplar.
//   sapma   : olu bant duzeltilmis kol sapmasi
//   yay_ucu : kolun tam sapma degeri (olu bant cikarilmis)
//   konum   : step uretecinin GERCEK konumu [adim]
//   eski    : bir onceki hedef [adim]
//   dt_ms   : gecen sure
int32_t hedef_hesapla(const JogAyar& a, int32_t sapma, int32_t yay_ucu,
                      int32_t konum, int32_t eski, uint32_t dt_ms);

// Duraklamadan bir eksene verilebilecek en yuksek hiz: kalan paya trapez
// rampayla girip tam durabilmek icin  v = sqrt(2 * ivme * pay).
// Bunu asan bir hiz, sinira carpmadan duramaz.
int32_t guvenli_hiz(int32_t ivme_adim_s2, int32_t kalan_pay_adim);

}  // namespace direksiyon
