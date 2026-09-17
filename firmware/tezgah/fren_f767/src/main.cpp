/*
 * fren_f767 — FREN AKTÜATÖRÜ + BTS7960B TEZGAH TESTİ (NUCLEO-F767ZI)
 *
 * `fren_mega`nin F767 portu. 30 Ağustos 2026'da Mega tasarımdan çıktı;
 * ölçümlerin kendisi değişmedi, kart ve pinler değişti.
 *
 * ── NEDEN BU TEST ───────────────────────────────────────────────────────────
 * Frende konum sensörü YOK ve ACS712 24 Ağustos'ta sistemden çıkarıldı.
 * Geriye tek koruma kaldı: `BRAKE_STALL_MS` zaman aşımı — ve o **2500 ms
 * tahmindir, ölçülmedi.** Kısa olursa fren yolun ortasında kesilir; uzun
 * olursa aktüatör uca dayanmış hâlde 2,5 saniye zorlanır.
 *
 * Dört ölçüm:
 *   1. FREN_TERS     — "uygula" komutu gerçekten frene basıyor mu
 *   2. BRAKE_PWM_MIN — aktüatörün kımıldadığı en düşük PWM (şu an 0 yazılı)
 *   3. Tam stroke süresi → BRAKE_STALL_MS bundan UZUN olmalı
 *   4. 🔴 F767'YE ÖZEL: köprü **3,3 V mantıkla** sürülüyor mu — Mega'da giriş
 *      5 V'tu, burada 3,3 V. BTS7960B'nin eşiği VCC'ye göre tanımlı ve bu
 *      nokta tasarımda **tek doğrulanmamış madde** olarak duruyor.
 *      Belirti: hiç dönmez, ya da tam PWM'de bile zayıf/kararsız döner.
 *      Çıkışı: `PWM_MIN` beklenenden çok yüksek çıkarsa şüphelen.
 *
 * ⚠⚠ GÜVENLİK — OKUMADAN BAĞLAMA
 *   • `B+` **12 V hattına** gider (araçta 4S LiPo). Tavanı ~27 V.
 *     **48 V'a bağlarsan modül anında ölür.**
 *   • **RPWM/LPWM/EN üçüne de 10 kΩ pull-down ŞART.** STM32 reset'ten
 *     `setup()`a kadar pinleri **giriş-yüzer** bırakır; o aralıkta köprüyü
 *     tutan tek şey bu dirençlerdir. (Mega'da da kuraldı, burada zorunlu.)
 *   • İki PWM kanalı ASLA aynı anda yüksek olmamalı — modül belgesinde
 *     karşılığı tek kelime: "BURN". Kod bunu sırayla yazarak engelliyor.
 *   • `B−` ile kartın `GND`si ORTAK olmalı, yoksa PWM'in referansı olmaz.
 *
 * ── DERLEME / YÜKLEME ───────────────────────────────────────────────────────
 *   cd ~/ika/testler/fren_f767
 *   ~/ika/testler/yukle.sh fren_f767        (derler + yukler)
 * 🔴 WSL'de ne `pio run -t upload` ne `cp ... /mnt/d/` calisir — yukle.sh
 *    kopyalamayi Windows'a yaptirir.
 * ⚠ Yüklemeden önce panoyu KAPAT — seri portu tutuyor. Port: COM6.
 */

#include <Arduino.h>

// ── PİNLER — araçtakiyle AYNI (BAGLANTI_HARITASI.md F767 tablosu) ───────────
const uint32_t RPWM_PIN = PC6;   // CN12-4  (D16) — "fren UYGULA" yonu
const uint32_t LPWM_PIN = PC7;   // CN12-19 (D21) — "fren SERBEST" yonu
const uint32_t EN_PIN   = PC8;   // CN12-2  (D43) — R_EN + L_EN kopruluk

// PC6/PC7 = TIM3_CH1/CH2 — ayni zamanlayicinin iki kanali, cakisma yok.
// TIM3 gosterge hatti (PB4) 30 Agu'da tasarimdan cikinca bosaldi.

// Akim okuma — BTS7960B'nin R_IS / L_IS uclari. OPSIYONEL.
// A0/A1 secildi: ADC123, Ethernet RMII'nin tuttugu pinlerin disinda, bos.
const uint32_t RIS_PIN = A0;     // PA3
const uint32_t LIS_PIN = A1;     // PC0
bool akim_bagli = false;         // 'a' komutuyla acilir

// ── AYARLAR — araçtakiyle AYNI ──────────────────────────────────────────────
// ⚠ TAM GÜÇ DEĞİL. Köprü 4S LiPo'dan besleniyor (14,8–16,8 V), aktüatör 12 V.
// 190/255 ≈ %75 → 16,8 V × 0,75 ≈ 12,6 V. Bu tavanı YÜKSELTME.
const uint8_t PWM_MAX = 190;
bool fren_ters = false;          // 'r' komutuyla cevrilir — 1. olcum bu

// Tezgah koruması: hicbir komut bundan uzun sure kopruyu surmez.
// Amac uca dayanmis aktuatoru gozetimsiz birakmamak.
// 🔴 31 Agu 2026: 8000 -> 20000. Ilk stroke olcumu 7571 ms cikti, yani 8 sn
// sinirina degmek uzereydi ve UCTAN UCA olcum onu kesin asacakti. Koruma
// olcumu kesmemeli; 20 sn hala gozetimsiz aktuatoru koruyacak kadar kisa.
const uint32_t KORUMA_MS = 20000;
uint32_t surus_bas = 0;          // 0 = surulmuyor

// ── Durum ───────────────────────────────────────────────────────────────────
int16_t  komut_binde = 0;        // -1000 serbest .. +1000 uygula
int8_t   yon_akt = 0;
uint8_t  pwm_akt = 0;
uint16_t akim_ham = 0, akim_tepe = 0;
uint16_t akim_mv  = 0;

// Otomatik PWM_MIN arama
bool     min_arama = false;
uint8_t  min_pwm = 0;
uint8_t  min_bulundu = 0;
uint32_t min_adim_t = 0;

// Stroke suresi olcumu
bool     stroke_olcum = false;
uint32_t stroke_bas = 0;
uint16_t stroke_ms = 0;

// ─────────────────────────────────────────────────────────────────────────────
// SIRA ÖNEMLİ: önce kapanacak kanal sıfırlanır, sonra öteki yazılır. Ters
// sırada iki kanal bir an birden yüksek kalır ve iki yarım köprü aynı anda
// iletime girer.
void pwm_yaz(int rpwm, int lpwm) {
  if (rpwm == 0) {
    analogWrite(RPWM_PIN, 0);
    analogWrite(LPWM_PIN, lpwm);
  } else {
    analogWrite(LPWM_PIN, 0);
    analogWrite(RPWM_PIN, rpwm);
  }
}

void kopru_dur() { pwm_yaz(0, 0); yon_akt = 0; pwm_akt = 0; surus_bas = 0; }

// hiz: +1 tam UYGULA, -1 tam SERBEST, 0 DUR (bulundugu yerde kalir —
// aktuator vidali, surus kesilince konumunu korur).
void fren_hiz_ham(float hiz) {
  if (hiz >  1.0f) hiz =  1.0f;
  if (hiz < -1.0f) hiz = -1.0f;
  if (fren_ters) hiz = -hiz;

  int8_t yon = (hiz > 0.02f) ? 1 : (hiz < -0.02f ? -1 : 0);
  if (yon == 0) { kopru_dur(); return; }

  uint8_t pwm = (uint8_t)(fabsf(hiz) * PWM_MAX);
  yon_akt = yon;
  pwm_akt = pwm;
  surus_bas = millis();
  if (yon > 0) pwm_yaz(pwm, 0);
  else         pwm_yaz(0, pwm);
}

// Ham PWM ile surer — PWM_MIN aramasi icin (0-255 dogrudan).
void ham_pwm_sur(uint8_t pwm, int8_t yon) {
  if (fren_ters) yon = -yon;
  yon_akt = yon; pwm_akt = pwm;
  surus_bas = millis();
  if (yon > 0) pwm_yaz(pwm, 0);
  else         pwm_yaz(0, pwm);
}

void akim_oku() {
  if (!akim_bagli) { akim_ham = 0; akim_mv = 0; return; }
  // Hangi yon suruluyorsa o tarafin IS ucu anlamli.
  akim_ham = (uint16_t)analogRead(yon_akt >= 0 ? RIS_PIN : LIS_PIN);
  // 12 bit, VDDA = 3,3 V.  ham -> mV
  akim_mv  = (uint16_t)(((uint32_t)akim_ham * 3300UL) / 4095UL);
  if (akim_ham > akim_tepe) akim_tepe = akim_ham;
}

// ─────────────────────────────────────────────────────────────────────────────
// Komutlar
//   f:<binde>  fren uygula (0..1000)      s:<binde>  serbest birak
//   d          DUR                        r          FREN_TERS'i cevir
//   m          PWM_MIN aramasini baslat   k          "KIMILDADI" isareti
//   t          stroke suresi olcumunu baslat   u   "DAYANDI" isareti
//   a          akim okumayi ac/kapa       z          sayaclari sifirla
void komut_isle(char *s) {
  char c = s[0];
  long v = 0;
  char *iki = strchr(s, ':');
  if (iki) v = atol(iki + 1);

  if (c == 'f' || c == 's') {
    min_arama = false; stroke_olcum = false;
    if (v <= 0) v = 1000;
    if (v > 1000) v = 1000;
    fren_hiz_ham((c == 'f' ? 1.0f : -1.0f) * (float)v / 1000.0f);
    komut_binde = (int16_t)((c == 'f') ? v : -v);
    Serial.print(F("# ")); Serial.print(c == 'f' ? F("UYGULA ") : F("SERBEST "));
    Serial.println(v);
  } else if (c == 'd') {
    min_arama = false; stroke_olcum = false;
    kopru_dur(); komut_binde = 0;
    Serial.println(F("# DUR (aktuator bulundugu yerde kalir)"));
  } else if (c == 'r') {
    fren_ters = !fren_ters;
    kopru_dur();
    Serial.print(F("# FREN_TERS = ")); Serial.println(fren_ters ? F("true") : F("false"));
    Serial.println(F("#   'UYGULA' komutu freni BIRAKIYORSA bu ayar dogru degildi."));
  } else if (c == 'm') {
    // Sifirdan baslayip her 250 ms'de PWM'i 1 artirir. Aktuator kimildayinca
    // 'k' basilir ve o an ki PWM = BRAKE_PWM_MIN.
    min_arama = true; stroke_olcum = false;
    min_pwm = 0; min_bulundu = 0; min_adim_t = millis();
    Serial.println(F("# PWM_MIN aramasi basladi. AKTUATORU IZLE."));
    Serial.println(F("#   ilk kimildadigi anda panodan 'KIMILDADI'ya bas."));
  } else if (c == 'k') {
    if (min_arama) {
      min_bulundu = min_pwm;
      min_arama = false;
      kopru_dur();
      Serial.print(F("# BRAKE_PWM_MIN = ")); Serial.println(min_bulundu);
      Serial.println(F("#   config.h'ye bu sayi yazilacak (biraz pay birak)."));
    }
  } else if (c == 't') {
    // Tam gucte uygula ve sureyi say. Uca dayanip durdugunda 'u' basilir.
    min_arama = false;
    stroke_olcum = true; stroke_bas = millis(); stroke_ms = 0;
    akim_tepe = 0;
    fren_hiz_ham(1.0f);
    Serial.println(F("# STROKE OLCUMU basladi (tam guc UYGULA)."));
    Serial.println(F("#   uca dayanip durdugunda 'DAYANDI'ya bas."));
  } else if (c == 'u') {
    if (stroke_olcum) {
      stroke_ms = (uint16_t)(millis() - stroke_bas);
      stroke_olcum = false;
      kopru_dur();
      Serial.print(F("# TAM STROKE = ")); Serial.print(stroke_ms);
      Serial.println(F(" ms"));
      Serial.print(F("#   BRAKE_STALL_MS bundan UZUN olmali. Onerilen: "));
      Serial.println((uint16_t)(stroke_ms * 1.4f));
    }
  } else if (c == 'a') {
    akim_bagli = !akim_bagli;
    akim_tepe = 0;
    Serial.print(F("# akim okuma ")); Serial.println(akim_bagli ? F("ACIK (A0/A1)") : F("kapali"));
  } else if (c == 'z') {
    akim_tepe = 0; stroke_ms = 0; min_bulundu = 0;
    Serial.println(F("# sayaclar sifirlandi"));
  }
}

// ─────────────────────────────────────────────────────────────────────────────
char tampon[24];
uint8_t tampon_n = 0;
uint32_t t_rapor = 0;

void setup() {
  Serial.begin(115200);

  // ⚠ Once seviye yaz, SONRA cikis yap. Ters sirada reset aninda kopru bir an
  // tanimsiz surulur ve aktuator seyirir.  (STM32'de de gecerli: digitalWrite
  // ODR'ye yaziyor, pin cikisa alininca o seviye disari verilir.)
  digitalWrite(RPWM_PIN, LOW); pinMode(RPWM_PIN, OUTPUT);
  digitalWrite(LPWM_PIN, LOW); pinMode(LPWM_PIN, OUTPUT);

  analogWriteResolution(8);        // PWM_MAX = 190 -> 255 tabanina gore
  analogWrite(RPWM_PIN, 0);
  analogWrite(LPWM_PIN, 0);

  analogReadResolution(12);        // IS okumasi 0..4095

  digitalWrite(EN_PIN, LOW); pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH);      // kopru acik (R_EN + L_EN kopruluk)

  Serial.println();
  Serial.println(F("# fren_f767 — fren aktuatoru + BTS7960B tezgah testi"));
  Serial.println(F("# pin: RPWM=PC6(CN12-4) LPWM=PC7(CN12-19) EN=PC8(CN12-2)"));
  Serial.println(F("# ⚠ B+ 12 V hattina. 48 V modulu oldurur."));
  Serial.println(F("# ⚠ RPWM/LPWM/EN ucune de 10k pull-down ŞART (STM32 reset'te yuzer)."));
  Serial.println(F("# SIRA: 1) yon (FREN_TERS)  2) PWM_MIN  3) stroke suresi"));
  Serial.println(F("# 🔴 F767 farki: girisler 3,3 V mantikla suruluyor — bu test onu da yokluyor."));
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (tampon_n) { tampon[tampon_n] = 0; komut_isle(tampon); tampon_n = 0; }
    } else if (tampon_n < sizeof(tampon) - 1) {
      tampon[tampon_n++] = c;
    }
  }

  // PWM_MIN aramasi: her 250 ms'de bir kademe
  if (min_arama && millis() - min_adim_t >= 250) {
    min_adim_t = millis();
    if (min_pwm < PWM_MAX) {
      min_pwm++;
      ham_pwm_sur(min_pwm, 1);
    } else {
      min_arama = false;
      kopru_dur();
      Serial.println(F("# tavana gelindi, aktuator hic kimildamadi."));
      Serial.println(F("#   besleme? yon? kablo? B+ gercekten 12 V mi?"));
      Serial.println(F("#   ...ve F767'ye ozel: girisler 3,3 V'la surulemiyor olabilir."));
    }
  }

  // Tezgah korumasi — gozetimsiz surusu kes. PWM_MIN aramasi haric tutuldu:
  // orada surus zaten 250 ms'de bir yenileniyor ve akim kademe kademe artiyor.
  if (surus_bas && !min_arama && millis() - surus_bas >= KORUMA_MS) {
    kopru_dur(); komut_binde = 0; stroke_olcum = false;
    Serial.print(F("# ⚠ TEZGAH KORUMASI: "));
    Serial.print(KORUMA_MS / 1000); Serial.println(F(" sn kesintisiz surus -> kopru kesildi."));
    Serial.println(F("#   Stroke olcumu bu sureden uzun surduyse aktuator bu testte cok yavas."));
  }

  akim_oku();

  if (millis() - t_rapor >= 100) {
    t_rapor = millis();
    Serial.print(F("D pwm="));      Serial.print(pwm_akt);
    Serial.print(F(" yon="));       Serial.print(yon_akt);
    Serial.print(F(" komut="));     Serial.print(komut_binde);
    Serial.print(F(" ters="));      Serial.print(fren_ters ? 1 : 0);
    Serial.print(F(" akim="));      Serial.print(akim_ham);
    Serial.print(F(" akimmv="));    Serial.print(akim_mv);
    Serial.print(F(" akimtepe=")); Serial.print(akim_tepe);
    Serial.print(F(" minpwm="));    Serial.print(min_arama ? min_pwm : min_bulundu);
    Serial.print(F(" minbul="));    Serial.print(min_bulundu);
    Serial.print(F(" stroke="));    Serial.print(stroke_ms);
    Serial.print(F(" sayac="));
    Serial.println(stroke_olcum ? (uint16_t)(millis() - stroke_bas) : 0);
  }
}
