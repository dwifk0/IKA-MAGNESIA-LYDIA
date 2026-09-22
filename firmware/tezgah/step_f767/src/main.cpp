// SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
// Copyright (C) 2026 Ahmet Efe Nezli
/*
 * step_f767 — DİREKSİYON STEP MOTORU TEZGAH TESTİ (NUCLEO-F767ZI)
 *
 * `step_mega`nın portu. 30 Ağustos 2026'da Mega tasarımdan çıktı ve aynı gün
 * bağlantı ortak katottan **ORTAK ANOT**'a döndü — bu iki değişiklik testin
 * elektriğini de ters çevirdi, sadece pin numarasını değil.
 *
 * ── ORTAK ANOT / AKTİF-DÜŞÜK ────────────────────────────────────────────────
 *   PUL+ , DIR+  →  +5 V (UBEC)
 *   PUL−         →  PE9   AÇIK DRENAJ
 *   DIR−         →  PE10  AÇIK DRENAJ
 *
 * Pin 0 V'a çekince opto iletir = ETKİN. HIGH = Hi-Z, opto kesin söner.
 * Mega'da tam tersiydi (5 V verince etkin). Sebep: F767 3,3 V sürüyor ve
 * DM860H'nin giriş direnci 5 V'a göre boyutlanmış; ortak katotta opto yeterli
 * akım çekmez. 74HCT244 tam bu yüzden iptal edildi.
 *
 * ⚠⚠ ORTAK TOPRAK ÇEKME. Girişler optokuplörlü; opto akımının dönüş yolu
 *    MCU pininin kendisi. Sürücünün GND/AC ucu 48 V'un EKSİSİ, mantık toprağı
 *    değil — birleştirmek izolasyonu bozar ve 48 V gürültüsünü karta taşır.
 *
 * ⚠ ENA HİÇ TAKILMAYACAK. Boşta ENA = sürücü açık. Kablo takmak yalnız yeni
 *   bir arıza yolu açar; hareket hâlinde direksiyonun serbest kalması,
 *   tutulan tekerlekten tehlikelidir.
 *
 * ── BU TESTİN ASIL ÇIKTISI İKİ SAYI ────────────────────────────────────────
 *   1. adım kaçırmadan çıkılabilen en yüksek HIZ TAVANI
 *   2. aynı koşulda İVME
 * Bugün firmware'de 2700 Hz / 6000 Hz/s yazıyor ve **ikisi de masa başı
 * tahmini**. Buradan çıkan sayılarla değişecek.
 *
 * 🔴 PANONUN SAYACI KANIT DEĞİLDİR. Açık çevrimde sayaç her zaman "tam
 *    yerindeyim" der — gönderdiği darbeyi sayar, dönen mili değil.
 *    **Mile bant yapıştır ve işaretle. Kanıt o işarettir.**
 *
 * ── DERLEME / YÜKLEME ───────────────────────────────────────────────────────
 *   ~/ika/testler/yukle.sh step_f767
 * ⚠ Bu, ana firmware'in üzerine yazar. Test bitince tekrar ana firmware yükle.
 */

#include <Arduino.h>

// ── PİNLER — araçtakiyle AYNI ───────────────────────────────────────────────
static const uint32_t PIN_PUL = PE9;    // CN12-52 (D6)  — açık drenaj, aktif LOW
static const uint32_t PIN_DIR = PE10;   // CN12-47 (D40) — açık drenaj, aktif LOW
// PIN_ENA = PE12 — BİLEREK TANIMLANMADI. Bağlanmayacak.

// ── Mekanik ─────────────────────────────────────────────────────────────────
// ⚠ DIP SW5-SW8 ile AYNI olmak zorunda. Panodan 'm:' ile değiştirilebilir —
// DIP'i her denediğinde yeniden derlememek için. Uyuşmazsa "2 tur" komutu
// 2 tur döndürmez ve bunu SAYAÇTAN GÖREMEZSİN.
static long  mikroadim = 1600;
static const float REDUKTOR = 10.0f;    // FLE57-05SW 1:10
static long  adim_tur  = 16000;         // mikroadim × redüktör, 'm:' ile güncellenir

// ── Hız / ivme ──────────────────────────────────────────────────────────────
static const float HIZ_MIN_HZ = 200.0f;
static float hiz_tavan_hz  = 3000.0f;   // 'h' ile değişir
// ⚠ İVME ARTIK SABİT DEĞİL. Yüksek tavanlarda gerçek sınır burasıdır:
// 6000 Hz/s ile 6000 Hz'e çıkmak 1 saniye sürer, hareketin çoğu hızlanmayla
// geçer ve "tavanı yükselttim ama hızlanmadı" izlenimi doğar. 'i:' ile değişir.
static float ivme_hz_s = 6000.0f;
// DM860H: DIR, PUL'dan en az 5 µs önce kurulmalı. 20 µs pay bırakıldı.
static const uint32_t DIR_KURULMA_US = 20;

// ── Durum ───────────────────────────────────────────────────────────────────
static volatile int32_t konum_adim = 0;
static volatile int32_t hedef_adim = 0;
static volatile int8_t  yon_akt    = 0;
static volatile bool    pul_dusuk  = false;
static float    hiz_akt_hz = 0.0f;
static uint32_t darbe_toplam = 0;

// Git-gel
static int32_t  gitgel_kalan = 0, gitgel_adim = 0, gitgel_bas_konum = 0;
static int8_t   gitgel_yon = 1;
static uint16_t gitgel_toplam = 0;

// ⚠ TIM1: F767'de adım darbe treni buraya taşındı. Mega'da OC4C'ydi; burada
// TIM4 ENKODERİN (ana firmware, §6) ve TIM3 FRENİN — ikisi de kullanılamaz.
static HardwareTimer *tim = nullptr;

// ─────────────────────────────────────────────────────────────────────────────
// Darbe üreteci. Kesme her çağrıldığında PUL'u ters çevirir; bir TAM darbe
// iki kesmede tamamlanır, o yüzden zamanlayıcı 2×hz'te çalışır.
// Adım, düşen kenarda (opto ilettiği an) sayılır.
static void darbe_isr(void) {
    if (konum_adim == hedef_adim) return;
    pul_dusuk = !pul_dusuk;
    digitalWrite(PIN_PUL, pul_dusuk ? LOW : HIGH);   // LOW = etkin
    if (pul_dusuk) {                                  // düşen kenar = bir adım
        konum_adim += yon_akt;
        darbe_toplam++;
    }
}

static void darbe_kes(void) {
    if (tim) tim->pause();
    digitalWrite(PIN_PUL, HIGH);     // Hi-Z, opto sönük
    pul_dusuk = false;
}

static void darbe_basla(float hz) {
    if (hz < 1.0f) { darbe_kes(); return; }
    tim->setOverflow((uint32_t)(hz * 2.0f), HERTZ_FORMAT);   // 2× → toggle
    tim->resume();
}

// DIR değiştirmeden ÖNCE darbeyi kes, kurulma süresi bekle, sonra devam et.
// Aksi hâlde sürücü yön bilgisi oturmadan darbe görür ve o adım yanlış yöne gider.
static void dir_uygula(int8_t yon) {
    if (yon == yon_akt) return;
    darbe_kes();
    // Ters mantık: DIR de aktif-düşük. Hangi seviyenin "sağ" olduğu DENEMEYLE
    // bulunur, tahmin edilmez — araçta STEER_DIR_TERS ile düzeltilir.
    digitalWrite(PIN_DIR, (yon > 0) ? LOW : HIGH);
    delayMicroseconds(DIR_KURULMA_US);
    yon_akt = yon;
}

static void dur_hemen(void) {
    darbe_kes();
    noInterrupts();
    hedef_adim = konum_adim;
    interrupts();
    hiz_akt_hz = 0.0f;
    gitgel_kalan = 0;
}

static bool mesgul(void) {
    noInterrupts();
    bool m = (konum_adim != hedef_adim);
    interrupts();
    return m;
}

// ─────────────────────────────────────────────────────────────────────────────
static void guncelle(uint32_t dt_ms) {
    noInterrupts();
    int32_t k = konum_adim, h = hedef_adim;
    interrupts();

    int32_t kalan = h - k;
    if (kalan == 0) { if (hiz_akt_hz != 0.0f) darbe_kes(); hiz_akt_hz = 0.0f; return; }

    dir_uygula(kalan > 0 ? 1 : -1);

    // Yamuk profil: frenleme mesafesi v²/(2a). Adım kaçırmanın en sık sebebi
    // ani hız değişimidir, sabit hız değil.
    float mutlak = (float)(kalan > 0 ? kalan : -kalan);
    float fren_mesafe = (hiz_akt_hz * hiz_akt_hz) / (2.0f * ivme_hz_s);
    float hedef_hz = (mutlak <= fren_mesafe) ? HIZ_MIN_HZ : hiz_tavan_hz;

    float d = ivme_hz_s * (float)dt_ms / 1000.0f;
    if (hiz_akt_hz < hedef_hz) { hiz_akt_hz += d; if (hiz_akt_hz > hedef_hz) hiz_akt_hz = hedef_hz; }
    else                       { hiz_akt_hz -= d; if (hiz_akt_hz < hedef_hz) hiz_akt_hz = hedef_hz; }
    if (hiz_akt_hz < HIZ_MIN_HZ) hiz_akt_hz = HIZ_MIN_HZ;

    darbe_basla(hiz_akt_hz);
}

static void gitgel_yurut(void) {
    if (gitgel_kalan <= 0 || mesgul()) return;
    gitgel_kalan--;
    if (gitgel_kalan == 0) {
        noInterrupts(); int32_t k = konum_adim; interrupts();
        Serial.print(F("# GIT-GEL BITTI. sayac sapmasi=")); Serial.print(k - gitgel_bas_konum);
        Serial.println(F(" adim (0 olmali)."));
        Serial.println(F("#   ASIL SORU: MILDEKI ISARET nerede? Sayac kanit degil."));
        return;
    }
    gitgel_yon = -gitgel_yon;
    noInterrupts(); hedef_adim = konum_adim + (int32_t)gitgel_yon * gitgel_adim; interrupts();
}

// ─────────────────────────────────────────────────────────────────────────────
//   r:<onda bir tur>  saga · l:<...> sola · g:<adet> git-gel
//   h:<Hz> hiz tavani · d DUR · z sayaci sifirla
static void komut_isle(char *s) {
    char c = s[0];
    long v = 0;
    char *iki = strchr(s, ':');
    if (iki) v = atol(iki + 1);

    if (c == 'r' || c == 'l') {
        if (v <= 0) v = 10;
        long adim = (long)((float)v * (float)adim_tur / 10.0f);
        if (c == 'l') adim = -adim;
        noInterrupts(); hedef_adim = konum_adim + adim; interrupts();
        gitgel_kalan = 0;
        Serial.print(F("# ")); Serial.print(v / 10.0f, 1);
        Serial.println(c == 'r' ? F(" tur SAGA") : F(" tur SOLA"));
    } else if (c == 'g') {
        if (v <= 0) v = 1;
        gitgel_toplam = (uint16_t)v;
        gitgel_kalan  = v * 2;
        gitgel_adim   = 2 * adim_tur;
        gitgel_yon    = 1;
        noInterrupts();
        gitgel_bas_konum = konum_adim;
        hedef_adim = gitgel_bas_konum + gitgel_adim;
        interrupts();
        Serial.print(F("# GIT-GEL x")); Serial.print(v);
        Serial.println(F("  --- MILDEKI ISARETE BAK, sayaca degil"));
    } else if (c == 'h') {
        // Tavan 20 kHz'e cikarildi. Sinir motorun momenti, kodun degil:
        // 1600 mikroadimda 3000 Hz = motorda 112 RPM -- NEMA23 icin dusuk.
        if (v >= 100 && v <= 20000) {
            hiz_tavan_hz = (float)v;
            Serial.print(F("# hiz tavani ")); Serial.print(v);
            Serial.print(F(" Hz  =  "));
            Serial.print((float)v / (float)adim_tur * 360.0f, 1);
            Serial.println(F(" derece/sn"));
        } else Serial.println(F("# hiz tavani 100..20000 olmali"));
    } else if (c == 'i') {
        if (v >= 500 && v <= 200000) {
            ivme_hz_s = (float)v;
            Serial.print(F("# ivme ")); Serial.print(v);
            Serial.print(F(" Hz/s  ->  tavana ulasma "));
            Serial.print(hiz_tavan_hz / ivme_hz_s, 2); Serial.println(F(" sn"));
        } else Serial.println(F("# ivme 500..200000 olmali"));
    } else if (c == 'm') {
        // ⚠ Once DIP'i degistir, SONRA burayi. Ikisi uyusmazsa tur sayisi yalan.
        if (v == 400 || v == 800 || v == 1600 || v == 3200) {
            dur_hemen();
            mikroadim = v;
            adim_tur  = (long)((float)mikroadim * REDUKTOR);
            noInterrupts(); konum_adim = 0; hedef_adim = 0; interrupts();
            Serial.print(F("# mikroadim ")); Serial.print(v);
            Serial.print(F("  ->  adim/cikis turu ")); Serial.print(adim_tur);
            Serial.println(F("  (sayac sifirlandi)"));
            Serial.println(F("#   ⚠ DIP SW5-SW8 de AYNI olmali, yoksa tur sayisi yalan."));
        } else Serial.println(F("# mikroadim 400 / 800 / 1600 / 3200 olmali"));
    } else if (c == 'd') {
        dur_hemen(); Serial.println(F("# DUR"));
    } else if (c == 'z') {
        dur_hemen();
        noInterrupts(); konum_adim = 0; hedef_adim = 0; interrupts();
        Serial.println(F("# sayac sifirlandi"));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
static char tampon[24];
static uint8_t tampon_n = 0;
static uint32_t t_kontrol = 0, t_rapor = 0;

void setup() {
    Serial.begin(115200);

    // ⚠ SIRA ÖNEMLİ: önce seviyeyi yaz, sonra çıkışa al. Ters sırada reset
    // anında pin bir an tanımsız sürülür ve sürücü hayalet darbe alabilir.
    // HIGH = Hi-Z = opto sönük, yani "hiçbir şey yapma" hâli.
    digitalWrite(PIN_PUL, HIGH); pinMode(PIN_PUL, OUTPUT_OPEN_DRAIN); digitalWrite(PIN_PUL, HIGH);
    digitalWrite(PIN_DIR, HIGH); pinMode(PIN_DIR, OUTPUT_OPEN_DRAIN); digitalWrite(PIN_DIR, HIGH);

    tim = new HardwareTimer(TIM1);
    tim->setOverflow(2000, HERTZ_FORMAT);
    tim->attachInterrupt(darbe_isr);
    tim->pause();

    delay(200);
    Serial.println();
    Serial.println(F("# step_f767 — direksiyon step motoru tezgah testi"));
    Serial.println(F("# ORTAK ANOT / AKTIF-DUSUK: PUL+ ve DIR+ 5 V'ta, PUL-/DIR- acik drenaj"));
    Serial.println(F("# ⚠ ORTAK TOPRAK YOK. ENA TAKILMAYACAK."));
    Serial.print  (F("# adim/cikis turu = ")); Serial.println(adim_tur);
    Serial.println(F("# 🔴 SAYAC KANIT DEGIL — mile bant yapistir, isarete bak."));
    Serial.print  (F("# hiz tavani ")); Serial.print((long)hiz_tavan_hz);
    Serial.println(F(" Hz (DUSUK basla, 'h' ile artir)"));
}

void loop() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (tampon_n) { tampon[tampon_n] = 0; komut_isle(tampon); tampon_n = 0; }
        } else if (tampon_n < sizeof(tampon) - 1) {
            tampon[tampon_n++] = c;
        }
    }

    uint32_t simdi = millis();
    if (simdi - t_kontrol >= 5) { guncelle(simdi - t_kontrol); t_kontrol = simdi; }
    gitgel_yurut();

    if (simdi - t_rapor >= 100) {
        t_rapor = simdi;
        noInterrupts(); int32_t k = konum_adim, h = hedef_adim; interrupts();
        Serial.print(F("D adim="));   Serial.print(k);
        Serial.print(F(" hedef="));   Serial.print(h);
        Serial.print(F(" tur10="));   Serial.print((int32_t)((float)k / (float)adim_tur * 10.0f));
        Serial.print(F(" hz="));      Serial.print((long)hiz_akt_hz);
        Serial.print(F(" tavan="));   Serial.print((long)hiz_tavan_hz);
        Serial.print(F(" ivme="));    Serial.print((long)ivme_hz_s);
        Serial.print(F(" mikro="));   Serial.print(mikroadim);
        Serial.print(F(" derecesn=")); Serial.print((long)(hiz_akt_hz / (float)adim_tur * 360.0f));
        Serial.print(F(" yon="));     Serial.print(yon_akt);
        Serial.print(F(" mesgul="));  Serial.print(mesgul() ? 1 : 0);
        Serial.print(F(" gitgel="));  Serial.print(gitgel_kalan);
        Serial.print(F(" darbe="));   Serial.println(darbe_toplam);
    }
}
