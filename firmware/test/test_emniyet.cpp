// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Ahmet Efe Nezli
#include "test.h"
#include "../cekirdek/emniyet.h"
using namespace emniyet;

int main() {
    // ── acil stop: tek kanal ────────────────────────────────────────────
    {
        AcilStop e(false);
        e.guncelle(true, false, 0);
        t::dogru("tek kanal: A basiliysa basili", e.basili());
        t::yanlis("tek kanal: uyusmazlik ilan edilmez", e.uyusmazlik());
    }
    // ── acil stop: iki kanal ────────────────────────────────────────────
    {
        AcilStop e(true, 100);
        e.guncelle(false, false, 0);
        t::yanlis("ikisi de serbest", e.basili());

        e.guncelle(true, false, 10);
        t::dogru("tek kanal basiliysa basili sayilir", e.basili());
        t::yanlis("sicrama hemen uyusmazlik degil", e.uyusmazlik());

        e.guncelle(true, false, 200);
        t::dogru("fark surerse uyusmazlik ilan edilir", e.uyusmazlik());

        e.guncelle(true, true, 300);
        t::yanlis("kanallar bulusunca uyusmazlik kalkar", e.uyusmazlik());
        t::dogru("ikisi de basili", e.basili());
    }
    // ── ariza kilidi ────────────────────────────────────────────────────
    {
        AriziKilidi k;
        t::yanlis("baslangicta kilit yok", k.kilitli());
        k.bildir(false);
        t::dogru("ariza gorulunce kilitlenir", k.kilitli());
        k.bildir(true);
        t::dogru("saglik geri gelse de kilit acilmaz", k.kilitli());
        k.kullanici_sifirla(false);
        t::dogru("ariza surerken sifirlama ise yaramaz", k.kilitli());
        k.kullanici_sifirla(true);
        t::yanlis("ariza gectiyse operator kilidi acar", k.kilitli());
    }
    // ── stall mandali ───────────────────────────────────────────────────
    {
        StallMandali s(4200, 20);
        t::esit("olu bolge icindeki komut 0", 0, s.uygula(15, 0));
        t::esit("olu bolge disi komut gecer", 800, s.uygula(800, 0));
        t::yanlis("esik dolmadan mandal yok", s.mandalli());

        s.uygula(800, 4100);
        t::yanlis("4100 ms'de hala serbest", s.mandalli());

        s.uygula(800, 4200);
        t::dogru("esikte mandal kapandi", s.mandalli());
        t::esit("mandalliyken cikti sifir", 0, s.uygula(800, 5000));

        // 🔴 Asil davranis: mandali TERS YON degil, yon ALANININ degismesi
        // cozer. `0` da bir alandir.
        s.uygula(0, 5100);
        t::yanlis("komut 0'a cekilince mandal cozuldu", s.mandalli());
        t::esit("cozulduk, ayni yon yeniden gecer", 800, s.uygula(800, 5200));
    }
    {
        // Mandal ters yonle de cozulur — ama gerekli olan bu degil.
        StallMandali s(1000, 20);
        s.uygula(500, 0); s.uygula(500, 1000);
        t::dogru("ayni yonde esik doldu", s.mandalli());
        s.uygula(-500, 1100);
        t::yanlis("ters yon de mandali cozer", s.mandalli());
    }
    return t::rapor("emniyet");
}
