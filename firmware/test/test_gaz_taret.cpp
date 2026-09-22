// SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
// Copyright (C) 2026 Ahmet Efe Nezli
#include "test.h"
#include "../cekirdek/gaz_profili.h"
#include "../cekirdek/taret_jog.h"
#include <cmath>

static bool yakin(float a, float b, float t = 0.001f) { return std::fabs(a-b) < t; }

int main() {
    // ── gaz tavani ──────────────────────────────────────────────────────
    gaz::TavanAyar ta{};
    ta.gecerli_alt = 900; ta.gecerli_ust = 2100;
    ta.olu_us = 10; ta.merkez_us = 1500; ta.yay_ucu_us = 500;
    ta.taban_oran = 0.30f; ta.tam_oran = 1.00f; ta.suzgec = 1.0f;  // suzgec kapali
    {
        gaz::Tavan t1(ta);
        t::dogru("pot en kisikta taban orani", yakin(t1.guncelle(1000), 0.30f));
        gaz::Tavan t2(ta);
        t::dogru("pot en acikta tam oran",     yakin(t2.guncelle(2000), 1.00f));
        gaz::Tavan t3(ta);
        t::dogru("pot ortada yaklasik yari",   yakin(t3.guncelle(1500), 0.65f));

        // 🔴 Kanal gecersizse TAM GUC — sifir degil.
        gaz::Tavan t4(ta);
        t::dogru("kanal olu ise tam guce donuluyor", yakin(t4.guncelle(0), 1.00f));
        t::dogru("kanal tasmissa da tam guc",        yakin(t4.guncelle(5000), 1.00f));
    }
    {
        // Olu bant: kucuk titremeler ciktiyi oynatmiyor.
        gaz::TavanAyar b = ta; b.olu_us = 50;
        gaz::Tavan t1(b);
        const float ilk = t1.guncelle(1500);
        t::dogru("olu bant icindeki titreme yok sayildi",
                 yakin(t1.guncelle(1530), ilk));
        t::yanlis("bant disi degisim gecerli",
                  yakin(t1.guncelle(1700), ilk));
    }
    {
        // Suzgec: hedefe bir adimda degil, kademeli gidiyor.
        gaz::TavanAyar b = ta; b.suzgec = 0.2f;
        gaz::Tavan t1(b);
        const float a1 = t1.guncelle(1000);
        t::dogru("ilk adim hedefe varmiyor", a1 > 0.30f && a1 < 1.00f);
        for (int i = 0; i < 60; i++) t1.guncelle(1000);
        t::dogru("tekrar edince hedefe yaklasiyor", yakin(t1.oran(), 0.30f, 0.01f));
    }

    // ── otonom gaz ──────────────────────────────────────────────────────
    gaz::OtonomAyar oa{};
    oa.rolanti_v = 0.80f; oa.kalkis_v = 2.00f; oa.kalkis_ms = 300;
    oa.hiz_min = 0.30f; oa.hiz_tavan = 1.50f;
    oa.taban_v = 1.10f; oa.egim_v_per_ms = 0.50f;
    {
        gaz::Otonom o(oa);
        t::dogru("sifir hiz rolanti",  yakin(o.volt(0.0f, 0), 0.80f));
        t::dogru("kalkis darbesi",     yakin(o.volt(1.0f, 10), 2.00f));
        t::dogru("darbe suresince sabit", yakin(o.volt(1.0f, 200), 2.00f));
        t::dogru("darbe bitince dogruya gecer",
                 yakin(o.volt(1.0f, 400), 1.10f + 0.70f * 0.50f));
        t::dogru("tavan kirpiliyor",
                 yakin(o.volt(9.0f, 500), 1.10f + (1.50f-0.30f) * 0.50f));

        // Durusta darbe yeniden hak ediliyor.
        o.volt(0.0f, 600);
        t::dogru("durduktan sonra darbe geri geldi", yakin(o.volt(1.0f, 610), 2.00f));
    }

    // ── taret: expo egrisi ──────────────────────────────────────────────
    t::dogru("expo 0 dogrusal", yakin(taret::expo_uygula(0.5f, 0.0f), 0.5f));
    t::dogru("expo 1 kubik",    yakin(taret::expo_uygula(0.5f, 1.0f), 0.125f));
    t::dogru("uclarda tam hiz korunuyor", yakin(taret::expo_uygula(1.0f, 1.0f), 1.0f));
    t::dogru("merkez civari yumusadi",
             taret::expo_uygula(0.3f, 0.7f) < 0.3f);

    // ── taret: olu bant sonrasi yeniden olcekleme ───────────────────────
    taret::Ayar a{};
    a.merkez_us = 1500; a.yay_ucu_us = 500; a.olu_bant_us = 50;
    a.dps = 60.0f; a.expo = 0.0f;
    a.pan_min = 0; a.pan_max = 180; a.tilt_min = 0; a.tilt_max = 180;
    t::dogru("bant icinde sifir",  yakin(taret::kol(1520, a), 0.0f));
    t::dogru("tam sapmada 1",      yakin(taret::kol(2000, a), 1.0f));
    t::dogru("bandin hemen disinda sifira yakin",
             taret::kol(1551, a) > 0.0f && taret::kol(1551, a) < 0.01f);
    // Yeniden olceklenmeseydi burada 0,1 civari bir deger olurdu ve taret
    // ilk kimildayista SICRARDI.

    // ── 🔴 float birikim: tamsayida taret HIC kimildamazdi ──────────────
    {
        taret::Jog j(a, 90.0f);
        j.guncelle(2000, 1500, 10);        // tam sapma, 10 ms -> 0,6 derece
        t::dogru("tek tikte 0,6 derece birikti", yakin(j.pan_f(), 90.6f, 0.01f));
        t::esit("yuvarlanmis aci", 91, j.pan_aci());
        for (int i = 0; i < 9; i++) j.guncelle(2000, 1500, 10);
        t::dogru("10 tikte 6 derece", yakin(j.pan_f(), 96.0f, 0.05f));
    }
    // ── sinirlar birikimin UZERINDE uygulaniyor ─────────────────────────
    {
        taret::Ayar b = a; b.pan_max = 100;
        taret::Jog j(b, 90.0f);
        for (int i = 0; i < 100; i++) j.guncelle(2000, 1500, 10);
        t::dogru("sinirda duruyor", yakin(j.pan_f(), 100.0f));
        for (int i = 0; i < 5; i++) j.guncelle(1000, 1500, 10);
        t::dogru("geri donuste OLU BOLGE yok", j.pan_f() < 100.0f);
        // Birikim serbest biraksaydi aci sessizce 180'e cikar, geri donerken
        // 80 derecelik olu bir bolge olusurdu.
    }
    return t::rapor("gaz + taret");
}
