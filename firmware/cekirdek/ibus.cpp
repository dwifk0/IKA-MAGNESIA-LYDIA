// SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
// Copyright (C) 2026 Ahmet Efe Nezli

#include "ibus.h"

namespace ibus {

void Cozucu::kur(uint8_t kesme_kanali, uint16_t kesme_kesik_us,
                 uint8_t fren_kanali,  uint16_t fren_serbest_us,
                 uint8_t mod_kanali,   uint16_t mod_manuel_us) {
    for (uint8_t i = 0; i < KANAL; i++) kanal_[i] = MERKEZ;
    if (kesme_kanali < KANAL) kanal_[kesme_kanali] = kesme_kesik_us;
    if (fren_kanali  < KANAL) kanal_[fren_kanali]  = fren_serbest_us;
    if (mod_kanali   < KANAL) kanal_[mod_kanali]   = mod_manuel_us;
}

bool Cozucu::cerceve_tamamla() {
    uint16_t toplam = 0;
    for (uint8_t i = 0; i < 30; i++) toplam += ara_[i];
    const uint16_t gelen = (uint16_t)ara_[30] | ((uint16_t)ara_[31] << 8);
    if ((uint16_t)(0xFFFF - toplam) != gelen) { bozuk_++; return false; }

    saglam_++;
    pencere_++;
    for (uint8_t i = 0; i < KANAL; i++)
        kanal_[i] = (uint16_t)ara_[2 + 2 * i] | ((uint16_t)ara_[3 + 2 * i] << 8);
    return true;
}

bool Cozucu::bayt(uint8_t b) {
    bayt_++;
    if (b == BAS0) bas20_++;

    if (n_ == 0) {
        if (b != BAS0) return false;
    } else if (n_ == 1) {
        if (b != BAS1) {
            // Ikinci bayt tutmadi. Ama elimizdeki bayt yeni bir cercevenin
            // BASI olabilir — atmak yerine yeniden senkron kuruyoruz. Bu
            // olmadan, gurultu her seferinde bir cerceve daha kaybettirir.
            n_ = 0;
            if (b == BAS0) { ara_[0] = b; n_ = 1; }
            return false;
        }
    }

    ara_[n_++] = b;
    if (n_ < BOY) return false;

    const bool ok = cerceve_tamamla();
    n_ = 0;
    return ok;
}

void Cozucu::pencere_kapat() {
    hz_ = pencere_;
    pencere_ = 0;
}

}  // namespace ibus
