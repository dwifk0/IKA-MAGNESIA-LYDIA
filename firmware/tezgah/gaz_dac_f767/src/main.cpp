// GAZ — F767ZI dahili DAC tezgah testi.  PA4 = DAC1_OUT1 (CN11-32, D24)
//
// MCP4725 dusuruldu; gaz artik dahili DAC'tan. Bu test kontrolcuye BAGLANMADAN,
// yalnizca multimetreyle yapilir: yazilan ham deger ile olculen volt tutuyor mu?
//
// 🔴 Eski kalibrasyon GECERSIZ: `THROTTLE_DAC_VREF 5.0` ve MCP4725 sayilari
// 5 V referansa goreydi. Yeni formul:  ham = (volt / VREF) * 4095,  VREF = VDDA ~ 3,3 V
//
// Kontrolcunun istedigi aralik: 0,80 V = %0 · 4,20 V = %100
//   rolanti 0,80 -> 993 · %30 guc 1,82 -> 2259 · %45 guc 2,33 -> 2892
//   DAC tavani ~3,1 V = tam gazin ~%68. Ustune cikilamaz (cikis VREF+'i asamaz).
//
// OLCULECEK UC SEY (hicbiri bugune kadar olculmedi):
//   1. VDDA gercekte kac volt?  -> 3V3 pini (CN8-7) ile GND arasi. Formulun tabani.
//   2. DAC'in GERCEK tavani     -> ham 4095 yazilir, olculur. 3,30 degil ~3,1 cikar.
//   3. DAC'in GERCEK tabani     -> ham 0 yazilir. Tamponlu cikis 0 V'a inmez, ~0,2 V.
//
// ⚠ Kontrolcu bagli DEGILKEN yapilacak. Bagliyken bu sketch gaz verir.

#include <Arduino.h>

static const uint32_t GAZ_PIN = PA4;
static const float    VREF    = 3.28f;      // OLCULDU 30 Agu: AVDD (CN12-7) = 3,28 V

static uint16_t ham = 0;

static uint16_t volt_to_ham(float v) {
  float h = (v / VREF) * 4095.0f;
  if (h < 0)    h = 0;
  if (h > 4095) h = 4095;
  return (uint16_t)h;
}

static void yaz(uint16_t deger, const __FlashStringHelper *ne) {
  if (deger > 4095) deger = 4095;
  ham = deger;
  analogWrite(GAZ_PIN, ham);
  // mV'yi tam sayi olarak bildir (newlib-nano'da float printf kapali)
  const uint32_t mv = (uint32_t)((ham * VREF * 1000.0f) / 4095.0f);
  Serial.print(F("yazildi ham=")); Serial.print(ham);
  Serial.print(F("  beklenen=")); Serial.print(mv); Serial.print(F(" mV  <- "));
  Serial.println(ne);
}

void setup() {
  Serial.begin(115200);
  analogWriteResolution(12);
  pinMode(LED_GREEN, OUTPUT);

  Serial.println();
  Serial.println(F("GAZ DAC testi — PA4 (CN11-32)"));
  Serial.println(F("⚠ Kontrolcu BAGLI OLMAMALI."));
  Serial.println(F("Multimetre: PA4 (CN11-32) <-> GND (CN8-11)"));
  Serial.println(F("Once VDDA'yi olc: 3V3 (CN8-7) <-> GND. Formulun tabani o."));
  yaz(0, F("baslangic - taban olcumu"));
}

void loop() {
  digitalWrite(LED_GREEN, (millis() % 1000) < 500);

  if (Serial.available()) {
    String s = Serial.readStringUntil('\n');
    s.trim();
    if (s.length() == 0) return;

    const char k = s[0];
    if (k == '0')      yaz(0,                    F("TABAN - gercek min olculecek"));
    else if (k == 'r') yaz(volt_to_ham(0.80f),   F("rolanti 0,80 V"));
    else if (k == 'd') yaz(volt_to_ham(1.82f),   F("%30 guc 1,82 V"));
    else if (k == 'k') yaz(volt_to_ham(2.33f),   F("%45 guc 2,33 V"));
    else if (k == 't') yaz(4095,                 F("TAVAN - gercek max olculecek"));
    else if (k == 'h') {                          // h:<ham deger>
      const int i = s.indexOf(':');
      if (i > 0) yaz((uint16_t)s.substring(i + 1).toInt(), F("elle"));
    }
  }

  static uint32_t t = 0;
  if (millis() - t >= 200) {
    t = millis();
    const uint32_t mv = (uint32_t)((ham * VREF * 1000.0f) / 4095.0f);
    Serial.print(F("D ham=")); Serial.print(ham);
    Serial.print(F(" bekle_mv=")); Serial.println(mv);
  }
}
