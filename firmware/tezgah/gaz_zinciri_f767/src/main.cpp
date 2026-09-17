// UCTAN UCA GAZ ZINCIRI — FlySky iBUS -> NUCLEO-F767ZI -> MCP4725 -> gaz voltaji
//
// ⚠ VOUT KONTROLCUYE BAGLI DEGIL. Multimetre VOUT <-> GND.
//
// Zincirin iki ucu ayri ayri dogrulandi (testler/ibus_f767, testler/mcp4725_f767);
// bu ilk kez ortasini birlestiriyor. Cevaplanan sorular:
//   1. Sag stick dikey (CH2) oynatinca gaz voltaji gercekten degisiyor mu?
//   2. SwA (CH7) kesmesi gazi rolantiye cekiyor mu?
//   3. SwB (CH8) taret moduna gecince gaz rolantiye dusuyor mu? (tasarim kurali)
//   4. Cerceve akisi kesilirse ne oluyor?
//
// OLCULEN KANAL DUZENI (30 Agu 2026, testler/ibus_f767/SONUC.md):
//   CH1 sag yatay = direksiyon · CH2 sag dikey = GAZ (yukari ileri)
//   CH3 sol dikey = fren · CH7 SwA = kesme · CH8 SwB = taret · CH9 SwC = mod

#include <Arduino.h>
#include <Wire.h>

// ── ANA GUC KESME (SSR-100 DD) ──────────────────────────────────────────────
// PE15 (D37, CN12-53) -> transistor -> SSR girisi.  AKTIF-YUKSEK secildi:
// MCU reset atarsa/olurse pin yuksek empedansa doner, SSR birakir, guc kesilir.
// Yani ariza guvenli tarafa dusuyor. Bedeli: her yukleme/reset gucu kesiyor.
//
// Kumanda: SwD / CH10.  Yuksek (2000) = guc VAR · dusuk (1000) = KES.
// SwA ile ayni yon: kesme anahtarinda dusuk = kes.
// Acilista kanal[] 1500'e kurulu -> ilk cerceve gelene kadar SSR KAPALI.
//
// SwA (yazilimsal, gazi rolantiye ceker) ile SwD (donanimsal, gucu keser)
// BILEREK AYRI tutuldu: biri yumusak duruma, oteki tam kesmeye.
static const uint32_t SSR_PIN = PE15;
static bool     ssr_acik  = false;
static bool     ssr_elle  = false;   // tezgah: kumandadan bagimsiz elle surme
static uint32_t ssr_sayac = 0;

// ── geri vites ──────────────────────────────────────────────────────────────
// Role modulu AKTIF-DUSUK: IN = LOW -> role ceker. PE14 (D38, CN12-51).
// ⚠ Yon degistirme HAREKET HALINDE YAPILMAZ. Once gaz rolantiye iner,
// GECIS_MS kadar beklenir, sonra role konum degistirir. Aksi halde
// kontrolcu ileri giderken geri vites kontagi kapanir.
static const uint32_t GERI_PIN  = PE14;
static const uint32_t GECIS_MS  = 250;     // role anahtarlamadan once rolanti suresi

static bool     geri_istek = false;        // stick asagida mi
static bool     geri_aktif = false;        // role su an cekili mi
static uint32_t gecis_bas  = 0;
static bool     gecisde    = false;
static uint32_t geri_sayac = 0;
// Modul aktif-dusuk mu aktif-yuksek mi: tezgahta 'v' ile cevrilir.
// Varsayilan aktif-dusuk (IN=LOW -> role ceker).
static bool     geri_ters  = false;
static bool     geri_elle_var = false;   // tezgah: kumandadan bagimsiz elle surme
static bool     geri_elle_deg = false;

// ── gaz haritasi ────────────────────────────────────────────────────────────
static const float V_ROLANTI = 0.80f;   // kontrolcunun "gaz yok"u
static const float V_TAM     = 4.20f;   // kontrolcunun tam gazi
static float       VDD       = 4.97f;   // 30 Agu 2. olcum: rolanti ham 741 -> 0,90 V okundu => VDD 4,97

// Guc siniri: stick tepesi bu orana kadar gaz verir. Guvenli varsayilan %45,
// arac uzerinde daha once gercekten kullanilan deger. 'p' ile degisir.
static float guc_siniri = 0.45f;

static const uint16_t MERKEZ    = 1500;
static const uint16_t OLU_BANT  = 25;     // us
static const uint16_t YAY_UCU   = 500;    // merkezden uca us

// ── iBUS ────────────────────────────────────────────────────────────────────
HardwareSerial ibus(PD2, PC12);
static uint8_t  ara[32]; static uint8_t n = 0;
static uint16_t kanal[14];
static uint32_t saglam = 0, bozuk = 0, saniye = 0, hz = 0, son_cerceve = 0;
static uint32_t bayt = 0, bas_20 = 0;   // teshis: UART'a hic bayt geliyor mu
// 🔴 30 Agu: iBUS iki kez, acilisin hemen ardindan, TAMAMEN sustu (bayt=0) ve
// ancak yeniden yukleme cozdu. Belirti STM32 UART'inin taşma/cerceve hatasini
// mandallayip almayi durdurmasina uyuyor — bayrak temizlenmedikce bir daha
// bayt gelmiyor. Aracta bu, kumandanin sessizce olmesi demek.
// Gozcu: 1 sn hic bayt gelmezse UART'i yeniden kur.
static uint32_t son_bayt_ms = 0, uart_yeniden = 0;

// ── MCP4725 ─────────────────────────────────────────────────────────────────
static uint8_t  adres = 0x60;
static bool     dac_var = false;
static uint32_t i2c_hata = 0;
static uint16_t son_ham = 0;

static uint16_t volt_to_ham(float v) {
  float h = (v / VDD) * 4095.0f;
  if (h < 0) h = 0; if (h > 4095) h = 4095;
  return (uint16_t)h;
}

static bool dac_yaz(uint16_t d) {
  if (d > 4095) d = 4095;
  Wire.beginTransmission(adres);
  Wire.write(0x40);
  Wire.write((uint8_t)(d >> 4));
  Wire.write((uint8_t)((d & 0x0F) << 4));
  if (Wire.endTransmission() != 0) { i2c_hata++; return false; }
  son_ham = d;
  return true;
}

static void dac_ara() {
  dac_var = false;
  for (uint8_t a : {0x60, 0x61}) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { adres = a; dac_var = true; break; }
  }
  Serial.print(F("MCP4725: "));
  if (dac_var) { Serial.print(F("0x")); Serial.println(adres, HEX); }
  else Serial.println(F("YOK"));
}

static void cerceve() {
  uint16_t t = 0; for (uint8_t i = 0; i < 30; i++) t += ara[i];
  if ((uint16_t)(0xFFFF - t) != ((uint16_t)ara[30] | ((uint16_t)ara[31] << 8))) { bozuk++; return; }
  saglam++; saniye++; son_cerceve = millis();
  for (uint8_t i = 0; i < 14; i++)
    kanal[i] = (uint16_t)ara[2 + 2 * i] | ((uint16_t)ara[3 + 2 * i] << 8);
}

void setup() {
  Serial.begin(115200);
  ibus.begin(115200);
  Wire.setSCL(PB8); Wire.setSDA(PB9); Wire.begin(); Wire.setClock(100000);
  pinMode(LED_GREEN, OUTPUT); pinMode(LED_RED, OUTPUT);
  // Once HIGH'a kur, sonra cikis yap: acilista role kisa sureligine cekmesin.
  // 🔴 30 Agu: modul JQC3F-5VDC-C'li klasik mavi tek kanalli modul — AKTIF-DUSUK
  // ve girisi icerde 5 V'a pull-up'li. Bizim 3,3 V'luk HIGH'imiz birakma esiginin
  // altinda kaliyordu, role hic birakmiyordu. Cozum ACIK DRENAJ:
  //   LOW  -> pin topraga ceker            -> role CEKER
  //   HIGH -> pin Hi-Z, modul 5 V'a tasir  -> role BIRAKIR
  // PE14 FT (5 V toleransli), hattin 5 V'a cikmasi guvenli. Ek donanim yok.
  digitalWrite(GERI_PIN, HIGH);
  pinMode(GERI_PIN, OUTPUT_OPEN_DRAIN);
  digitalWrite(GERI_PIN, HIGH);
  digitalWrite(SSR_PIN, LOW);
  pinMode(SSR_PIN, OUTPUT);
  digitalWrite(SSR_PIN, LOW);               // aktif-yuksek -> LOW = SSR kapali
  for (uint8_t i = 0; i < 14; i++) kanal[i] = MERKEZ;

  delay(200);
  Serial.println();
  Serial.println(F("UCTAN UCA GAZ ZINCIRI  —  iBUS(PD2) -> MCP4725(PB8/PB9)"));
  Serial.println(F("⚠ VOUT kontrolcuye BAGLI OLMAMALI"));
  dac_ara();
  dac_yaz(volt_to_ham(V_ROLANTI));          // her sey rolantiden baslar
  Serial.println(F("komut: p=guc siniri (%30/%45/%100)  o=oku"));
}

void loop() {
  while (ibus.available()) {
    const uint8_t b = (uint8_t)ibus.read();
    bayt++; if (b == 0x20) bas_20++; son_bayt_ms = millis();
    if (n == 0) { if (b != 0x20) continue; }
    else if (n == 1) { if (b != 0x40) { n = 0; if (b == 0x20) { ara[0] = b; n = 1; } continue; } }
    ara[n++] = b;
    if (n == 32) { cerceve(); n = 0; }
  }

  const uint32_t simdi = millis();
  static uint32_t t_hz = 0, t_dac = 0, t_yaz = 0;

  // ── UART gozcusu ──────────────────────────────────────────────────────────
  if (son_bayt_ms == 0) son_bayt_ms = simdi;
  if (simdi - son_bayt_ms > 1000) {
    ibus.end();
    ibus.begin(115200);
    n = 0;
    son_bayt_ms = simdi;
    uart_yeniden++;
    Serial.print(F("UART yeniden kuruldu (#")); Serial.print(uart_yeniden); Serial.println(F(")"));
  }
  if (simdi - t_hz >= 1000) { t_hz = simdi; hz = saniye; saniye = 0; }

  // ── gaz hesabi ────────────────────────────────────────────────────────────
  const uint16_t ch2 = kanal[1], ch7 = kanal[6], ch8 = kanal[7];
  const uint16_t ch10 = kanal[9];
  const bool cerceve_yok = (simdi - son_cerceve) > 500;
  const bool kesme       = (ch7 < 1500);      // SwA: dusuk = KES

  // ── ana guc kesme — SwA/CH7 ile ───────────────────────────────────────────
  // 30 Agu: SSR SwD'den SwA'ya alindi. Artik TEK kesme anahtari var ve iki
  // katmani birden yapiyor: gazi rolantiye ceker VE ana gucu keser.
  // Aktif-yuksek surme -> MCU olurse pin Hi-Z olur, SSR birakir, guc kesilir.
  const bool ssr_istek = ssr_elle || (!kesme && !cerceve_yok);
  if (ssr_istek != ssr_acik) {
    ssr_acik = ssr_istek;
    digitalWrite(SSR_PIN, ssr_acik ? HIGH : LOW);
    if (ssr_acik) ssr_sayac++;
    Serial.print(F("ANA GUC -> ")); Serial.println(ssr_acik ? F("ACIK") : F("KESILDI"));
  }
  const bool taret       = (ch8 > 1500);      // SwB: yuksek = taret aktif
  const bool rolantiye   = kesme || taret || cerceve_yok;

  // ── stick -> yon + buyukluk ───────────────────────────────────────────────
  float oran = 0.0f;
  bool  istek_geri = false;
  if (ch2 > MERKEZ + OLU_BANT) {
    oran = (float)(ch2 - MERKEZ - OLU_BANT) / (float)(YAY_UCU - OLU_BANT);
  } else if (ch2 + OLU_BANT < MERKEZ) {
    oran = (float)(MERKEZ - OLU_BANT - ch2) / (float)(YAY_UCU - OLU_BANT);
    istek_geri = true;
  }
  if (oran > 1.0f) oran = 1.0f;
  if (rolantiye) istek_geri = false;       // kesme/taret: geri de iptal
  if (geri_elle_var) istek_geri = geri_elle_deg;   // elle surme her seyi ezer
  geri_istek = istek_geri;

  // ── yon degistirme: once gazi kes, sonra role ─────────────────────────────
  bool gaz_kilit = false;
  if (istek_geri != geri_aktif) {
    if (!gecisde) { gecisde = true; gecis_bas = simdi; }
    gaz_kilit = true;                      // gecis boyunca gaz rolantide
    if (simdi - gecis_bas >= GECIS_MS) {
      geri_aktif = istek_geri;
      digitalWrite(GERI_PIN, (geri_aktif != geri_ters) ? LOW : HIGH);
      if (geri_aktif) geri_sayac++;
      gecisde = false;
      Serial.print(F("GERI VITES -> ")); Serial.println(geri_aktif ? F("ACIK") : F("KAPALI"));
    }
  } else gecisde = false;
  const float tavan = V_ROLANTI + guc_siniri * (V_TAM - V_ROLANTI);
  const float hedef = (rolantiye || gaz_kilit) ? V_ROLANTI
                                               : V_ROLANTI + oran * (tavan - V_ROLANTI);

  if (simdi - t_dac >= 20) { t_dac = simdi; if (dac_var) dac_yaz(volt_to_ham(hedef)); }

  digitalWrite(LED_GREEN, dac_var && hz > 50 && !rolantiye);
  digitalWrite(LED_RED,   rolantiye);

  if (simdi - t_yaz >= 100) {
    t_yaz = simdi;
    Serial.print(F("D ch2="));    Serial.print(ch2);
    Serial.print(F(" ch7="));     Serial.print(ch7);
    Serial.print(F(" ch8="));     Serial.print(ch8);
    Serial.print(F(" oran="));    Serial.print((int)(oran * 100));
    Serial.print(F(" gaz_mv="));  Serial.print((int)(hedef * 1000));
    Serial.print(F(" ham="));     Serial.print(son_ham);
    Serial.print(F(" kes="));     Serial.print(kesme ? 1 : 0);
    Serial.print(F(" taret="));   Serial.print(taret ? 1 : 0);
    Serial.print(F(" sinir="));   Serial.print((int)(guc_siniri * 100));
    Serial.print(F(" hz="));      Serial.print(hz);
    Serial.print(F(" i2c_hata=")); Serial.print(i2c_hata);
    Serial.print(F(" bayt=")); Serial.print(bayt);
    Serial.print(F(" bas20=")); Serial.print(bas_20);
    Serial.print(F(" saglam=")); Serial.print(saglam);
    Serial.print(F(" bozuk=")); Serial.print(bozuk);
    Serial.print(F(" geri=")); Serial.print(geri_aktif ? 1 : 0);
    Serial.print(F(" gecis=")); Serial.print(gecisde ? 1 : 0);
    Serial.print(F(" geri_sayac=")); Serial.print(geri_sayac);
    Serial.print(F(" ch10=")); Serial.print(ch10);
    Serial.print(F(" ssr=")); Serial.print(ssr_acik ? 1 : 0);
    Serial.print(F(" uart_yeniden=")); Serial.print(uart_yeniden);
    Serial.print(F(" geri_ters=")); Serial.print(geri_ters ? 1 : 0);
    Serial.print(F(" pe14=")); Serial.println(digitalRead(GERI_PIN));
  }

  if (Serial.available()) {
    const char k = (char)Serial.read();
    if (k == 'p') {
      guc_siniri = (guc_siniri < 0.31f) ? 0.45f : (guc_siniri < 0.46f ? 1.00f : 0.30f);
      Serial.print(F("guc siniri = %")); Serial.print((int)(guc_siniri * 100));
      Serial.print(F("  -> stick tepesi ")); 
      Serial.print((int)((V_ROLANTI + guc_siniri * (V_TAM - V_ROLANTI)) * 1000));
      Serial.println(F(" mV"));
    } else if (k == 'o') dac_ara();
    else if (k == 'r') {
      geri_elle_var = true; geri_elle_deg = !geri_elle_deg;
      Serial.print(F("ELLE GERI ROLE = ")); Serial.println(geri_elle_deg ? F("CEK") : F("BIRAK"));
    }
    else if (k == 'n') { geri_elle_var = false; Serial.println(F("elle surme kapandi, kumandaya dondu")); }
    else if (k == 'v') {
      geri_ters = !geri_ters;
      digitalWrite(GERI_PIN, (geri_aktif != geri_ters) ? LOW : HIGH);
      Serial.print(F("GERI ROLE polaritesi = "));
      Serial.println(geri_ters ? F("AKTIF-YUKSEK") : F("AKTIF-DUSUK"));
    }
    else if (k == 'g') {
      ssr_elle = !ssr_elle;
      Serial.print(F("ELLE SSR = ")); Serial.println(ssr_elle ? F("ACIK") : F("KAPALI"));
    }
  }
}
