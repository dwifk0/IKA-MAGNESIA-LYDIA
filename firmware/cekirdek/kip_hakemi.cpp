// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Ahmet Efe Nezli

#include "kip_hakemi.h"

namespace kip {

Kip oku(uint16_t ch_us, bool ters, uint16_t esik_orta, uint16_t esik_ust,
        bool ust_bilgisayar_etkin) {
    if (!ust_bilgisayar_etkin) return MANUEL;
    uint16_t ch = ch_us;
    if (ters) ch = (uint16_t)(3000 - ch);      // 1000 <-> 2000 aynala
    if (ch >= esik_ust)  return OTONOM;
    if (ch >= esik_orta) return BOS;
    return MANUEL;
}

Karar karar_ver(const Girdi& g, bool taret_var, bool taret_rc_var) {
    Karar k{};

    // 🔴 BOS kademe (anahtarin ortasi) = YUMUSAK ACIL STOP. Guvenlik kumesine
    // katiliyor; tek bu satir gaz rolanti + fren bas + direksiyon dondur
    // sagliyor. Manuelden otonoma gecis bu kademeden GECMEK zorunda — arada
    // arac tam durur. Gecisin kendisi bir emniyet adimi.
    const bool bos_mod = (g.kip == BOS);

    k.guvenlik  = g.kesme || g.rc_sessiz || g.estop_basili
               || bos_mod || g.gaz_ariza_kilit;
    k.rolantiye = k.guvenlik || g.taret_anahtari;

    k.ust_yok = !g.ust_canli || g.ust_estop || g.ust_dur;

    k.gaz_ust_bilgisayarda = (g.kip == OTONOM) && !k.rolantiye && !k.ust_yok;

    // Manuel taret: taret anahtari acik + kip MANUEL + guvenlik yok.
    // Ucu birden sart.
    //
    // ⚠ Otonom kipte taret anahtarina BAKILMIYOR: otonom kosuda tareti ust
    // bilgisayar surer ve operatorun yanlislikla anahtara dokunmasi nisani
    // elinden almamali.
    k.elle_taret = taret_var && taret_rc_var && g.taret_anahtari
                && (g.kip == MANUEL) && !k.guvenlik;
    return k;
}

GecisIstegi GecisIzleyici::guncelle(Kip yeni) {
    GecisIstegi i{};
    if (ilk_) { ilk_ = false; suanki_ = yeni; return i; }
    if (yeni == suanki_) return i;

    i.degisti = true;
    if (yeni != MANUEL) {
        i.komut_sifirla = true;
        // 🔴 Otonoma GIRERKEN bulunulan konum yeni referans (0) olur.
        // Kumanda yolunda yazilim siniri olmadigi icin direksiyon acilistaki
        // referanstan uzaklasabiliyor; otonom ise MUTLAK aci aliyor.
        // Referansi burada kurmak, otonomun "duz" dedigi seyi operatorun
        // anahtari attigi andaki konuma esitliyor.
        //
        // ⚠ ISLETME KURALI: otonoma alirken TEKERLEKLER DUZ OLACAK. Egri
        // konumda gecilirse otonom o egriligi duz sayar.
        i.referans_sifirla = true;
    }
    suanki_ = yeni;
    return i;
}

}  // namespace kip
