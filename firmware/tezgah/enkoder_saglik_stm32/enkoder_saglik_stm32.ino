// =====================================================================
//  ARTIMLI ENKODER SAGLIK TESTI          (WeAct BlackPill, STM32F411CE)
//
//  Sponsordan gelen "ROTARY ENCODER 300B-5-30FG2" icin.
//  Enkoder 4 telli: Kirmizi=Vcc, Siyah=0V, Yesil=A, Beyaz=B.
//  Z (index) kanali YOK -> tur olcumu elle yapilir (safta isaret koy).
//
//  NEDEN BU KART: PB6 ve PB7, STM32F4 datasheet'inde FT (five-volt
//  tolerant) isaretli. Enkoder 5 V ile beslendiginde cikisi push-pull
//  ise sinyal de 5 V olur; FT pin bunu sorunsuz kaldirir. Cikis acik
//  kollektor ise dahili pull-up (3,3 V) isi gorur. Iki durumda da
//  arada hicbir eleman yok -> direnc/seviye cevirici GEREKMIYOR.
//
//  ENKODER BAGLANTISI:
//     Kirmizi (Vcc) -> BlackPill 5V     (USB-C takiliyken VBUS)
//     Siyah   (0V)  -> BlackPill G
//     Yesil   (A)   -> PB6
//     Beyaz   (B)   -> PB7
//
//  ####  Kirmiziyi 12 V'a VERME. O zaman A/B'den de 12 V cikar    ####
//  ####  ve FT pinler bile bunu kaldirmaz.                        ####
//
//  CIKTI IKI YERE BIRDEN GIDER (hangisi calisirsa):
//     1) USB CDC  -> USB-C kablosu (VERI TASIYAN bir kablo olmali)
//     2) USART1   -> PA9 = TX, PA10 = RX
//        CP2102/USB-TTL ile:  PA9 -> adaptorun RXD
//                             GND -> adaptorun GND   (ortak GND sart)
//        Adaptorun TXD'sini PA10'a baglamak yalnizca komut yollamak
//        icin gerekir; sadece izlemek istiyorsan tek tel + GND yeter.
//        ADAPTORU 3,3 V MODUNDA KULLAN, 5 V TX PA10'a girmesin.
//
//  YUKLEME: ST-Link V2 -> kartin SWD basligi (SWCLK, SWDIO, GND).
//     ST-Link'in 3.3V/5V pinleri BAGLANMAZ; kart USB-C'den beslenir.
//
//  KOMUTLAR (115200, iki porttan da kabul edilir):
//     s = sifirla   d = durum   p = ham seviye 5 sn
//  Her 100 ms'de pano icin makine okunur satir:
//     D sayim=.. hatali=.. sonhata=.. hiz10=.. A=.. B=.. t=..
//  hiz10 = sayim/saniye'nin ONDA BIRI (tam sayi). Ondalik bicimleme
//  newlib-nano'da varsayilan olarak kapali oldugu icin boyle.
// =====================================================================

const uint32_t PIN_A = PB6;
const uint32_t PIN_B = PB7;

// Etiketin "Pulse" satiri parmakla kapaliydi; 300 yalnizca model
// adindan tahmin. Testin yan ciktisi bunu dogrulayacak.
const long PPR      = 300;
const long TUR_BASI = PPR * 4;   // 4x cozumleme

volatile long     sayim       = 0;
volatile uint32_t hataliGecis = 0;   // <-- ASIL KALITE OLCUTU
volatile uint32_t sonHataMs   = 0;
volatile uint8_t  eskiDurum   = 0;

// Quadrature 4x cozumleme tablosu. Indeks = (eski<<2)|yeni,
// durum = (A<<1)|B. Deger 2 = YASAK GECIS (iki bit birden degismis)
// yani bir darbe kacirilmis: kirli disk, gevsek kablo, gurultu ya da
// asiri hiz. Saglam bir enkoderde bu sayac SIFIR kalir.
const int8_t TABLO[16] = {
  0, 1, -1, 2,
 -1, 0,  2, 1,
  1, 2,  0,-1,
  2,-1,  1, 0
};

// ---- ciktiyi iki porta birden bas ----
template <typename T> void bas(T v)   { Serial.print(v);   Serial1.print(v); }
template <typename T> void basln(T v) { Serial.println(v); Serial1.println(v); }
void basln()                          { Serial.println();  Serial1.println(); }
void bas(float v, int d)              { Serial.print(v, d); Serial1.print(v, d); }

// ---- komutu iki porttan da oku ----
int komutOku() {
  if (Serial.available())  return Serial.read();
  if (Serial1.available()) return Serial1.read();
  return -1;
}
void komutArtiginiAt() {
  while (Serial.available()  && Serial.peek()  < ' ') Serial.read();
  while (Serial1.available() && Serial1.peek() < ' ') Serial1.read();
}

void isrAB() {
  uint8_t yeni = (digitalRead(PIN_A) << 1) | digitalRead(PIN_B);
  int8_t  adim = TABLO[(eskiDurum << 2) | yeni];
  if (adim == 2) { hataliGecis++; sonHataMs = millis(); }
  else           { sayim += adim; }
  eskiDurum = yeni;
}

void setup() {
  Serial.begin(115200);    // USB CDC
  Serial1.begin(115200);   // USART1: PA9 = TX, PA10 = RX

  // CDC: terminal acilmasa da kart calissin diye zaman asimli bekleme
  uint32_t t0 = millis();
  while (!Serial && (millis() - t0) < 3000) { ; }

  // Acik kollektor cikisi da kurtarir; push-pull'da zararsiz.
  pinMode(PIN_A, INPUT_PULLUP);
  pinMode(PIN_B, INPUT_PULLUP);
  delay(50);

  eskiDurum = (digitalRead(PIN_A) << 1) | digitalRead(PIN_B);

  // PB6 -> EXTI6, PB7 -> EXTI7. Ayri hatlar, catisma yok.
  attachInterrupt(digitalPinToInterrupt(PIN_A), isrAB, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_B), isrAB, CHANGE);

  basln();
  basln(F("=== ENKODER SAGLIK TESTI (BlackPill F411CE) ==="));
  bas  (F("Varsayilan: ")); bas(PPR);
  bas  (F(" P/R -> tam turda ")); bas(TUR_BASI);
  basln(F(" sayim (4x)"));
  bas  (F("Acilis seviyeleri  A=")); bas(digitalRead(PIN_A));
  bas  (F("  B="));                  basln(digitalRead(PIN_B));
  basln(F("Komutlar: s=sifirla  d=durum  p=ham seviye"));
  basln(F("Safti YAVASCA cevir."));
  basln();
}

void durumBas() {
  noInterrupts();
  long     s  = sayim;
  uint32_t hg = hataliGecis;
  interrupts();

  bas(F("sayim="));    bas(s);
  bas(F("  tur="));    bas((float)s / (float)TUR_BASI, 3);
  bas(F("  HATALI=")); bas(hg);
  basln(hg == 0 ? F("   [temiz]") : F("   [!! DARBE KACIRIYOR]"));
}

void hamBas() {
  basln(F("-- 5 sn ham seviye (safti cevir, degismeli) --"));
  uint32_t bitis = millis() + 5000;
  while (millis() < bitis) {
    bas(F("A="));  bas(digitalRead(PIN_A));
    bas(F(" B=")); basln(digitalRead(PIN_B));
    delay(120);
  }
  basln(F("-- bitti --"));
  basln(F("A ve B hep AYNI degeri gosteriyorsa iki tel ayni yere bagli."));
  basln(F("Biri hic degismiyorsa o kanal olu ya da kablosu kopuk."));
}

void loop() {
  static uint32_t sonBas   = 0;
  static long     oncekiS  = 0;
  static uint32_t oncekiMs = 0;

  int k = komutOku();
  if (k >= 0) {
    komutArtiginiAt();
    switch (k) {
      case 's':
        noInterrupts();
        sayim = 0; hataliGecis = 0; sonHataMs = 0;
        eskiDurum = (digitalRead(PIN_A) << 1) | digitalRead(PIN_B);
        interrupts();
        oncekiS = 0;
        basln(F("sifirlandi"));
        break;
      case 'd': durumBas(); break;
      case 'p': hamBas();   break;
    }
  }

  uint32_t simdi = millis();
  if (simdi - sonBas >= 100) {
    noInterrupts();
    long     s  = sayim;
    uint32_t hg = hataliGecis;
    uint32_t sh = sonHataMs;
    interrupts();

    float dt  = (simdi - oncekiMs) / 1000.0f;
    float hiz = (dt > 0.0f) ? (s - oncekiS) / dt : 0.0f;
    oncekiS  = s;
    oncekiMs = simdi;
    sonBas   = simdi;

    // Pano bu satiri ayristirir. Ondalik yok: hiz onda bir biriminde.
    char buf[110];
    snprintf(buf, sizeof buf,
             "D sayim=%ld hatali=%lu sonhata=%lu hiz10=%ld A=%d B=%d t=%lu",
             s, (unsigned long)hg, (unsigned long)sh,
             (long)lroundf(hiz * 10.0f),
             (int)digitalRead(PIN_A), (int)digitalRead(PIN_B),
             (unsigned long)simdi);
    basln(buf);
  }
}
