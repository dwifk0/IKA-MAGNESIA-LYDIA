// SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
// Copyright (C) 2026 Ahmet Efe Nezli
//
// Emniyet mantigi — acil stop izleme, ariza kilidi, aktuator stall mandali.
//
// Uc desen de ayni fikri tasiyor: BIR EMNIYET KARARI, KENDISINI URETEN
// SINYALDEN DAHA KARARLI OLMALIDIR. Cirpinan bir girdi dogrudan aktuatore
// baglanirsa, aktuator de cirpinir.
//
// Donanimdan bagimsiz: pin okuma ve zaman cagiran tarafta.

#pragma once
#include <stdint.h>

namespace emniyet {

// ─────────────────────────────────────────────────────────────────────────
// ACIL STOP IZLEME
//
// ⚠ Bu sinif acil stopu UYGULAMAZ. Kesme butonun kendi kontagindadir ve ana
// motor beslemesini dogrudan keser; kart donsa, resetlense ya da USB'si
// cekilse bile motor durur. Burada yalnizca DURUM OKUNUR.
//
// Iki kanal varsa ikisi de izlenir ve uyusmazlik raporlanir. Uyusmazlik
// aninda degil, SURE esigi asilinca ilan edilir — kontak sicramasi bir
// uyusmazlik degildir.
// ─────────────────────────────────────────────────────────────────────────
class AcilStop {
public:
    // b_kanali_var == false ise tek kanal kipinde calisir.
    //
    // 🔴 Bagli OLMAYAN bir pini "ikinci kanal" saymak, olmayan bir emniyeti
    // var sanmaktan DAHA KOTUDUR: pin surekli basili okur ve uyusmazlik
    // bayragi hep yanar, operator de bir sure sonra ona bakmayi birakir.
    explicit AcilStop(bool b_kanali_var, uint32_t uyusmazlik_ms = 100)
        : b_var_(b_kanali_var), uyusmazlik_ms_(uyusmazlik_ms) {}

    // Kanal durumlarini ve o anki zamani verir. Tek kanal kipinde `b`
    // yok sayilir.
    void guncelle(bool a, bool b, uint32_t simdi_ms);

    bool basili() const { return basili_; }
    bool uyusmazlik() const { return uyusmazlik_; }

private:
    bool     b_var_;
    uint32_t uyusmazlik_ms_;
    bool     basili_ = false;
    bool     uyusmazlik_ = false;
    uint32_t fark_bas_ = 0;
};

// ─────────────────────────────────────────────────────────────────────────
// ARIZA KILIDI
//
// Bir saglik bayragi tek basarili islemde kendiliginden temizlenebiliyorsa,
// hat sinirdayken bayrak CIRPINIR. Cirpinan bayragi dogrudan frene baglamak,
// hareket halindeki araclarda freni basip birakmak demektir.
//
// Cozum: ariza KILITLENIR. Kilidi zamanin gecmesi degil, OPERATORUN acik bir
// hareketi cozer — ve ariza hala suruyorsa kilit hemen geri gelir.
// ─────────────────────────────────────────────────────────────────────────
class AriziKilidi {
public:
    // saglikli == false gorulur gorulmez kilitlenir.
    void bildir(bool saglikli) { if (!saglikli) kilit_ = true; }

    // Operator acik hareketi yapti (ornegin kumanda anahtarini KES konumuna
    // alip geri getirdi). Ariza hala suruyorsa kilit acilmaz.
    void kullanici_sifirla(bool su_an_saglikli) { if (su_an_saglikli) kilit_ = false; }

    bool kilitli() const { return kilit_; }

private:
    bool kilit_ = false;
};

// ─────────────────────────────────────────────────────────────────────────
// STALL MANDALI  (fren aktuatorü)
//
// Fren aktuatorunde konum sensoru ve akim olcumu YOK. Koruma zamana dayaniyor:
// ayni yonde kesintisiz `esik_ms` surulduyse surus kesilir.
//
// Fren BIRAKILMAZ — aktuator vidali, konumunu korur; yalnizca motor
// beslemesi kesilir.
//
// 🔴 EN ONEMLI DAVRANIS: mandali yon DEGISIMI degil, YON ALANININ DEGISMESI
// cozer. `0` da bir yondur — komutu bir tik `0`'a cekmek yeterlidir, ters
// yone komut vermek GEREKMEZ. Ust katman surekli ayni yonde komut bastigi
// icin mandalin hic acilmadigi, sahada saatlerce arandi.
// ─────────────────────────────────────────────────────────────────────────
class StallMandali {
public:
    // olu_bolge: altinda hicbir hareket uretmeyen komut buyuklugu [binde]
    StallMandali(uint32_t esik_ms, int16_t olu_bolge)
        : esik_ms_(esik_ms), olu_bolge_(olu_bolge) {}

    // Komutu [binde, -1000..1000] ve o anki zamani verir.
    // Aktuatore uygulanacak degeri dondurur: mandal kapaliyken 0.
    int16_t uygula(int16_t binde, uint32_t simdi_ms);

    bool mandalli() const { return mandal_; }
    int8_t yon() const { return yon_; }

private:
    uint32_t esik_ms_;
    int16_t  olu_bolge_;
    int8_t   yon_ = 0;
    uint32_t yon_bas_ = 0;
    bool     mandal_ = false;
};

}  // namespace emniyet
