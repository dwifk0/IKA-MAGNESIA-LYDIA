// SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
// Copyright (C) 2026 Ahmet Efe Nezli

#include "taret_jog.h"

namespace taret {

float kol(uint16_t ch_us, const Ayar& a) {
    int32_t d = (int32_t)ch_us - (int32_t)a.merkez_us;
    if (d > -(int32_t)a.olu_bant_us && d < (int32_t)a.olu_bant_us) return 0.0f;
    d += (d > 0) ? -(int32_t)a.olu_bant_us : (int32_t)a.olu_bant_us;

    float o = (float)d / (float)(a.yay_ucu_us - a.olu_bant_us);
    if (o >  1.0f) o =  1.0f;
    if (o < -1.0f) o = -1.0f;
    return o;
}

float expo_uygula(float x, float e) {
    return (1.0f - e) * x + e * x * x * x;
}

void Jog::guncelle(uint16_t pan_ch, uint16_t tilt_ch, uint32_t dt_ms) {
    const float dt = (float)dt_ms / 1000.0f;

    pan_  += expo_uygula(kol(pan_ch,  a_), a_.expo) * a_.dps * dt;
    tilt_ += expo_uygula(kol(tilt_ch, a_), a_.expo) * a_.dps * dt;

    if (pan_  < a_.pan_min)  pan_  = a_.pan_min;
    if (pan_  > a_.pan_max)  pan_  = a_.pan_max;
    if (tilt_ < a_.tilt_min) tilt_ = a_.tilt_min;
    if (tilt_ > a_.tilt_max) tilt_ = a_.tilt_max;
}

}  // namespace taret
