// SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
// Copyright (C) 2026 Ahmet Efe Nezli
#include "test.h"
#include "../cekirdek/direksiyon_jog.h"
using namespace direksiyon;

int main() {
    // ── olu bant disari tasiniyor, kirpilmiyor ──────────────────────────
    t::esit("bant icinde sifir",        0, sapma_duzelt(30, 50));
    t::esit("bant kenarinda sifir",     0, sapma_duzelt(50, 50));
    t::esit("bant disinda kaydirilmis", 1, sapma_duzelt(51, 50));
    t::esit("negatif tarafta da",      -1, sapma_duzelt(-51, 50));
    // Kirpilsaydi 51 -> 51 olurdu ve bant kenarinda cikti sifirdan 51'e
    // sicrardi. Tasiniyor, o yuzden sifirdan duzgun buyuyor.

    JogAyar a{};
    a.jog_hz_max = 10000; a.onde_max = 400;
    a.merkez_hz = 2000;   a.merkez_olu = 20; a.limit_adim = 20000;

    // ── jog: kol sapmasi hedefi ilerletiyor ─────────────────────────────
    {
        int32_t h = hedef_hesapla(a, 500, 500, 0, 0, 10);   // tam sapma, 10 ms
        t::esit("tam sapmada 10 ms'de 100 adim", 100, h);
    }

    // ── 🔴 hedef gercek konumun cok onune gecemez ───────────────────────
    // Gecseydi kol birakildiginda motor biriken farki kapatmak icin donmeye
    // devam ederdi: "birakinca durmuyor".
    {
        int32_t h = hedef_hesapla(a, 500, 500, 0, 5000, 10);
        t::esit("hedef onde_max ile kelepcelendi", 400, h);
    }

    // ── merkeze donus kademeli ──────────────────────────────────────────
    {
        int32_t h = hedef_hesapla(a, 0, 500, 1000, 1000, 10);
        t::esit("merkeze donus adim adim", 980, h);   // 2000 Hz * 10 ms = 20
        // Dogrudan 0 yazilsaydi hedef konumun cok onune atlardi, kelepce her
        // tikte devreye girer ve hareket rampasiz, tek hizda olurdu.
    }
    {
        int32_t h = hedef_hesapla(a, 0, 500, 5, 5, 10);
        t::esit("merkez olu bandinda tam sifir", 0, h);
    }
    {
        JogAyar b = a; b.merkez_hz = 0;
        int32_t h = hedef_hesapla(b, 0, 500, 1234, 1234, 10);
        t::esit("merkeze donus kapaliysa yerinde kalir", 1234, h);
    }

    // ── yumusak sinir ───────────────────────────────────────────────────
    {
        JogAyar b = a; b.limit_adim = 150; b.onde_max = 100000;
        int32_t h = hedef_hesapla(b, 500, 500, 0, 140, 100);
        t::esit("yumusak sinirda duruyor", 150, h);
    }

    // ── guvenli hiz: v = sqrt(2*a*s) ────────────────────────────────────
    t::esit("ivme 50000, pay 100 -> 3162", 3162, guvenli_hiz(50000, 100));
    t::esit("pay yoksa hiz yok",              0, guvenli_hiz(50000, 0));
    t::esit("pay negatifse hiz yok",          0, guvenli_hiz(50000, -5));

    // ⚠ GERILEME TESTI: x = 2*ivme*pay degeri n*n - 1 bicimini aldiginda
    // eski Newton durma olcutu iki deger arasinda salinip ASLA durmuyordu.
    // Asagidakilerin her biri o bicimde bir x uretiyor; donen deger
    // floor(sqrt(x)) olmali ve en onemlisi: fonksiyon DONMELI.
    t::esit("x=8  (3*3-1)   -> 2",   2, guvenli_hiz(1, 4));
    t::esit("x=24 (5*5-1)   -> 4",   4, guvenli_hiz(1, 12));
    t::esit("x=48 (7*7-1)   -> 6",   6, guvenli_hiz(1, 24));
    t::esit("x=80 (9*9-1)   -> 8",   8, guvenli_hiz(1, 40));
    t::esit("x=120 (11*11-1)-> 10", 10, guvenli_hiz(1, 60));
    t::esit("x=168 (13*13-1)-> 12", 12, guvenli_hiz(1, 84));
    // Tam kareler ve en kucuk girdiler de dogru kalsin.
    // (x = 2*ivme*pay her zaman CIFT'tir, tam kareler bu yuzden cift kareler.)
    t::esit("x=4  tam kare  -> 2",   2, guvenli_hiz(1, 2));
    t::esit("x=16 tam kare  -> 4",   4, guvenli_hiz(1, 8));
    t::esit("x=36 tam kare  -> 6",   6, guvenli_hiz(1, 18));
    t::esit("x=2  en kucuk  -> 1",   1, guvenli_hiz(1, 1));
    // Bu degeri asan bir hiz, sinira carpmadan duramaz.
    return t::rapor("direksiyon");
}
