// SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
// Copyright (C) 2026 Ahmet Efe Nezli
#include "test.h"
#include "../cekirdek/cerceve.h"
#include <initializer_list>
using namespace cerceve;

int main() {
    // ── kur/coz gidis donus ──────────────────────────────────────────────
    uint8_t b[BOY];
    kur({0x31, -1500, 4200}, b);
    t::esit("bas bayti", 0xAA, b[0]);
    t::esit("son bayti", 0x55, b[7]);
    t::esit("v0 buyuk sonlu yuksek bayt", 0xFA, b[2]);  // -1500 = 0xFA24
    t::esit("v0 buyuk sonlu dusuk bayt",  0x24, b[3]);

    Paket p{};
    t::dogru("gecerli cerceve cozuluyor", coz(b, p));
    t::esit("tip korunuyor", 0x31, p.tip);
    t::esit("negatif v0 korunuyor", -1500, p.v0);
    t::esit("v1 korunuyor", 4200, p.v1);

    // ── bozuk cerceveler ────────────────────────────────────────────────
    uint8_t x[BOY]; kur({0x30, 1, 2}, x);
    x[6] ^= 0xFF;
    t::yanlis("XOR bozuksa reddediliyor", coz(x, p));

    kur({0x30, 1, 2}, x); x[7] = 0x00;
    t::yanlis("bitis bayti yanlissa reddediliyor", coz(x, p));

    kur({0x30, 1, 2}, x); x[0] = 0x00;
    t::yanlis("bas bayti yanlissa reddediliyor", coz(x, p));

    // ── toplayici: temiz akis ───────────────────────────────────────────
    {
        Toplayici tp; Paket c{};
        kur({0x39, 2, 0x0F}, b);
        bool tam = false;
        for (uint8_t i = 0; i < BOY; i++) tam = tp.bayt(b[i], c);
        t::dogru("tam cerceve yakalandi", tam);
        t::esit("toplayici tipi cozdu", 0x39, c.tip);
        t::esit("paket sayaci", 1, (long long)tp.paket_sayisi());
        t::yanlis("cerceve bitti, surmuyor", tp.suruyor());
    }

    // ── toplayici: onunde cop bayt ──────────────────────────────────────
    {
        Toplayici tp; Paket c{};
        for (uint8_t g : {0x00, 0x7F, 0x13}) tp.bayt(g, c);
        t::esit("cop baytlar senkron kurmadi", 0, (long long)tp.bas_sayisi());
        kur({0x35, 0, 7}, b);
        bool tam = false;
        for (uint8_t i = 0; i < BOY; i++) tam = tp.bayt(b[i], c);
        t::dogru("copten sonra cerceve yakalandi", tam);
    }

    // ── toplayici: kayip bayt senkronu kaydirabilir ─────────────────────
    // Bir bayt dusunce toplayici bir sonraki 0xAA'yi arar. Veri baytlarindan
    // biri 0xAA ise, onu BAS sanip cercevenin ORTASINDAN senkron kurar ve hat
    // sessizce olur — hata da vermez. birak() tam olarak bunun icin var.
    {
        Toplayici tp; Paket c{};
        kur({0x31, (int16_t)0xAA00, 20}, b);   // v0'in yuksek bayti 0xAA
        for (uint8_t i = 1; i < BOY; i++) tp.bayt(b[i], c);   // ilk bayt dustu
        t::dogru("veri icindeki 0xAA yanlis senkron kurdu", tp.suruyor());

        bool tam = false;
        for (uint8_t i = 0; i < BOY; i++) tam = tp.bayt(b[i], c);
        t::yanlis("kaymis senkronla cerceve yakalanamiyor", tam);

        tp.birak();
        for (uint8_t i = 0; i < BOY; i++) tam = tp.bayt(b[i], c);
        t::dogru("birak() sonrasi senkron geri geliyor", tam);
    }

    // ── veri 0xAA icermiyorsa kayip bayt kendiliginden toparlaniyor ─────
    // Yani senkron kaybi her zaman kalici DEGIL; kaliciligi verinin icerigi
    // belirliyor. Zaman asimi bu yuzden sansa birakilamaz.
    {
        Toplayici tp; Paket c{};
        kur({0x31, 10, 20}, b);                // hicbir bayti 0xAA degil
        for (uint8_t i = 1; i < BOY; i++) tp.bayt(b[i], c);
        bool tam = false;
        for (uint8_t i = 0; i < BOY; i++) tam = tp.bayt(b[i], c);
        t::dogru("0xAA'siz veride kendiliginden toparlandi", tam);
    }

    // ── teshis sayaclari sebebi ayirt ediyor ────────────────────────────
    {
        Toplayici tp; Paket c{};
        for (int i = 0; i < 40; i++) tp.bayt(0x11, c);        // protokol disi veri
        t::dogru("bayt sayaci artiyor", tp.bayt_sayisi() == 40);
        t::esit("bas hic yakalanmadi -> baud suphesi", 0, (long long)tp.bas_sayisi());
    }
    return t::rapor("cerceve");
}
