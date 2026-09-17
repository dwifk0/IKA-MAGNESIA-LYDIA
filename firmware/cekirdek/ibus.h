// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Ahmet Efe Nezli
//
// FlySky iBUS cozucusu — 32 baytlik cerceve, 14 kanal, ~128 Hz.
//
// Cerceve: [0x20][0x40] + 14x2 bayt kanal (kucuk sonlu) + 2 bayt saglama
//          saglama = 0xFFFF - (ilk 30 baytin toplami)
//
// Donanimdan bagimsiz: UART okuma cagiran tarafta, burada yalnizca cozme.

#pragma once
#include <stdint.h>

namespace ibus {

constexpr uint8_t  BAS0   = 0x20;
constexpr uint8_t  BAS1   = 0x40;
constexpr uint8_t  BOY    = 32;
constexpr uint8_t  KANAL  = 14;
constexpr uint16_t MERKEZ = 1500;   // us

class Cozucu {
public:
    // Kanallari guvenli baslangic degerlerine kurar.
    //
    // ⚠ Acilista merkeze kurmak YETMEZ. Uc kanal ozel olarak ele alinir:
    //   - kesme kanali "KESIK" tarafina,
    //   - fren kanali serbest tarafina,
    //   - mod kanali MANUEL ucuna.
    // Sebep: mod kanalinda 1500 ORTA kademedir; merkezde birakilirsa ilk
    // cerceve gelmeden sistem kendini yari otonom sanar. Guvenli varsayilan
    // "ortada" degil, "en zararsiz uctadir".
    void kur(uint8_t kesme_kanali, uint16_t kesme_kesik_us,
             uint8_t fren_kanali,  uint16_t fren_serbest_us,
             uint8_t mod_kanali,   uint16_t mod_manuel_us);

    // Tek bayt besler. Tam ve saglamasi tutan bir cerceve tamamlandiysa true.
    bool bayt(uint8_t b);

    // Kanal degeri [us]. Gecersiz indis MERKEZ dondurur.
    uint16_t oku(uint8_t i) const { return (i < KANAL) ? kanal_[i] : MERKEZ; }

    // Cerceve sayacini saniyelik pencereye tasir. Cagiran her 1000 ms'de
    // bir cagirir.
    //
    // ⚠ Kumulatif sayac degil HZ tutuluyor: "toplam 41283 cerceve geldi"
    // bir kopmayi GIZLER, "hz 128'den 0'a dustu" gizlemez.
    void pencere_kapat();

    uint32_t hz()        const { return hz_; }
    uint32_t saglam()    const { return saglam_; }
    uint32_t bozuk()     const { return bozuk_; }
    uint32_t bayt_sayisi() const { return bayt_; }
    uint32_t bas_sayisi()  const { return bas20_; }

    // Hic gecerli cerceve alindi mi?
    //
    // ⚠ Bu ayri sorulmali. "son_cerceve_zamani" sifirken gecen sureyi esikle
    // karsilastirmak, acilistan sonraki ilk pencerede sistemin "sinyal VAR"
    // sanmasina yol acar.
    bool hic_cerceve_geldi() const { return saglam_ > 0; }

private:
    bool cerceve_tamamla();

    uint8_t  ara_[BOY] = {0};
    uint8_t  n_ = 0;
    uint16_t kanal_[KANAL] = {0};
    uint32_t saglam_ = 0, bozuk_ = 0, bayt_ = 0, bas20_ = 0;
    uint32_t pencere_ = 0, hz_ = 0;
};

}  // namespace ibus
