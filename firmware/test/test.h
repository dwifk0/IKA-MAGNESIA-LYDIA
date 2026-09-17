// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Ahmet Efe Nezli
//
// Kucuk bir kosum takimi. Kutuphane yok, bagimlilik yok — testler kartsiz,
// masaustunde `make` ile kosuyor.

#pragma once
#include <cstdio>
#include <cstdint>

namespace t {
inline int gecen = 0, kalan = 0;

inline void esit(const char* ad, long long beklenen, long long bulunan) {
    if (beklenen == bulunan) { gecen++; return; }
    kalan++;
    std::printf("  ✗ %-52s beklenen %lld, bulunan %lld\n", ad, beklenen, bulunan);
}

inline void dogru(const char* ad, bool k) {
    if (k) { gecen++; return; }
    kalan++;
    std::printf("  ✗ %-52s dogru bekleniyordu\n", ad);
}

inline void yanlis(const char* ad, bool k) { dogru(ad, !k); }

inline int rapor(const char* kume) {
    std::printf("%-26s %3d gecti, %d kaldi\n", kume, gecen, kalan);
    return kalan == 0 ? 0 : 1;
}
}  // namespace t
