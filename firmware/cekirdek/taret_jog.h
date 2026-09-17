// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Ahmet Efe Nezli
//
// TARET JOG — sag kolun sapmasindan pan/tilt acisina.
//
// 🔴 Aci FLOAT tutuluyor, tamsayi degil. 60 derece/sn x 10 ms = 0,6 derece
// tik basina; tamsayida bu her tikte 0'a yuvarlanir ve taret HIC KIMILDAMAZ.
// "Kolu itiyorum, hicbir sey olmuyor" hatasi tam olarak boyle doguyor.

#pragma once
#include <stdint.h>

namespace taret {

struct Ayar {
    uint16_t merkez_us, yay_ucu_us, olu_bant_us;
    float    dps;          // kol tam sapmada derece/saniye
    float    expo;         // 0..1 ustel egri miktari
    float    pan_min, pan_max, tilt_min, tilt_max;
};

// Kol sapmasini -1..+1 araligina cevirir.
//
// Olu bant SONRASI yeniden olcekleniyor: yoksa bandin hemen disinda hiz
// sifirdan degil, bandin karsiligi olan bir degerden baslar ve taret ilk
// kimildayista SICRAR.
float kol(uint16_t ch_us, const Ayar& a);

// 🔴 USTEL EGRI:  cikis = (1-e)*x + e*x^3
// Dogrusal kolda merkezdeki 1 mm ile uctaki 1 mm ayni aciyi verir ve nisan
// alirken ince duzeltme yapilamaz. Egri merkez civarini yumusatir, uclarda
// tam hizi korur — "daha hassas ama yavas degil" ancak boyle olur.
float expo_uygula(float x, float e);

class Jog {
public:
    Jog(const Ayar& a, float baslangic_aci)
        : a_(a), pan_(baslangic_aci), tilt_(baslangic_aci) {}

    // Bir tikte acilari ilerletir. dt_ms gercek dongu periyodu.
    //
    // Sinirlar BURADA uygulanir, yalnizca cikista degil: birikimi serbest
    // birakip ciktida kirpmak, kolu ucta tutunca acinin sessizce buyumesine
    // ve geri donerken OLU BIR BOLGE olusmasina yol acardi.
    void guncelle(uint16_t pan_ch, uint16_t tilt_ch, uint32_t dt_ms);

    // Ust bilgisayar taretin sahibiyken jog birikimini onun acisiyla esitle.
    // Yoksa yetki geri alindiginda taret eski birikime siçrar.
    void esitle(float pan, float tilt) { pan_ = pan; tilt_ = tilt; }

    uint8_t pan_aci()  const { return (uint8_t)(pan_  + 0.5f); }
    uint8_t tilt_aci() const { return (uint8_t)(tilt_ + 0.5f); }
    float   pan_f()    const { return pan_; }
    float   tilt_f()   const { return tilt_; }

private:
    Ayar  a_;
    float pan_, tilt_;
};

}  // namespace taret
