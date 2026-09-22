// SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
// Copyright (C) 2026 Ahmet Efe Nezli

#include "cerceve.h"

namespace cerceve {

uint8_t xor_hesapla(const uint8_t* p) {
    return (uint8_t)(p[1] ^ p[2] ^ p[3] ^ p[4] ^ p[5]);
}

void kur(const Paket& p, uint8_t* hedef) {
    hedef[0] = BAS;
    hedef[1] = p.tip;
    hedef[2] = (uint8_t)((uint16_t)p.v0 >> 8);
    hedef[3] = (uint8_t)((uint16_t)p.v0 & 0xFF);
    hedef[4] = (uint8_t)((uint16_t)p.v1 >> 8);
    hedef[5] = (uint8_t)((uint16_t)p.v1 & 0xFF);
    hedef[6] = xor_hesapla(hedef);
    hedef[7] = SON;
}

bool coz(const uint8_t* k, Paket& h) {
    if (k[0] != BAS) return false;
    if (k[7] != SON) return false;
    if (xor_hesapla(k) != k[6]) return false;
    h.tip = k[1];
    h.v0  = (int16_t)(((uint16_t)k[2] << 8) | k[3]);
    h.v1  = (int16_t)(((uint16_t)k[4] << 8) | k[5]);
    return true;
}

bool Toplayici::bayt(uint8_t b, Paket& cikti) {
    bayt_++;

    if (n_ == 0) {
        // Senkron disindayiz: yalnizca baslangic bayti bizi ice alir.
        if (b == BAS) { ara_[n_++] = b; bas_++; }
        return false;
    }

    ara_[n_++] = b;
    if (n_ < BOY) return false;

    n_ = 0;
    if (!coz(ara_, cikti)) { bozuk_++; return false; }
    paket_++;
    return true;
}

}  // namespace cerceve
