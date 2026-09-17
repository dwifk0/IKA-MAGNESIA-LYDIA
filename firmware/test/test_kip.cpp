// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Ahmet Efe Nezli
#include "test.h"
#include "../cekirdek/kip_hakemi.h"
using namespace kip;

static Girdi temiz() {
    Girdi g{};
    g.kip = MANUEL;
    g.ust_canli = true;
    return g;   // diger her sey false
}

int main() {
    // ── anahtar okuma ───────────────────────────────────────────────────
    t::esit("alt kademe manuel",  MANUEL, oku(1000, false, 1400, 1700, true));
    t::esit("orta kademe bos",    BOS,    oku(1500, false, 1400, 1700, true));
    t::esit("ust kademe otonom",  OTONOM, oku(2000, false, 1400, 1700, true));
    t::esit("ters bayrak aynalar", MANUEL, oku(2000, true, 1400, 1700, true));
    t::esit("ust bilgisayar kapaliysa hep manuel",
            MANUEL, oku(2000, false, 1400, 1700, false));

    // ── 🔴 GUVENLIK > KUMANDA > UST BILGISAYAR ──────────────────────────
    {
        Girdi g = temiz(); g.kip = OTONOM;
        Karar k = karar_ver(g, true, true);
        t::dogru("otonomda gaz ust bilgisayara gecti", k.gaz_ust_bilgisayarda);

        g.estop_basili = true;
        k = karar_ver(g, true, true);
        t::dogru("acil stop guvenlik kumesini doldurur", k.guvenlik);
        t::yanlis("otonom kip guvenligin ustune CIKAMAZ", k.gaz_ust_bilgisayarda);
    }
    // Her guvenlik sebebi tek basina yeter.
    for (int i = 0; i < 5; i++) {
        Girdi g = temiz(); g.kip = OTONOM;
        switch (i) {
            case 0: g.kesme = true; break;
            case 1: g.rc_sessiz = true; break;      // KOPUK KABLO dahil
            case 2: g.estop_basili = true; break;
            case 3: g.kip = BOS; break;             // yumusak acil stop
            case 4: g.gaz_ariza_kilit = true; break;
        }
        Karar k = karar_ver(g, true, true);
        t::dogru("tek sebep guvenlik icin yeterli", k.guvenlik);
        t::yanlis("guvenlikte gaz devredilmez", k.gaz_ust_bilgisayarda);
    }

    // ── "gazi kes" ile "freni bas" ayni sey degil ───────────────────────
    {
        Girdi g = temiz(); g.taret_anahtari = true;
        Karar k = karar_ver(g, true, true);
        t::dogru("taret kipinde gaz kesilir", k.rolantiye);
        t::yanlis("ama fren KARISMAZ", k.guvenlik);
    }

    // ── ust bilgisayar yoklugu: uc sebep ayni cevap ─────────────────────
    for (int i = 0; i < 3; i++) {
        Girdi g = temiz(); g.kip = OTONOM;
        if (i == 0) g.ust_canli = false;
        if (i == 1) g.ust_estop = true;
        if (i == 2) g.ust_dur   = true;
        Karar k = karar_ver(g, true, true);
        t::dogru("link/estop/dur -> ust yok", k.ust_yok);
        t::yanlis("ust yokken gaz devredilmez", k.gaz_ust_bilgisayarda);
    }

    // ── elle taret ucu birden ister ─────────────────────────────────────
    {
        Girdi g = temiz(); g.taret_anahtari = true;
        t::dogru("manuel + anahtar + guvenlik yok", karar_ver(g, true, true).elle_taret);
        g.kip = OTONOM;
        t::yanlis("otonomda taret anahtarina bakilmaz",
                  karar_ver(g, true, true).elle_taret);
        // Operatorun yanlislikla anahtara dokunmasi nisani elinden almamali.
        g.kip = MANUEL; g.estop_basili = true;
        t::yanlis("guvenlikte elle taret yok", karar_ver(g, true, true).elle_taret);
    }

    // ── kip gecisi: bayat komut uygulanmasin ────────────────────────────
    {
        GecisIzleyici iz;
        t::yanlis("ilk okuma gecis sayilmaz", iz.guncelle(MANUEL).degisti);

        GecisIstegi i = iz.guncelle(OTONOM);
        t::dogru("manuel -> otonom gecis", i.degisti);
        t::dogru("otonoma girerken komut sifirlanir", i.komut_sifirla);
        t::dogru("otonoma girerken referans kurulur", i.referans_sifirla);

        t::yanlis("ayni kipte gecis yok", iz.guncelle(OTONOM).degisti);

        i = iz.guncelle(MANUEL);
        t::dogru("otonom -> manuel gecis", i.degisti);
        t::yanlis("manuele donerken komut sifirlamaya gerek yok", i.komut_sifirla);
    }
    return t::rapor("kip hakemi");
}
