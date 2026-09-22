// SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
// Copyright (C) 2026 Ahmet Efe Nezli
//
// GAZ PROFILI — kumanda kolundan ve otonom hiz komutundan gaz gerilimine.
//
// Gaz ACIK DONGU: kart hiz geri beslemesiyle duzeltme yapmaz, yalnizca
// istenen hizi gerilime cevirir. Kapali dongu isteniyorsa ust katmanda
// kurulmali — kart gercek hizi zaten telemetride veriyor.
//
// Buradaki sayilar araca ozeldir ve bu depoda BOS birakilmistir; hepsi
// disaridan parametre geliyor. Nasil olculdukleri: ../../docs/KALIBRASYON.md

#pragma once
#include <stdint.h>

namespace gaz {

// ─────────────────────────────────────────────────────────────────────────
// MANUEL GAZ TAVANI — potansiyometreli guc siniri
//
// Operator bir potla manuel gazin tavanini canli olarak kisabiliyor.
// Iki koruma var, ikisi de bilerek:
//   1. Olu bant — pot mekanik olarak titrer; her mikrosaniye degisimini
//      cikisa gecirmek gazi kararsiz yapar.
//   2. Alcak gecirgen suzgec — potu hizli cevirmek gazda basamak uretmesin.
//
// 🔴 Kanal saglam DEGILSE TAM GUCE donulur, sifira DEGIL. Bir knob arizasinin
// cevabi "arac yavaslasin" olamaz; bilinen eski davranis olmali. Aksi halde
// ariza, sahada aracin sebepsiz gucsuzlesmesi olarak gorunur ve aranmasi
// gereken yer hic akla gelmez.
// ─────────────────────────────────────────────────────────────────────────
struct TavanAyar {
    uint16_t gecerli_alt, gecerli_ust;   // kanalin makul araligi [us]
    uint16_t olu_us;                     // altinda guncelleme yapilmayan kayma
    uint16_t merkez_us, yay_ucu_us;
    float    taban_oran;                 // potun en kisik ucundaki oran
    float    tam_oran;                   // potun en acik ucundaki oran
    float    suzgec;                     // 0..1, buyudukce hizli
};

class Tavan {
public:
    explicit Tavan(const TavanAyar& a) : a_(a), suzulmus_(a.tam_oran) {}
    float guncelle(uint16_t ham_us);
    float oran() const { return suzulmus_; }
private:
    TavanAyar a_;
    float     suzulmus_;
    uint16_t  ham_son_ = 0;
};

// ─────────────────────────────────────────────────────────────────────────
// OTONOM GAZ — m/s'den volta
//
// Manuel daldaki "kol orani" burada ise yaramaz: otonom komut gercek m/s
// tasiyor ve gerilime sahada olculmus bir dogruyla cevriliyor.
// ─────────────────────────────────────────────────────────────────────────
struct OtonomAyar {
    float    rolanti_v;      // araci hareket ettirmeyen taban gerilim
    float    kalkis_v;       // duristan kalkis darbesinin gerilimi
    uint32_t kalkis_ms;      // darbenin suresi
    float    hiz_min, hiz_tavan;   // m/s
    float    egim_v_per_ms;  // hiz -> gerilim egimi
    float    taban_v;        // hiz_min'in karsiligi
};

class Otonom {
public:
    explicit Otonom(const OtonomAyar& a) : a_(a) {}

    // Istenen hizi [m/s] gerilime cevirir.
    //
    // Duristan kalkis darbesi, komut sifirdan pozitife gectigi AN baslar:
    // statik surtunmeyi yenmek icin kisa sureli yuksek gerilim. Darbe her
    // duruşta yeniden hak edilir.
    float volt(float hiz_ms, uint32_t simdi_ms);

    // Kip degisiminde cagrilir: kalkis darbesi yeniden hak edilsin.
    void sifirla() { hareket_ = false; }

private:
    OtonomAyar a_;
    bool       hareket_ = false;
    uint32_t   kalkis_bitis_ = 0;
};

}  // namespace gaz
