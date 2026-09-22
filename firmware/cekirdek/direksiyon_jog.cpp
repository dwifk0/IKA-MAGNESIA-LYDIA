// SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
// Copyright (C) 2026 Ahmet Efe Nezli

#include "direksiyon_jog.h"

namespace direksiyon {

int32_t sapma_duzelt(int32_t ham, int32_t olu) {
    if (ham > -olu && ham < olu) return 0;
    return ham + ((ham > 0) ? -olu : olu);
}

int32_t hedef_hesapla(const JogAyar& a, int32_t sapma, int32_t yay_ucu,
                      int32_t konum, int32_t eski, uint32_t dt_ms) {
    int32_t hedef;

    if (sapma != 0) {
        // Kol sapmasi -> adim/saniye. Hedef her tikte o kadar ilerliyor.
        const int32_t hiz_hz = (yay_ucu != 0)
                             ? (sapma * a.jog_hz_max) / yay_ucu
                             : 0;
        hedef = eski + (hiz_hz * (int32_t)dt_ms) / 1000;
    } else if (a.merkez_hz > 0) {
        // MERKEZE DON. "Merkez" = kartin acildigi konum (0), cunku homing yok.
        //
        // Donus hedefi ADIM ADIM tasiniyor, dogrudan 0 YAZILMIYOR: 0 yazmak
        // hedefi konumun cok onune atar, asagidaki kelepce her tikte devreye
        // girer ve hareket tek hizda, rampasiz olur. Kademeli tasima ivme
        // rampasinin calismasini saglar.
        const int32_t adim = (a.merkez_hz * (int32_t)dt_ms) / 1000;
        if      (eski >  adim) hedef = eski - adim;
        else if (eski < -adim) hedef = eski + adim;
        else                   hedef = 0;

        // Merkez olu bandi: sifira dayanip surekli duzeltme yapmasin.
        // Adim gurultusu ve bosuna tutma akimi uretiyordu.
        if (hedef > -a.merkez_olu && hedef < a.merkez_olu) hedef = 0;
    } else {
        hedef = eski;   // merkeze donus kapali: bulundugu yerde kalir
    }

    // 🔴 Hedef, gercek konumun COK ONUNE gecmesin. Gecerse kol birakildiginda
    // motor biriken farki kapatmak icin donmeye devam eder — kullanicida
    // "birakinca durmuyor" hissi. Bu kelepce onu engelliyor.
    if (hedef - konum >  a.onde_max) hedef = konum + a.onde_max;
    if (hedef - konum < -a.onde_max) hedef = konum - a.onde_max;

    // Yumusak sinir. Homing olmadigi icin bu sinir MUTLAK degil, acilis
    // konumuna goredir.
    if (a.limit_adim > 0) {
        if (hedef >  a.limit_adim) hedef =  a.limit_adim;
        if (hedef < -a.limit_adim) hedef = -a.limit_adim;
    }
    return hedef;
}

int32_t guvenli_hiz(int32_t ivme_adim_s2, int32_t kalan_pay_adim) {
    if (kalan_pay_adim <= 0 || ivme_adim_s2 <= 0) return 0;
    // v = sqrt(2 * a * s) — tamsayi karekok (Newton), kayan nokta istemeden.
    uint64_t x = 2ULL * (uint64_t)ivme_adim_s2 * (uint64_t)kalan_pay_adim;
    if (x == 0) return 0;
    uint64_t k = x, onceki = 0;
    while (k != onceki) { onceki = k; k = (k + x / k) / 2; }
    return (int32_t)k;
}

}  // namespace direksiyon
