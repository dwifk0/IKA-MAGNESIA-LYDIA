// SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
// Copyright (C) 2026 Ahmet Efe Nezli
// FlySky iBUS -> NUCLEO-F767ZI  (UART5_RX = PD2)
//
// Bu test "veri geliyor mu" sorusunu DEGIL, su dordunu cevapliyor:
//
//   1. Hangi kol/anahtar hangi kanalda?   -> kanal oynayinca ham kutuge yazar
//   2. SwC gercekten uc kademe mi?        -> panoda ch9'a bak (1000/1500/2000)
//   3. Cerceve saglam mi?                 -> saglam / bozuk sayaclari
//   4. FAILSAFE ne yapiyor?               -> asagiya bak
//
// (4) en kritigi: 29 Agustos tezgah olcumu, ALICININ VERICI KAPALIYKEN DE
// YAYIN YAPTIGINI gosterdi. Yani "cerceve gelmiyor" korumasi hicbir zaman
// tetiklenmez; gelen sey alicinin failsafe degerleridir.
//
// 🔴 30 Agu: ilk surumde "degerler donduysa failsafe" diye bir bayrak vardi.
// YANLISTI ve testi sonucsuz birakti: verici ACIKKEN de kimse kola dokunmazsa
// degerler donar. Alici tarafindan sinyal kaybini ANLAMANIN YOLU YOK.
// Dogru yontem olcum duzeninde: bir kolu MERKEZDEN UZAK tutarken vericiyi
// kapat. Deger yerinde kalirsa failsafe "son degeri tut"tur (tehlikeli),
// baska bir yere siçrarsa failsafe kuruludur ve o siçradigi deger ayardir.
// Firmware burada yalnizca ham gercegi bildirir: her kanal ve son degisimden
// bu yana gecen sure.
//
// iBUS cercevesi: 32 bayt | 0x20 0x40 | 14 x 2 bayt LE kanal | 2 bayt saglama
// saglama = 0xFFFF - (ilk 30 baytin toplami)

#include <Arduino.h>

// UART5: RX = PD2 (dogrulandi: PeripheralPins.c AF8_UART5).
// TX = PC12 sadece surucunun istedigi icin veriliyor, hicbir yere baglanmiyor.
HardwareSerial ibus(PD2, PC12);

static const uint8_t  KANAL_SAYISI  = 14;
static const uint16_t OYNAMA_ESIGI  = 60;    // "bu kanal oynadi" esigi (us)
static const uint32_t PENCERE_MS    = 400;   // oynama penceresi

static uint8_t  ara[32];
static uint8_t  n = 0;

static uint16_t kanal[KANAL_SAYISI];
static uint16_t p_min[KANAL_SAYISI], p_max[KANAL_SAYISI];   // pencere ici

static uint32_t saglam = 0, bozuk = 0;
static uint32_t saniye_saglam = 0, hz = 0;
static uint32_t son_cerceve_ms = 0, son_oynama_ms = 0;
static uint32_t onceki_bozuk = 0, bozuk_led_ms = 0;
static uint32_t t_hz = 0, t_pencere = 0, t_yaz = 0;
static bool     ilk_cerceve = false;

static void pencereyi_sifirla() {
  for (uint8_t i = 0; i < KANAL_SAYISI; i++) { p_min[i] = 0xFFFF; p_max[i] = 0; }
  t_pencere = millis();
}

static void sayaclari_sifirla() {
  saglam = bozuk = 0;
  son_oynama_ms = millis();
  pencereyi_sifirla();
  Serial.println(F("sayaclar sifirlandi"));
}

static void cerceveyi_isle() {
  uint16_t toplam = 0;
  for (uint8_t i = 0; i < 30; i++) toplam += ara[i];
  const uint16_t bekleneni = 0xFFFF - toplam;
  const uint16_t gelen = (uint16_t)ara[30] | ((uint16_t)ara[31] << 8);

  if (gelen != bekleneni) { bozuk++; return; }

  saglam++; saniye_saglam++;
  son_cerceve_ms = millis();

  for (uint8_t i = 0; i < KANAL_SAYISI; i++) {
    const uint16_t v = (uint16_t)ara[2 + 2 * i] | ((uint16_t)ara[3 + 2 * i] << 8);
    kanal[i] = v;
    if (v < p_min[i]) p_min[i] = v;
    if (v > p_max[i]) p_max[i] = v;
  }

  if (!ilk_cerceve) {
    ilk_cerceve = true;
    son_oynama_ms = millis();
    Serial.println(F("ilk saglam cerceve alindi"));
  }
}

void setup() {
  Serial.begin(115200);
  ibus.begin(115200);

  for (uint8_t i = 0; i < KANAL_SAYISI; i++) kanal[i] = 0;
  pencereyi_sifirla();
  son_oynama_ms = millis();

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED,   OUTPUT);

  Serial.println();
  Serial.println(F("iBUS testi - UART5 RX = PD2, 115200"));
  Serial.println(F("Kollari TEK TEK oynat: hangi kanalin oynadigini buraya yazar."));
  Serial.println(F("FAILSAFE denemesi: bir kolu MERKEZDEN UZAK TUTARKEN vericiyi kapat."));
}

void loop() {
  // ---- bayt topla ----
  while (ibus.available()) {
    const uint8_t b = (uint8_t)ibus.read();
    if (n == 0) { if (b != 0x20) continue; }
    else if (n == 1) { if (b != 0x40) { n = 0; if (b == 0x20) { ara[0] = b; n = 1; } continue; } }
    ara[n++] = b;
    if (n == 32) { cerceveyi_isle(); n = 0; }
  }

  const uint32_t simdi = millis();

  // ---- saniyelik hiz ----
  if (simdi - t_hz >= 1000) { t_hz = simdi; hz = saniye_saglam; saniye_saglam = 0; }

  // ---- oynama penceresi: hangi kanal hareket etti ----
  if (simdi - t_pencere >= PENCERE_MS) {
    int8_t   en_cok = -1;
    uint16_t en_genis = 0;
    for (uint8_t i = 0; i < KANAL_SAYISI; i++) {
      if (p_max[i] < p_min[i]) continue;
      const uint16_t genislik = p_max[i] - p_min[i];
      if (genislik > en_genis) { en_genis = genislik; en_cok = (int8_t)i; }
    }
    if (en_cok >= 0 && en_genis >= OYNAMA_ESIGI) {
      son_oynama_ms = simdi;
      Serial.print(F(">>> CH")); Serial.print(en_cok + 1);
      Serial.print(F(" oynadi  ")); Serial.print(p_min[en_cok]);
      Serial.print(F(" .. "));      Serial.println(p_max[en_cok]);
    }
    pencereyi_sifirla();
  }

  const uint32_t sessiz  = simdi - son_cerceve_ms;
  const uint32_t degisim = simdi - son_oynama_ms;   // son oynamadan beri (bilgi)

  if (bozuk != onceki_bozuk) { onceki_bozuk = bozuk; bozuk_led_ms = simdi; }

  digitalWrite(LED_GREEN, hz > 50);                       // veri akiyor
  digitalWrite(LED_RED,   simdi - bozuk_led_ms < 1000);   // yeni BOZUK cerceve

  // ---- pano satiri ----
  if (simdi - t_yaz >= 100) {
    t_yaz = simdi;
    Serial.print(F("D"));
    for (uint8_t i = 0; i < 10; i++) {           // vericide 10 kanal acik
      Serial.print(F(" ch")); Serial.print(i + 1);
      Serial.print('=');      Serial.print(kanal[i]);
    }
    Serial.print(F(" hz="));     Serial.print(hz);
    Serial.print(F(" saglam=")); Serial.print(saglam);
    Serial.print(F(" bozuk="));  Serial.print(bozuk);
    Serial.print(F(" sessiz="));  Serial.print(sessiz  > 9999 ? 9999 : sessiz);
    Serial.print(F(" degisim=")); Serial.println(degisim > 99999 ? 99999 : degisim);
  }

  // ---- panodan gelen komut ----
  if (Serial.available()) {
    const char k = (char)Serial.read();
    if (k == 's') sayaclari_sifirla();
  }
}
