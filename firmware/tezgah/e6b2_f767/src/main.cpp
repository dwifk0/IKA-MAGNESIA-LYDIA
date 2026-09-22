// SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
// Copyright (C) 2026 Ahmet Efe Nezli
/*
 * E6B2-CWZ6C ENKODER TEZGAH TESTİ — Nucleo-F767ZI, donanım kuadratür
 *
 * Amaç: enkoderi araca takmadan, elle çevirerek doğrulamak.
 *
 * ── NEDEN BU TEST ÖZELLİKLE ÖNEMLİ ──────────────────────────────────────────
 * 16 Ağustos 2026'da sponsorun bedava verdiği 300B enkoderi tam da burada
 * elendi: **A kanalı çalışıyordu, B kanalı hiç cevap vermiyordu.** Tek kanalla
 * kuadratür olmaz, yön bilinmez, `anti_rollback` rampada geri kaymayı
 * ters işaretle görür. O arıza multimetreyle GÖRÜLEMEDİ — beslemesiz direnç
 * ölçümü sağlam kanalı da "OL" okuyor, beslemeliyken de DMM çok yavaş.
 *
 * Bu yüzden sketch iki şeyi ayrı ayrı raporluyor:
 *   • ab_saglik : her iki kanal da hem HIGH hem LOW görüldü mü
 *   • hatali    : yasak kuadratür geçişi sayısı (iki kanal aynı anda değişti)
 * Sayım artıyor olması yetmez — TEK kanal da sayım artırır.
 *
 * ── DERLEME / YÜKLEME ───────────────────────────────────────────────────────
 *   ~/.local/bin/pio run  /  ~/.local/bin/pio run -t upload
 * ⚠ Yüklemeden önce panoyu KAPAT.
 */

#include <Arduino.h>

// Ana firmware'le (F767_FIRMWARE/src/config.h) AYNI pinler.
// TIM4_CH1 = PD12, TIM4_CH2 = PD13.
// ⚠ TIM2/TIM3'ün doğal enkoder pinleri (PA0/PA1, PA6/PA7) Ethernet'in RMII
// hattında; bu kartta kullanılamıyorlar. Sebep config.h §0'da yazılı.
#define A_PORT   GPIOD
#define A_BIT    GPIO_PIN_12
#define B_BIT    GPIO_PIN_13
#define Z_PIN    PD11

#define PULSE_TUR   600L
#define SAYIM_TUR   (PULSE_TUR * 4)      // ×4 kuadratür

TIM_HandleTypeDef htim;

int32_t  sayim = 0;
uint16_t son_cnt = 0;
int32_t  hiz_sayim_s = 0;
int32_t  tur100 = 0;             // yüzde bir tur
int32_t  rpm = 0;

// Kanal sağlığı: her kanalın hem 1 hem 0 görülmesi gerekir.
uint8_t a_gorulen = 0, b_gorulen = 0;    // bit0 = LOW goruldu, bit1 = HIGH
uint32_t hatali = 0;                     // GERCEK yasak gecis (ornekleme sikken)
uint8_t  son_ab = 0xFF;

// 31 Agu 2026 — YASAK GECIS SAYISI YANILTICIYDI.
// Eski surum 1 kHz orneklyordu ve ayni dongu 10 Hz'de seri porta ~120 karakter
// basiyordu; 115200 baud'da o satir ~10 ms suruyor ve o sirada ornekleme
// DURUYOR. Bosluk boyunca enkoder 2+ adim ilerleyince ornekleyici "iki bit
// birden degisti" goruyor. Saniyede 10 rapor -> saniyede ~10 sahte kayit.
// Elle yavas cevirirken bile sayi artiyordu, cunku sebep hiz degil BOSLUKTU.
//
// Cozum: her dongude ornekle, ornekler arasi bosluğu olc, ve bosluk esikten
// buyukse o gecisi "yargilanamaz" diye AYRI say. Boylece:
//   hatali  > 0 -> gercek sinyal butunlugu sorunu
//   atlanan > 0 -> yalnizca olcum atladi, enkoder hakkinda bir sey soylemez
// Dogrulama: tur basi sayim tam 2400 ise gecis kaybi/fazlasi YOKTUR.
#define ORNEK_ESIK_US  250U              // bu kadar sik ornekledikse yargila
uint32_t atlanan = 0;
uint32_t son_ornek_us = 0;
uint32_t en_buyuk_bosluk_us = 0;

volatile uint32_t z_sayisi = 0;
int32_t z_son_sayim = 0, z_fark = 0;

// 31 Agu 2026: "tam tur cevir, 2400 mu" testi elle cevirmeye ve goz kararina
// bagliydi. Z darbesi ZATEN tur basi geliyor — sayaci Z aninda yakalayip iki
// Z arasindaki farki olcersek olcek testi kendiliginden yapilir.
//   z_araligi == 2400  -> P/R dogru, kuadratur saglam
//   z_araligi == -2400 -> ayni, ters yone donuyor
//   kucuk/rastgele     -> Z'de gurultu, sahte tetikleme
volatile int16_t  z_araligi = 0;
volatile uint16_t z_cnt_son = 0;
volatile bool     z_ilk = true;

static void enk_kur(void) {
  __HAL_RCC_GPIOD_CLK_ENABLE();
  GPIO_InitTypeDef g = {0};
  g.Pin       = A_BIT | B_BIT;
  g.Mode      = GPIO_MODE_AF_PP;
  // Dahili pull-up İKİNCİ savunma. E6B2 NPN açık kollektör: yüksek seviyeyi
  // pull-up belirler. Dahili ~40 kΩ 53 kHz'de kenarı yuvarlar →
  // HARİCİ 1 kΩ ŞART. Tezgahta elle çevirirken dahili yeter, araçta YETMEZ.
  g.Pull      = GPIO_PULLUP;
  g.Speed     = GPIO_SPEED_FREQ_HIGH;
  g.Alternate = GPIO_AF2_TIM4;
  HAL_GPIO_Init(A_PORT, &g);

  __HAL_RCC_TIM4_CLK_ENABLE();
  htim.Instance               = TIM4;
  htim.Init.Prescaler         = 0;
  htim.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim.Init.Period            = 0xFFFF;
  htim.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  TIM_Encoder_InitTypeDef e = {0};
  e.EncoderMode  = TIM_ENCODERMODE_TI12;
  e.IC1Polarity  = TIM_ICPOLARITY_RISING;
  e.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  e.IC1Prescaler = TIM_ICPSC_DIV1;
  e.IC1Filter    = 6;
  e.IC2Polarity  = TIM_ICPOLARITY_RISING;
  e.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  e.IC2Prescaler = TIM_ICPSC_DIV1;
  e.IC2Filter    = 6;

  HAL_TIM_Encoder_Init(&htim, &e);
  HAL_TIM_Encoder_Start(&htim, TIM_CHANNEL_ALL);
  son_cnt = (uint16_t)__HAL_TIM_GET_COUNTER(&htim);
}

static void z_isr(void) {
  // (int16_t) fark tasmayi kendisi cozer; bir tur 2400 sayim, int16'ya sigar.
  uint16_t c = (uint16_t)__HAL_TIM_GET_COUNTER(&htim);
  if (!z_ilk) z_araligi = (int16_t)(c - z_cnt_son);
  z_ilk = false;
  z_cnt_son = c;
  z_sayisi++;
}

// AF moduna alınmış pin de IDR'den okunabilir — kanalların ham seviyesini
// görmek için timer'ı bozmaya gerek yok.
static inline uint8_t ab_oku(void) {
  uint32_t idr = A_PORT->IDR;
  uint8_t a = (idr & A_BIT) ? 1 : 0;
  uint8_t b = (idr & B_BIT) ? 1 : 0;
  return (uint8_t)((a << 1) | b);
}

// Kuadratürde her adımda TEK bit değişir. İkisi birden değişirse ya darbe
// kaçtı ya gürültü var. Bu örnekleme (1 kHz) hızlı çevirmede kendisi de
// atlar — bu yüzden sayı "mutlak arıza sayısı" değil, ELLE YAVAŞ çevirirken
// bakılan bir gösterge.
static void ab_denetle(void) {
  uint32_t su_an = micros();
  uint32_t bosluk = su_an - son_ornek_us;
  son_ornek_us = su_an;
  if (bosluk > en_buyuk_bosluk_us) en_buyuk_bosluk_us = bosluk;

  uint8_t ab = ab_oku();
  uint8_t a = (ab >> 1) & 1, b = ab & 1;
  a_gorulen |= (uint8_t)(1 << a);
  b_gorulen |= (uint8_t)(1 << b);
  if (son_ab != 0xFF && ab != son_ab) {
    uint8_t d = ab ^ son_ab;
    if (d == 0x03) {              // iki bit birden degisti
      if (bosluk <= ORNEK_ESIK_US) hatali++;   // ornekleme sikti -> GERCEK
      else                         atlanan++;  // ornekleme atladi -> yargilanamaz
    }
  }
  son_ab = ab;
}

uint32_t t_kontrol = 0, t_rapor = 0;   // t_hizli kalkti: ornekleme artik her dongude

void setup() {
  Serial.begin(115200);
  delay(300);
  enk_kur();
  pinMode(Z_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(Z_PIN), z_isr, FALLING);

  Serial.println();
  Serial.println(F("# E6B2 enkoder tezgah testi — F767ZI, TIM4 donanim sayici"));
  Serial.print(F("# 1 tur = ")); Serial.print(SAYIM_TUR);
  Serial.println(F(" sayim (600 P/R x 4)"));
  Serial.println(F("# ELLE YAVASCA CEVIR. Bakilacak uc sey:"));
  Serial.println(F("#   1) sayim degisiyor mu"));
  Serial.println(F("#   2) ters cevirince sayim GERI gidiyor mu"));
  Serial.println(F("#      (gitmiyorsa B kanali olu — 300B'de tam bu oldu)"));
  Serial.println(F("#   3) tam 1 tur cevirince sayim 2400 artiyor mu"));

  // Ilk ornek acilistan beri gecen sureyi bosluk sanmasin (setup 300 ms
  // bekliyor + micros() sifirdan geliyor -> 300 000 us gibi sahte bir tepe).
  son_ornek_us = micros();
  en_buyuk_bosluk_us = 0;
}

void loop() {
  uint32_t simdi = millis();

  // Her dongude ornekle — 1 kHz kapisi kaldirildi (bkz. ab_denetle yorumu).
  ab_denetle();

  if (simdi - t_kontrol >= 50) {     // 20 Hz
    uint32_t dt = simdi - t_kontrol;
    t_kontrol = simdi;

    uint16_t cnt  = (uint16_t)__HAL_TIM_GET_COUNTER(&htim);
    int16_t  fark = (int16_t)(cnt - son_cnt);   // tasmayi kendisi cozer
    son_cnt = cnt;
    sayim  += fark;

    hiz_sayim_s = ((int32_t)fark * 1000) / (int32_t)dt;
    tur100 = sayim * 100 / SAYIM_TUR;
    rpm    = hiz_sayim_s * 60 / SAYIM_TUR;
  }

  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'z') {
      sayim = 0; tur100 = 0;
      son_cnt = (uint16_t)__HAL_TIM_GET_COUNTER(&htim);
      Serial.println(F("# sayac sifirlandi"));
    } else if (c == 'h') {
      hatali = 0; a_gorulen = 0; b_gorulen = 0; son_ab = 0xFF;
      atlanan = 0; en_buyuk_bosluk_us = 0;
      z_sayisi = 0; z_araligi = 0; z_ilk = true;   // Z de sifirlanir (31 Agu)
      Serial.println(F("# saglik sayaclari sifirlandi (Z dahil)"));
    } else if (c == 'i') {
      z_fark = sayim - z_son_sayim;
      z_son_sayim = sayim;
      Serial.print(F("# son Z'den beri sayim farki: ")); Serial.print(z_fark);
      Serial.println(F("  (tam tur ise 2400 olmali)"));
    }
  }

  if (simdi - t_rapor >= 100) {      // 10 Hz — panonun okudugu satir
    t_rapor = simdi;
    // ab_saglik: 2 = iki kanal da saglam, 1 = biri olu, 0 = ikisi de sessiz
    uint8_t a_ok = (a_gorulen == 0x03) ? 1 : 0;
    uint8_t b_ok = (b_gorulen == 0x03) ? 1 : 0;
    Serial.print(F("D sayim="));   Serial.print(sayim);
    Serial.print(F(" tur100=")); Serial.print(tur100);
    Serial.print(F(" rpm="));      Serial.print(rpm);
    Serial.print(F(" hizs="));     Serial.print(hiz_sayim_s);
    Serial.print(F(" akanal="));   Serial.print(a_ok);
    Serial.print(F(" bkanal="));   Serial.print(b_ok);
    Serial.print(F(" hatali="));   Serial.print(hatali);
    Serial.print(F(" z="));        Serial.print(z_sayisi);
    // TESHIS (31 Agu 2026): sayac hic saymayinca "sinyal yok mu, sayac mi
    // saymiyor" ayrilamadi. Ham seviye bunu tek bakista soyler:
    //   alvl/blvl = pinin SU ANKI seviyesi
    //   amask/bmask = 1 sadece LOW goruldu · 2 sadece HIGH · 3 ikisi de
    // Ikisi de HIGH'ta takili -> hat hic asagi cekilmiyor (besleme/kablo/pin)
    // Ikisi de LOW'da takili  -> pull-up yok, ya da hat GND'ye kacmis
    uint8_t ab = ab_oku();
    Serial.print(F(" alvl="));  Serial.print((ab >> 1) & 1);
    Serial.print(F(" blvl="));  Serial.print(ab & 1);
    Serial.print(F(" zlvl="));  Serial.print(digitalRead(Z_PIN) ? 1 : 0);
    Serial.print(F(" amask=")); Serial.print(a_gorulen);
    Serial.print(F(" bmask=")); Serial.print(b_gorulen);
    Serial.print(F(" cnt="));   Serial.print((uint16_t)__HAL_TIM_GET_COUNTER(&htim));
    Serial.print(F(" zara="));  Serial.print(z_araligi);
    Serial.print(F(" atlanan=")); Serial.print(atlanan);
    Serial.print(F(" bosluk="));  Serial.println(en_buyuk_bosluk_us);
  }
}
