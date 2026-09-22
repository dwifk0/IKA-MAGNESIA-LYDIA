// SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
// Copyright (C) 2026 Ahmet Efe Nezli
// ─────────────────────────────────────────────────────────────────────────────
// DİREKSİYON ENKODERİ — tezgah testi (4 Eylül 2026)
// ─────────────────────────────────────────────────────────────────────────────
// İkinci E6B2-CWZ6C (600 P/R), direksiyon miline gidecek olan. Bu test onu
// ARACA TAKMADAN, mil elle çevrilerek doğrular.
//
// Traksiyon enkoderinden farkı: bu YAZILIM kuadratürü. Boş donanım sayacı
// kalmadı (TIM1 step, TIM2 taret, TIM3 fren, TIM4 traksiyon; TIM5 Ethernet'e,
// TIM8 frene takılı). Gerek de yok — direksiyon mili yavaş, ~1.800 sayım/s.
//
// Bu test tam olarak `e6b2_f767` testinin ölçütlerini tekrar ediyor, çünkü
// sponsorun 300B enkoderi 16 Ağustos'ta tam o ölçütlerde elenmişti: A kanalı
// çalışıyor, B ölü, sayım artıyor ama yön yok. Aynı tuzağa iki kez düşülmez.
//
// Pinler config.h'daki rezervasyonla BİREBİR aynı — bu test kabloyu da
// doğruluyor, sonra ana firmware'e aynı bağlantıyla geçilecek.
// ─────────────────────────────────────────────────────────────────────────────
#include <Arduino.h>

#define A_PIN      PE7        // D41
#define B_PIN      PE8        // D42
#define Z_PIN      PF12       // D8
#define Z_VAR      true       // Z teli bağlıysa true

#define PULSE_TUR  600L
#define SAYIM_TUR  (PULSE_TUR * 4)     // ×4 kuadratür = 2400

// ── Kuadratür durum tablosu ─────────────────────────────────────────────────
// index = (önceki << 2) | şimdiki,  durum = (A << 1) | B
// 0 olan girdilerin İKİ ayrı anlamı var:
//   • değişim yok  (0, 5, 10, 15)     → normal, sayılmaz
//   • İKİ BİT BİRDEN değişti (3,6,9,12) → YASAK GEÇİŞ, kaçırılmış kenar demek
// Yasak geçişleri ayrı saymak şart: sayım "çalışıyor gibi" görünürken sessizce
// adım kaybediyor olabilir ve bunu başka hiçbir ölçüt yakalamaz.
static const int8_t QTAB[16] = {
     0, +1, -1,  0,
    -1,  0,  0, +1,
    +1,  0,  0, -1,
     0, -1, +1,  0
};
static const bool YASAK[16] = {
    false,false,false,true,
    false,false,true, false,
    false,true, false,false,
    true, false,false,false
};

volatile int32_t  sayim      = 0;
volatile uint32_t yasak_n    = 0;
volatile uint8_t  a_maske    = 0;      // bit0: LOW görüldü · bit1: HIGH görüldü
volatile uint8_t  b_maske    = 0;
volatile uint8_t  onceki     = 0;
volatile uint32_t kenar_n    = 0;

volatile int32_t  z_sayim    = 0;      // son Z anındaki sayım
volatile int32_t  z_ara      = 0;      // iki Z arası fark
volatile uint32_t z_n        = 0;
volatile bool     z_ilk      = true;

// ⚠ ISR'de digitalRead() KULLANILMIYOR: STM32 çekirdeğinde o çağrı pin
// haritası üzerinden gidiyor ve kenar başına yüzlerce çevrim yiyor. İki kanal
// aynı portta ve ardışık bit olduğu için tek register okumasıyla alınıyor —
// pinlerin PE7/PE8 seçilmesinin asıl sebebi buydu (config.h §7).
static inline uint8_t durum_oku(void) {
    uint32_t idr = GPIOE->IDR;
    uint8_t a = (idr >> 7) & 1;
    uint8_t b = (idr >> 8) & 1;
    a_maske |= (uint8_t)(a ? 2 : 1);
    b_maske |= (uint8_t)(b ? 2 : 1);
    return (uint8_t)((a << 1) | b);
}

static void ab_isr(void) {
    const uint8_t s = durum_oku();
    const uint8_t i = (uint8_t)((onceki << 2) | s);
    onceki = s;
    kenar_n++;
    if (YASAK[i]) { yasak_n++; return; }
    sayim += QTAB[i];
}

static void z_isr(void) {
    if (!z_ilk) z_ara = sayim - z_sayim;
    z_sayim = sayim;
    z_ilk   = false;
    z_n++;
}

void setup(void) {
    Serial.begin(115200);
    delay(300);

    // Dahili pull-up AÇILMIYOR: E6B2 NPN açık kollektör ve harici 1 kΩ
    // pull-up'lar araçtaki hâliyle kuruluyor. Dahili ~40 kΩ onlara paralel
    // girip seviyeyi bozmasın diye giriş yüzer bırakılıyor — pull-up'ı
    // takmayı unutursanız bu test HİÇ saymaz ve hatayı hemen görürsünüz.
    pinMode(A_PIN, INPUT);
    pinMode(B_PIN, INPUT);
    if (Z_VAR) pinMode(Z_PIN, INPUT);

    onceki = durum_oku();
    a_maske = b_maske = 0;             // ilk okuma maskeyi kirletmesin

    attachInterrupt(digitalPinToInterrupt(A_PIN), ab_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(B_PIN), ab_isr, CHANGE);
    if (Z_VAR) attachInterrupt(digitalPinToInterrupt(Z_PIN), z_isr, FALLING);

    Serial.println();
    Serial.println(F("# DIREKSIYON ENKODERI — tezgah testi"));
    Serial.println(F("# A=PE7(D41)  B=PE8(D42)  Z=PF12(D8)   600 P/R, tur=2400 sayim"));
    Serial.println(F("# Mili ELLE cevir. Olcutler:"));
    Serial.println(F("#   1) sayim degisiyor"));
    Serial.println(F("#   2) ters cevirince GERI gidiyor"));
    Serial.println(F("#   3) amask=3  (A hem HIGH hem LOW gorundu)"));
    Serial.println(F("#   4) bmask=3  (B hem HIGH hem LOW gorundu)"));
    Serial.println(F("#   5) zara = +/-2400, sapmasiz"));
    Serial.println(F("#   6) yasak = 0"));
    Serial.println(F("# Komut:  s = sayaclari sifirla"));
    Serial.println();
}

void loop(void) {
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 's' || c == 'S') {
            noInterrupts();
            sayim = 0; yasak_n = 0; kenar_n = 0;
            a_maske = b_maske = 0;
            z_n = 0; z_ara = 0; z_ilk = true;
            interrupts();
            Serial.println(F("# sifirlandi"));
        }
    }

    static uint32_t t = 0;
    if (millis() - t < 200) return;
    t = millis();

    noInterrupts();
    int32_t  s   = sayim;
    uint32_t y   = yasak_n;
    uint32_t k   = kenar_n;
    uint8_t  am  = a_maske, bm = b_maske;
    int32_t  za  = z_ara;
    uint32_t zn  = z_n;
    interrupts();

    // Derece: kolon açısı. Teker açısı için direksiyon kutusu redüksiyonuna
    // bölünecek — o oran ÖLÇÜLMEDİ (bkz. OTONOM_DIR_ORAN), bu yüzden burada
    // bilerek ham kolon derecesi basılıyor.
    float derece = (float)s * 360.0f / (float)SAYIM_TUR;

    Serial.print(F("D sayim=")); Serial.print(s);
    Serial.print(F(" tur="));    Serial.print((float)s / (float)SAYIM_TUR, 3);
    Serial.print(F(" derece=")); Serial.print(derece, 2);
    Serial.print(F(" amask="));  Serial.print(am);
    Serial.print(F(" bmask="));  Serial.print(bm);
    Serial.print(F(" kenar="));  Serial.print(k);
    Serial.print(F(" yasak="));  Serial.print(y);
    Serial.print(F(" zn="));     Serial.print(zn);
    Serial.print(F(" zara="));   Serial.print(za);
    Serial.println();
}
