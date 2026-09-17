// F767ZI PIN TARAYICI — "veri var mi, hangi pinde?"
//
// 29 Agustos'ta Mega'da yanlis izi bu yontem cozmustu: iBUS sinyali A0'daydi,
// bakilan pin bostu ve 50 Hz sebeke ugultusunu veri saniyorduk. Bostaki pinin
// imzasi ile gercek seri verinin imzasi tamamen farkli:
//
//   bosta / ugultu : ~0-5 kenar, en kisa darbe ~ms mertebesi
//   115200 UART    : yuzlerce kenar, en kisa darbe ~9 us (1 bit = 8,68 us)
//
// Her pini 30 ms dinler, kenar sayar, en kisa darbeyi olcer.
// Sadece hareketli pinleri basar; bir sey bulunamazsa acikca soyler.

#include <Arduino.h>

static const uint32_t PENCERE_US = 30000;   // pin basina dinleme suresi
static const uint16_t ESIK_KENAR = 50;      // bunun altini gurultu say

// Dokunulmayacaklar: ST-LINK sanal COM portu (PD8/PD9), SWD (PA13/PA14),
// osilator (PH0/PH1), kart ustu LED'ler ve USER butonu.
static bool yasak(uint32_t pin) {
  return pin == PD8 || pin == PD9 || pin == PA13 || pin == PA14 ||
         pin == PH0 || pin == PH1 ||
         pin == PB0 || pin == PB7 || pin == PB14 || pin == PC13;
}

static void pin_adi(uint32_t p) {
  const PinName pn = digitalPinToPinName(p);
  Serial.print('P');
  Serial.print((char)('A' + (STM_PORT(pn))));
  Serial.print(STM_PIN(pn));
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println(F("F767ZI PIN TARAYICI"));
  Serial.println(F("Her tur butun pinleri tarar. Kabloyu oynatip tekrar bak."));
  Serial.println(F("115200 UART imzasi: yuzlerce kenar + en kisa darbe ~9 us"));
  Serial.println();
}

void loop() {
  Serial.println(F("--- tarama ---"));
  uint8_t bulunan = 0;

  for (uint32_t p = 0; p < NUM_DIGITAL_PINS; p++) {
    if (!digitalPinIsValid(p) || yasak(p)) continue;

    pinMode(p, INPUT);

    uint32_t kenar = 0;
    uint32_t en_kisa = 0xFFFFFFFF;
    int son = digitalRead(p);
    uint32_t t_son = micros();
    const uint32_t bitis = t_son + PENCERE_US;
    uint32_t yuksek = 0, ornek = 0;

    while ((int32_t)(micros() - bitis) < 0) {
      const int simdi = digitalRead(p);
      ornek++; if (simdi) yuksek++;
      if (simdi != son) {
        const uint32_t t = micros();
        const uint32_t sure = t - t_son;
        if (sure < en_kisa) en_kisa = sure;
        t_son = t; son = simdi; kenar++;
      }
    }

    if (kenar >= ESIK_KENAR) {
      bulunan++;
      Serial.print(F("  D")); Serial.print(p);
      Serial.print(F("  ")); pin_adi(p);
      Serial.print(F("   kenar=")); Serial.print(kenar);
      Serial.print(F("  en_kisa=")); Serial.print(en_kisa);
      Serial.print(F(" us  bosta="));
      Serial.print((yuksek * 100) / (ornek ? ornek : 1)); Serial.print(F("% HIGH"));
      if (en_kisa >= 6 && en_kisa <= 20) Serial.print(F("   <== 115200 UART gibi"));
      Serial.println();
    }
  }

  if (!bulunan) Serial.println(F("  hicbir pinde hareket yok - hatta sinyal gelmiyor"));
  Serial.println();
  delay(1500);
}
