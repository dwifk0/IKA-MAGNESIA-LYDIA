// SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
// Copyright (C) 2026 Ahmet Efe Nezli
//
// KIP HAKEMI — surus yetkisinin kimde oldugunu belirleyen karar katmani.
//
// Tek cumlelik kural:   GUVENLIK  >  KUMANDA  >  UST BILGISAYAR
// Otonom kip bu siranin ustune CIKAMAZ. Guvenlik kumesi doluyken ust
// bilgisayarin hicbir komutu uygulanmaz.
//
// Bu dosyada donanim yok: anahtar konumlari, canlilik ve zaman disaridan
// geliyor. Aktuator surulmuyor, yalnizca KARAR uretiliyor.

#pragma once
#include <stdint.h>

namespace kip {

enum Kip : uint8_t { MANUEL = 0, BOS = 1, OTONOM = 2 };

// Uc kademeli anahtarin (SwC) ham us degerini kipe cevirir.
//
// ⚠ Bu fonksiyon guvenlige BAKMAZ, yalnizca anahtari okur. Kesme, sinyal
// yoklugu ve acil stop kararlari kipin USTUNDEDIR.
Kip oku(uint16_t ch_us, bool ters, uint16_t esik_orta, uint16_t esik_ust,
        bool ust_bilgisayar_etkin);

// Hakemin girdileri — hepsi o anki olculmus durum.
struct Girdi {
    Kip      kip;
    bool     kesme;            // SwA kesme konumunda
    bool     rc_sessiz;        // kumanda cercevesi gelmiyor (KOPUK KABLO dahil)
    bool     estop_basili;
    bool     gaz_ariza_kilit;  // bkz. emniyet::AriziKilidi
    bool     taret_anahtari;   // SwB taret konumunda
    bool     ust_canli;        // son paket zaman asimi penceresi icinde
    bool     ust_estop;        // ust bilgisayar E-STOP ilan etti
    bool     ust_dur;          // son komut "acil dur", taze komut bekleniyor
};

// Hakemin ciktisi — asagidaki her aktuator blogu yalnizca bunlara bakar.
struct Karar {
    // Gercekten tehlikeli durum: gaz rolantiye VE fren basar.
    bool guvenlik;

    // Gaz kesilir ama fren KARISMAZ.
    //
    // ⚠ "Gazi kes" ile "freni bas" ayni sey degil. Taret kipinde arac zaten
    // duruyor ve fren aktuatoru vidali oldugu icin konumunu koruyor; ustune
    // tam fren basmak gereksiz zorlama olurdu.
    bool rolantiye;

    // Gaz yetkisi ust bilgisayarda mi? Yalniz TAM otonomda devredilir.
    //
    // YARI kipte (BOS kademesi degil, otonom-yari kullanimi) gaz ve fren
    // insanda kalir: ust bilgisayar direksiyonu surer, insan gazi ve freni
    // elinde tutar ve yanlis giden seyi BIRAKARAK durdurur.
    bool gaz_ust_bilgisayarda;

    // Taret kumandayla mi suruluyor?
    bool elle_taret;

    // Ust bilgisayar yok sayilacak mi? Uc ayri sebep AYNI cevabi doguruyor:
    // link oldu · E-STOP ilan etti · son komut "acil dur"du.
    bool ust_yok;
};

Karar karar_ver(const Girdi& g, bool taret_var, bool taret_rc_var);

// ─────────────────────────────────────────────────────────────────────────
// KIP GECISI
//
// Kipe GIRERKEN saklanan komutlar sifirlanir. Yoksa anahtari otonoma atmak,
// dakikalar once gelmis BAYAT bir hiz komutunu uygular ve arac kendiliginden
// kalkar. Hareket icin her zaman TAZE komut sart.
// ─────────────────────────────────────────────────────────────────────────
struct GecisIstegi {
    bool degisti;
    bool komut_sifirla;      // saklanan ust bilgisayar komutlarini sil
    bool referans_sifirla;   // direksiyon ve yonelim referansini burada kur
};

class GecisIzleyici {
public:
    GecisIstegi guncelle(Kip yeni);
    Kip suanki() const { return suanki_; }
private:
    Kip  suanki_ = MANUEL;
    bool ilk_ = true;
};

}  // namespace kip
