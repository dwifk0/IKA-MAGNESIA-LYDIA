// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Ahmet Efe Nezli
#include "test.h"
#include "../cekirdek/ibus.h"
#include <vector>
using namespace ibus;

// Gecerli bir iBUS cercevesi uret.
static std::vector<uint8_t> cerceve_uret(const uint16_t* k) {
    std::vector<uint8_t> f(BOY);
    f[0] = BAS0; f[1] = BAS1;
    for (int i = 0; i < KANAL; i++) {
        f[2 + 2*i] = (uint8_t)(k[i] & 0xFF);
        f[3 + 2*i] = (uint8_t)(k[i] >> 8);
    }
    uint16_t t = 0; for (int i = 0; i < 30; i++) t += f[i];
    uint16_t s = (uint16_t)(0xFFFF - t);
    f[30] = (uint8_t)(s & 0xFF); f[31] = (uint8_t)(s >> 8);
    return f;
}

int main() {
    uint16_t k[KANAL];
    for (int i = 0; i < KANAL; i++) k[i] = 1500;
    k[0] = 1000; k[2] = 1875;

    // ── guvenli acilis degerleri ────────────────────────────────────────
    {
        Cozucu c;
        // kesme ch4 -> KESIK 1000 ; fren ch5 -> serbest 1000 ; mod ch6 -> manuel 1000
        c.kur(4, 1000, 5, 1000, 6, 1000);
        t::esit("kesme kanali KESIK tarafinda basliyor", 1000, c.oku(4));
        t::esit("mod kanali MANUEL ucunda basliyor",     1000, c.oku(6));
        t::esit("diger kanallar merkezde",               1500, c.oku(1));
        // 🔴 Mod kanali merkezde birakilsaydi 1500 = ORTA kademe olurdu ve
        // sistem ilk cerceve gelmeden kendini yari otonom sanardi.
        t::yanlis("hic cerceve gelmedi", c.hic_cerceve_geldi());
    }

    // ── temiz cerceve ───────────────────────────────────────────────────
    {
        Cozucu c; c.kur(4, 1000, 5, 1000, 6, 1000);
        auto f = cerceve_uret(k);
        bool tam = false;
        for (uint8_t b : f) tam = c.bayt(b);
        t::dogru("gecerli cerceve kabul edildi", tam);
        t::esit("kanal 0 cozuldu", 1000, c.oku(0));
        t::esit("kanal 2 cozuldu", 1875, c.oku(2));
        t::dogru("cerceve geldi bayragi", c.hic_cerceve_geldi());
        t::esit("saglam sayaci", 1, (long long)c.saglam());
    }

    // ── saglama tutmuyorsa ──────────────────────────────────────────────
    {
        Cozucu c; c.kur(4, 1000, 5, 1000, 6, 1000);
        auto f = cerceve_uret(k);
        f[10] ^= 0x01;                       // veri bozuldu, saglama eski
        bool tam = false;
        for (uint8_t b : f) tam = c.bayt(b);
        t::yanlis("saglama tutmayan cerceve reddedildi", tam);
        t::esit("bozuk sayaci artti", 1, (long long)c.bozuk());
        t::esit("bozuk cerceve kanallari EZMEDI", 1500, c.oku(0));
    }

    // ── yeniden senkron: 0x20 0x20 0x40 ... ─────────────────────────────
    // Ikinci bayt tutmadiginda elimizdeki bayt yeni bir cercevenin BASI
    // olabilir; atilirsa her gurultude bir cerceve daha kaybedilir.
    {
        Cozucu c; c.kur(4, 1000, 5, 1000, 6, 1000);
        auto f = cerceve_uret(k);
        c.bayt(0x20);                        // sahte bas
        bool tam = false;
        for (uint8_t b : f) tam = c.bayt(b); // hemen ardindan gercek cerceve
        t::dogru("sahte bastan sonra cerceve kurtarildi", tam);
    }

    // ── Hz penceresi ────────────────────────────────────────────────────
    // Kumulatif sayac bir kopmayi gizler, Hz gizlemez.
    {
        Cozucu c; c.kur(4, 1000, 5, 1000, 6, 1000);
        auto f = cerceve_uret(k);
        for (int n = 0; n < 5; n++) for (uint8_t b : f) c.bayt(b);
        c.pencere_kapat();
        t::esit("pencerede 5 cerceve", 5, (long long)c.hz());
        c.pencere_kapat();
        t::esit("kopma Hz'de hemen gorunur", 0, (long long)c.hz());
        t::esit("kumulatif sayac ise hala 5 -> kopmayi gizler",
                5, (long long)c.saglam());
    }
    return t::rapor("ibus");
}
