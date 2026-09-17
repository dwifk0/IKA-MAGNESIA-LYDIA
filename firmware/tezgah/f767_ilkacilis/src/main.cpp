// F767ZI — ilk açılış / yükleme yolu doğrulaması.
// Karta HİÇBİR ŞEY bağlı olmadan çalıştırılır.
//   Görsel ölçüt : üç LED sırayla yanıp söner (yeşil → mavi → kırmızı)
//   Seri ölçüt   : ST-LINK sanal COM portundan saniyede bir satır düşer
// Serial = USART3 (PD8/PD9) = ST-LINK VCP. Ek kablo yok, CN1'deki USB yeter.

#include <Arduino.h>

// Nucleo-144 kart üstü kullanıcı LED'leri
#define LD1_YESIL   PB0
#define LD2_MAVI    PB7
#define LD3_KIRMIZI PB14

static const int leds[3] = { LD1_YESIL, LD2_MAVI, LD3_KIRMIZI };
static uint32_t tik = 0;

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 3; i++) { pinMode(leds[i], OUTPUT); digitalWrite(leds[i], LOW); }

  delay(500);
  Serial.println();
  Serial.println(F("=================================="));
  Serial.println(F(" F767ZI CANLI — ilk acilis testi"));
  Serial.println(F("=================================="));
  Serial.print(F("Saat (SystemCoreClock): "));
  Serial.print(SystemCoreClock / 1000000UL);
  Serial.println(F(" MHz   [beklenen: 216]"));
  Serial.print(F("Derleme: ")); Serial.print(F(__DATE__));
  Serial.print(' ');            Serial.println(F(__TIME__));
  Serial.println(F("Uc LED sirayla yanmali. Sayac basliyor..."));
  Serial.println();
}

void loop() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(leds[i], HIGH);
    delay(150);
    digitalWrite(leds[i], LOW);
  }
  Serial.print(F("tik "));
  Serial.print(++tik);
  Serial.print(F("   calisma suresi "));
  Serial.print(millis() / 1000);
  Serial.println(F(" sn"));
  delay(550);
}
