// SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
// Copyright (C) 2026 Ahmet Efe Nezli
//
// 8 baytlik seri cerceve — kurma, cozme ve bayt basina durum makinesi.
//
// Cerceve:  [0xAA][tip][v0_yuksek][v0_dusuk][v1_yuksek][v1_dusuk][XOR][0x55]
//           XOR = bayt[1] ^ bayt[2] ^ bayt[3] ^ bayt[4] ^ bayt[5]
//           v0 ve v1 isaretli 16 bit, BUYUK SONLU (yuksek bayt once).
//
// Bu dosyada donanim yoktur: Arduino, HAL, zamanlayici ya da kesme kullanmaz.
// Bu sayede ayni kod hem kartta hem masaustunde derlenip test edilebilir
// (bkz. ../test/test_cerceve.cpp).
//
// Tam protokol tablosu: ../../docs/PROTOKOL.md

#pragma once
#include <stdint.h>
#include <stddef.h>

namespace cerceve {

constexpr uint8_t BAS  = 0xAA;   // baslangic
constexpr uint8_t SON  = 0x55;   // bitis
constexpr size_t  BOY  = 8;      // sabit uzunluk

struct Paket {
    uint8_t tip;
    int16_t v0;
    int16_t v1;
};

// Paketi 8 baytlik tampona yazar. `hedef` en az BOY bayt olmali.
void kur(const Paket& p, uint8_t* hedef);

// 8 baytlik bir tamponu dogrular ve cozer. Gecerliyse true.
// Bas bayti, bitis bayti ve XOR'un ucu birden tutmali.
bool coz(const uint8_t* kaynak, Paket& hedef);

// XOR saglamasini hesaplar (bayt 1..5).
uint8_t xor_hesapla(const uint8_t* p);

// ─────────────────────────────────────────────────────────────────────────
// Bayt basina cerceve toplayici.
//
// Neden bayt basina: kart ayni USB akisinda hem ikili paket hem metin teshis
// komutu aliyor. Baytin hangisine ait oldugu ARAYAN tarafta belirleniyor;
// buraya yalnizca ikiliye ait oldugu bilinen baytlar giriyor. `suruyor()`
// tam da bunun icin var — cercevenin ortasindaki bir bayt metin araligina
// dusse bile metin sanilmamali.
// ─────────────────────────────────────────────────────────────────────────
class Toplayici {
public:
    // Tek bayt besler. Tam ve gecerli bir cerceve tamamlandiysa true doner
    // ve `cikti`ya yazar.
    bool bayt(uint8_t b, Paket& cikti);

    // Yarim kalan cerceveyi birakir. Cagiran, son baytin uzerinden zaman
    // asimi gectiginde cagirir.
    //
    // ⚠ Bu olmazsa tek bir kayip bayt senkronu KALICI olarak kaydirir:
    // toplayici bir sonraki 0xAA'yi cercevenin ortasinda yakalar ve hat
    // sessizce olur — hata da vermez.
    void birak() { n_ = 0; }

    // Cerceve toplanmasi suruyor mu?
    bool suruyor() const { return n_ > 0; }

    // Teshis sayaclari.
    //
    // 🔴 "Paket gelmiyor" tek basina sebebi SOYLEMEZ. Bunlarla birlikte soyler:
    //    bayt == 0                       -> hatta hic veri yok: kablo / ortak GND
    //    bayt > 0, bas == 0              -> veri var ama bu protokol degil: baud
    //    bayt > 0, bas > 0, paket == 0   -> cerceve yakalaniyor, XOR tutmuyor:
    //                                       gurultu ya da yanlis surum
    uint32_t bayt_sayisi()  const { return bayt_; }
    uint32_t bas_sayisi()   const { return bas_; }
    uint32_t paket_sayisi() const { return paket_; }
    uint32_t bozuk_sayisi() const { return bozuk_; }

private:
    uint8_t  ara_[BOY] = {0};
    uint8_t  n_ = 0;
    uint32_t bayt_ = 0, bas_ = 0, paket_ = 0, bozuk_ = 0;
};

}  // namespace cerceve
