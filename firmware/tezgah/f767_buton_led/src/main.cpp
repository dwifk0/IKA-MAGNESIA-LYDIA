// F767ZI — USER butonu ile kart üstü LED.
//
// Amaç sadece "ışık yandı" değil: kartın canlı olduğunu, yükleme yolunun
// çalıştığını ve saatin doğru kurulduğunu birbirinden AYIRT EDEREK göstermek.
//
//   LD2 (mavi)     : kalp atışı, 1 Hz. Firmware koşuyor mu?
//   LD3 (kırmızı)  : butonun ANLIK durumu — basılıyken yanar.
//   LD1 (yeşil)    : her basışta durum değiştirir (asıl istenen).
//
// Böylece arıza kendini ele veriyor:
//   mavi yanmıyor           -> firmware yüklenmedi / kart beslenmiyor
//   mavi yanıyor, kırmızı yok -> buton okunmuyor (polarite / pin)
//   kırmızı var, yeşil dönmüyor -> kenar yakalama / gürültü
//
// Buton polaritesi ÖLÇÜLMÜYOR, ÖĞRENİLİYOR: açılışta butona basılmadığı
// varsayılır, o andaki seviye "serbest" kabul edilir, tersi "basılı" olur.
// Kartın B1 butonunda pull-up mı pull-down mı takılı olduğunu bilmek gerekmez.

#include <Arduino.h>

static const uint32_t ZIPLAMA_MS = 30;   // kontak zıplaması penceresi

static int  serbest_seviye = HIGH;       // açılışta öğrenilir
static int  kararli        = HIGH;
static int  son_ham        = HIGH;
static uint32_t son_degisim = 0;
static uint32_t basma_sayisi = 0;
static bool yesil = false;

void setup() {
  Serial.begin(115200);

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE,  OUTPUT);
  pinMode(LED_RED,   OUTPUT);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_BLUE,  LOW);
  digitalWrite(LED_RED,   LOW);

  // Kartta B1 için harici direnç takılı; dahili pull-up/pull-down ile
  // çekişmemek için düz INPUT.
  pinMode(USER_BTN, INPUT);

  // Açılışta üç LED sırayla yansın: gözle "yeni firmware yüklendi" kanıtı.
  const int sira[3] = { LED_GREEN, LED_BLUE, LED_RED };
  for (int i = 0; i < 3; i++) {
    digitalWrite(sira[i], HIGH); delay(150); digitalWrite(sira[i], LOW);
  }

  delay(50);
  serbest_seviye = digitalRead(USER_BTN);   // butona basılmadığı varsayılıyor
  kararli = son_ham = serbest_seviye;

  Serial.println();
  Serial.println(F("F767ZI - USER butonu / LED testi"));
  Serial.print  (F("Saat: ")); Serial.print(SystemCoreClock / 1000000UL);
  Serial.println(F(" MHz   [beklenen: 216]"));
  Serial.print  (F("Buton serbest seviyesi: "));
  Serial.println(serbest_seviye == HIGH ? F("HIGH (basinca LOW)")
                                        : F("LOW (basinca HIGH)"));
  Serial.println(F("Butona bas: yesil LED durum degistirir."));
}

void loop() {
  // --- kalp atisi: 1 Hz, butondan bagimsiz ---
  digitalWrite(LED_BLUE, (millis() % 1000) < 500);

  // --- buton: zıplama filtresi + kenar yakalama ---
  const int ham = digitalRead(USER_BTN);
  if (ham != son_ham) { son_ham = ham; son_degisim = millis(); }

  if (millis() - son_degisim > ZIPLAMA_MS && ham != kararli) {
    kararli = ham;
    const bool basili = (kararli != serbest_seviye);

    digitalWrite(LED_RED, basili);

    if (basili) {                       // yalnız basma kenarında
      yesil = !yesil;
      digitalWrite(LED_GREEN, yesil);
      basma_sayisi++;
      Serial.print(F("D basma=")); Serial.print(basma_sayisi);
      Serial.print(F(" yesil="));  Serial.println(yesil ? 1 : 0);
    }
  }
}
