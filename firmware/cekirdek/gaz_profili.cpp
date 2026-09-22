// SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
// Copyright (C) 2026 Ahmet Efe Nezli

#include "gaz_profili.h"

namespace gaz {

float Tavan::guncelle(uint16_t ham) {
    // Kanal makul araligin disindaysa TAM GUC. Bkz. baslikta 🔴.
    if (ham < a_.gecerli_alt || ham > a_.gecerli_ust) return a_.tam_oran;

    const uint16_t fark = (ham > ham_son_) ? (uint16_t)(ham - ham_son_)
                                           : (uint16_t)(ham_son_ - ham);
    if (ham_son_ == 0 || fark > a_.olu_us) ham_son_ = ham;

    const uint16_t alt = (uint16_t)(a_.merkez_us - a_.yay_ucu_us);
    uint16_t k = ham_son_;
    if (k < alt) k = alt;

    float u = (float)(k - alt) / (float)(2 * a_.yay_ucu_us);   // 0..1
    if (u > 1.0f) u = 1.0f;

    const float hedef = a_.taban_oran + u * (a_.tam_oran - a_.taban_oran);
    suzulmus_ += a_.suzgec * (hedef - suzulmus_);
    return suzulmus_;
}

float Otonom::volt(float hiz, uint32_t simdi) {
    if (hiz < 0.01f) { hareket_ = false; return a_.rolanti_v; }

    if (hiz < a_.hiz_min)   hiz = a_.hiz_min;
    if (hiz > a_.hiz_tavan) hiz = a_.hiz_tavan;

    // Kalkis darbesi: komut sifirdan pozitife gectigi anda baslar.
    if (!hareket_) {
        hareket_ = true;
        kalkis_bitis_ = simdi + a_.kalkis_ms;
    }
    if (simdi < kalkis_bitis_) return a_.kalkis_v;

    return a_.taban_v + (hiz - a_.hiz_min) * a_.egim_v_per_ms;
}

}  // namespace gaz
