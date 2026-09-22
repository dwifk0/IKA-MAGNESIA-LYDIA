// SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
// Copyright (C) 2026 Ahmet Efe Nezli
// ACIL STOP tezgah testi — mantar butonun NC blogu -> PF14 (INPUT_PULLUP)
//
// Kablolama: NC blogunun `21` -> GND (CN8-11) · `22` -> PF14 (CN12-50, D4).
// Arada eleman YOK; pull-up kartin icinde.  (genel/ACIL_STOP.md)
//
//   buton birakilmis  -> kontak KAPALI -> pin LOW   = normal
//   buton basili      -> kontak ACIK   -> pin HIGH  = E-STOP
//   kablo kopuk       -> kontak ACIK   -> pin HIGH  = E-STOP  (ariza guvenli tarafa)
//
// 🔴 Bu polarite 24 Agustos'ta TERS CEVRILDI ve bugune kadar hic denenmedi.
// Eski Mega kodu `LOW = basili` bekliyordu; NC dogru baglanirsa arac acilista
// surekli E-STOP'ta kalirdi. Testin ilk isi bunu yerinde dogrulamak.
//
// "Basinca duruyor" KOLAY ve YETERSIZ yesil isik. Asil sorular:
//   1. Birakinca kurtuluyor mu?  (mandal aciktan once kendiliginden surmemeli)
//   2. Kopuk kablo tetikliyor mu? (soketi cek — HIGH okumali)
//   3. Kontak ziplamasi ne kadar? -> 100 nF gercekten gerekli mi (hic olculmemisti)

#include <Arduino.h>

static const uint32_t ZIPLAMA_MS = 30;      // kararlilik penceresi

static const int SERBEST = LOW;             // kontak kapali
static const int BASILI  = HIGH;            // kontak acik / kablo kopuk

static int      ham        = SERBEST;
static int      son_ham    = SERBEST;
static int      kararli    = SERBEST;
static uint32_t son_kenar  = 0;
static bool     mandal     = false;         // bir kez basildi mi (elle sifirlanir)

static uint32_t basma      = 0;
static uint32_t ziplama    = 0;             // son gecisteki kenar sayisi
static uint32_t ziplama_en = 0;             // gorulen en kotu zıplama
static uint32_t ziplama_ms = 0;             // son gecisin ne kadar surdugu
static uint32_t en_uzun_ms = 0;
static bool     gecis_var  = false;
static uint32_t gecis_bas  = 0;
static uint32_t t_yaz      = 0;

void setup() {
  Serial.begin(115200);
  pinMode(PF14, INPUT_PULLUP);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED,   OUTPUT);

  delay(50);
  ham = son_ham = kararli = digitalRead(PF14);

  Serial.println();
  Serial.println(F("ACIL STOP testi — PF14, INPUT_PULLUP"));
  Serial.println(F("LOW = birakilmis (normal) · HIGH = basili VEYA kablo kopuk"));
  Serial.print  (F("Acilistaki okuma: "));
  Serial.println(kararli == BASILI ? F("HIGH  -> buton basili ya da kablo yok")
                                   : F("LOW   -> normal"));
  Serial.println(F("Sirasiyla dene: (1) bas  (2) birak  (3) soketi cek"));
}

void loop() {
  const uint32_t simdi = millis();
  ham = digitalRead(PF14);

  // --- her ham kenar: ziplama sayaci ---
  if (ham != son_ham) {
    son_ham = ham;
    son_kenar = simdi;
    if (!gecis_var) { gecis_var = true; gecis_bas = simdi; ziplama = 0; }
    ziplama++;
  }

  // --- ZIPLAMA_MS boyunca kararli kaldiysa gecisi kabul et ---
  if (gecis_var && (simdi - son_kenar > ZIPLAMA_MS) && ham != kararli) {
    kararli = ham;
    gecis_var = false;
    ziplama_ms = son_kenar - gecis_bas;          // zıplamanin surdugu sure
    if (ziplama > ziplama_en) ziplama_en = ziplama;
    if (ziplama_ms > en_uzun_ms) en_uzun_ms = ziplama_ms;

    if (kararli == BASILI) {
      mandal = true;
      basma++;
      Serial.print(F("E-STOP -> BASILI  (kenar=")); Serial.print(ziplama);
      Serial.print(F("  sure=")); Serial.print(ziplama_ms); Serial.println(F(" ms)"));
    } else {
      Serial.print(F("kontak geri kapandi — MANDAL DURUYOR (kenar="));
      Serial.print(ziplama); Serial.println(F(")"));
    }
  }
  if (gecis_var && (simdi - son_kenar > ZIPLAMA_MS) && ham == kararli) {
    gecis_var = false;                            // gurultu, gecis olmadi
  }

  digitalWrite(LED_RED,   kararli == BASILI);     // anlik durum
  digitalWrite(LED_GREEN, !mandal);               // mandal acikken soner

  if (simdi - t_yaz >= 100) {
    t_yaz = simdi;
    Serial.print(F("D ham="));      Serial.print(ham == BASILI ? 1 : 0);
    Serial.print(F(" estop="));     Serial.print(kararli == BASILI ? 1 : 0);
    Serial.print(F(" mandal="));    Serial.print(mandal ? 1 : 0);
    Serial.print(F(" basma="));     Serial.print(basma);
    Serial.print(F(" ziplama="));   Serial.print(ziplama_en);
    Serial.print(F(" ziplama_ms=")); Serial.println(en_uzun_ms);
  }

  if (Serial.available()) {
    const char k = (char)Serial.read();
    if (k == 'r') {
      if (kararli == BASILI) {
        Serial.println(F("RED: buton hala basili — mandal acilmaz"));
      } else {
        mandal = false;
        Serial.println(F("mandal acildi"));
      }
    } else if (k == 's') {
      basma = ziplama_en = en_uzun_ms = 0;
      Serial.println(F("sayaclar sifirlandi"));
    }
  }
}
