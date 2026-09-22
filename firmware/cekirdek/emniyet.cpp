// SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
// Copyright (C) 2026 Ahmet Efe Nezli

#include "emniyet.h"

namespace emniyet {

void AcilStop::guncelle(bool a, bool b, uint32_t simdi_ms) {
    if (!b_var_) {
        // Tek kanal: karsilastirilacak ikinci olcum yok, uyusmazlik denetimi
        // de yok. Var gibi davranmak yanlis guven uretir.
        basili_ = a;
        uyusmazlik_ = false;
        fark_bas_ = 0;
        return;
    }

    // Iki kanaldan HERHANGI biri basili diyorsa basili sayilir. "Ikisi de
    // demeli" kurali, bir kanalin kopmasini sessizce tolere ederdi.
    basili_ = (a || b);

    if (a != b) {
        if (fark_bas_ == 0) fark_bas_ = simdi_ms;
        else if (simdi_ms - fark_bas_ > uyusmazlik_ms_) uyusmazlik_ = true;
    } else {
        fark_bas_ = 0;
        uyusmazlik_ = false;
    }
}

int16_t StallMandali::uygula(int16_t binde, uint32_t simdi_ms) {
    if (binde >  1000) binde =  1000;
    if (binde < -1000) binde = -1000;

    const int8_t yon = (binde > olu_bolge_) ? 1
                     : (binde < -olu_bolge_ ? -1 : 0);

    // Yon ALANI degisti mi? `0` da bir alandir; mandali cozen budur.
    if (yon != yon_) {
        yon_ = yon;
        yon_bas_ = simdi_ms;
        mandal_ = false;
    }

    if (yon_ != 0 && (simdi_ms - yon_bas_) >= esik_ms_) mandal_ = true;

    if (yon_ == 0 || mandal_) return 0;
    return binde;
}

}  // namespace emniyet
