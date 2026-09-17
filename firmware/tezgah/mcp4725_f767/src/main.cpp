// MCP4725 tezgah testi — NUCLEO-F767ZI, I2C1 (SCL=PB8, SDA=PB9)
//
// ⚠ VOUT KONTROLCUYE BAGLANMAYACAK. Bu test yalnizca multimetreyle yapilir.
//
// Testin ILK isi haberlesmeyi dogrulamak degil, EEPROM'u OKUMAK:
// MCP4725 guc gelir gelmez EEPROM'undaki degeri cikisa basiyor — firmware daha
// calismadan. Orada ne yazili oldugunu bilmeden bu cip gaz hattina baglanamaz.
//
// Okunan 5 bayt (MCP4725 datasheet):
//   [0] durum : bit7 BSY · bit6 POR · bit2-1 PD
//   [1][2]    : DAC yazmaci  (12 bit, sola dayali)
//   [3][4]    : EEPROM       ([3] bit2-0 = D11..D8 · [4] = D7..D0)

#include <Arduino.h>
#include <Wire.h>

static const uint8_t ADRESLER[2] = { 0x60, 0x61 };
static uint8_t  adres = 0x60;
static bool     bulundu = false;

// UBEC olculunce degistirilecek. Kalibrasyonun TABANI bu.
static float VDD = 4.50f;

static uint16_t son_yazilan = 0;

static uint16_t volt_to_ham(float v) {
  float h = (v / VDD) * 4095.0f;
  if (h < 0) h = 0;
  if (h > 4095) h = 4095;
  return (uint16_t)h;
}

static void tara() {
  Serial.println(F("I2C taramasi..."));
  bulundu = false;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("  cihaz 0x")); Serial.println(a, HEX);
      for (uint8_t i = 0; i < 2; i++)
        if (a == ADRESLER[i]) { adres = a; bulundu = true; }
    }
  }
  if (bulundu) { Serial.print(F("MCP4725 bulundu: 0x")); Serial.println(adres, HEX); }
  else Serial.println(F("MCP4725 YOK — pull-up / VDD / kablo bak"));
}

// DAC yazmacina yaz (kalici degil, EEPROM'a dokunmaz)
static bool yaz(uint16_t deger) {
  if (deger > 4095) deger = 4095;
  Wire.beginTransmission(adres);
  Wire.write(0x40);                      // "write DAC register"
  Wire.write((uint8_t)(deger >> 4));
  Wire.write((uint8_t)((deger & 0x0F) << 4));
  const uint8_t s = Wire.endTransmission();
  if (s == 0) {
    son_yazilan = deger;
    const uint32_t mv = (uint32_t)((deger * VDD * 1000.0f) / 4095.0f);
    Serial.print(F("yazildi ham=")); Serial.print(deger);
    Serial.print(F("  beklenen=")); Serial.print(mv); Serial.println(F(" mV"));
  } else {
    Serial.print(F("YAZILAMADI — Wire hatasi ")); Serial.println(s);
  }
  return s == 0;
}

// EEPROM'a yaz — KALICI. Cip bundan sonra her aciliste bu degerle baslar.
static void eeprom_yaz(uint16_t deger) {
  if (deger > 4095) deger = 4095;
  Wire.beginTransmission(adres);
  Wire.write(0x60);                      // "write DAC register and EEPROM"
  Wire.write((uint8_t)(deger >> 4));
  Wire.write((uint8_t)((deger & 0x0F) << 4));
  const uint8_t s = Wire.endTransmission();
  if (s) { Serial.print(F("EEPROM YAZILAMADI, hata ")); Serial.println(s); return; }
  delay(60);                             // EEPROM yazma suresi (tipik 25-50 ms)
  son_yazilan = deger;
  Serial.print(F("EEPROM'a yazildi: ham=")); Serial.print(deger);
  Serial.println(F("  — cip artik her aciliste bu degerle baslayacak"));
}

static void oku(bool bas) {
  const uint8_t n = Wire.requestFrom((int)adres, 5);
  if (n != 5) { if (bas) Serial.println(F("okunamadi")); return; }
  uint8_t b[5];
  for (uint8_t i = 0; i < 5; i++) b[i] = Wire.read();

  const uint16_t dac = ((uint16_t)b[1] << 4) | (b[2] >> 4);
  const uint16_t eep = (((uint16_t)(b[3] & 0x0F)) << 8) | b[4];
  const uint32_t dac_mv = (uint32_t)((dac * VDD * 1000.0f) / 4095.0f);
  const uint32_t eep_mv = (uint32_t)((eep * VDD * 1000.0f) / 4095.0f);

  if (bas) {
    Serial.print(F("durum=0x")); Serial.print(b[0], HEX);
    Serial.print(F("  POR=")); Serial.print((b[0] & 0x40) ? 1 : 0);
    Serial.print(F("  DAC=")); Serial.print(dac); Serial.print(F(" (")); Serial.print(dac_mv);
    Serial.print(F(" mV)  EEPROM=")); Serial.print(eep); Serial.print(F(" (")); Serial.print(eep_mv);
    Serial.println(F(" mV)"));
    if (eep_mv > 1000) {
      Serial.println(F("🔴 EEPROM ROLANTININ USTUNDE — bu cip su an gaz hattina BAGLANAMAZ."));
      Serial.println(F("   'e' ile EEPROM'a rolanti yaz."));
    }
  }
  Serial.print(F("D dac=")); Serial.print(dac);
  Serial.print(F(" dac_mv=")); Serial.print(dac_mv);
  Serial.print(F(" eep=")); Serial.print(eep);
  Serial.print(F(" eep_mv=")); Serial.print(eep_mv);
  Serial.print(F(" bagli=")); Serial.println(bulundu ? 1 : 0);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED,   OUTPUT);

  Wire.setSCL(PB8);
  Wire.setSDA(PB9);
  Wire.begin();
  Wire.setClock(100000);

  delay(200);
  Serial.println();
  Serial.println(F("MCP4725 testi — I2C1  SCL=PB8  SDA=PB9"));
  Serial.println(F("⚠ VOUT kontrolcuye BAGLI OLMAMALI. Multimetre: VOUT <-> GND"));
  Serial.print  (F("VDD varsayimi: ")); Serial.print((int)(VDD * 100));
  Serial.println(F(" /100 V — UBEC olculunce duzeltilecek"));
  tara();
  if (bulundu) { Serial.println(F("--- ilk okuma (EEPROM dahil) ---")); oku(true); }
  Serial.println(F("Komutlar: t=tara  o=oku  r=rolanti  d=%30  k=%45  m=tam gaz  e=EEPROM'a rolanti"));
}

void loop() {
  digitalWrite(LED_GREEN, bulundu && ((millis() % 1000) < 500));
  digitalWrite(LED_RED,   !bulundu);

  if (Serial.available()) {
    String s = Serial.readStringUntil('\n'); s.trim();
    if (s.length()) {
      const char k = s[0];
      if      (k == 't') tara();
      else if (k == 'o') oku(true);
      else if (k == 'r') yaz(volt_to_ham(0.80f));
      else if (k == 'd') yaz(volt_to_ham(1.82f));
      else if (k == 'k') yaz(volt_to_ham(2.33f));
      else if (k == 'm') yaz(volt_to_ham(4.20f));
      else if (k == 'e') eeprom_yaz(volt_to_ham(0.80f));
      else if (k == 'h') { const int i = s.indexOf(':');
                           if (i > 0) yaz((uint16_t)s.substring(i + 1).toInt()); }
    }
  }

  static uint32_t t = 0;
  if (millis() - t >= 500) { t = millis(); if (bulundu) oku(false); }
}
