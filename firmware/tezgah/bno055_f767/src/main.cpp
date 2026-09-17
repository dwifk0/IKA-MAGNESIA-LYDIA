/*
 * BNO055 TEZGAH TESTİ — Nucleo-F767ZI, UART modu
 *
 * Amaç: IMU'yu araca hiç yaklaştırmadan masada doğrulamak. Üç soruya cevap
 * verir ve üçü de ayrı ayrı önemlidir:
 *
 *   1. Çip KONUŞUYOR mu?        → CHIP_ID 0xA0 okunuyor mu
 *   2. Açılar ANLAMLI mı?       → kartı çevir, yaw takip ediyor mu
 *   3. KALİBRE mi?              → sys baytı 3 olmadan yaw'a güvenilmez
 *
 * ⚠ ÜÇÜNCÜSÜ EN ÇOK ATLANANDIR. BNO055 açılışta sayı basar ama o sayı,
 * kalibrasyon 3'e çıkana kadar sürüklenen bir tahmindir. "Veri geliyor,
 * demek ki çalışıyor" diyip geçmek EKF'i kirli veriyle beslemek olur.
 *
 * ── NEDEN UART, NEDEN I²C DEĞİL ─────────────────────────────────────────────
 * BNO055'in bilinen clock-stretching hatası I²C hattını kilitliyor. İKA'da
 * I²C kilidi aracın TAMAMINI donduruyordu (gaz DAC'ı da o hatta). UART'ta bu
 * arıza sınıfı yok. Modülde PS1 pinini VCC'ye çekmek gerekiyor.
 *
 * ── DERLEME / YÜKLEME ───────────────────────────────────────────────────────
 *   ~/ika/testler/yukle.sh bno055_f767      (derler + yukler)
 * 🔴 WSL'de NE `pio run -t upload` NE de `cp ... /mnt/d/` calisir: USB gecisi
 *    yok ve WSL kaldirilabilir surucuyu baglamiyor. yukle.sh kopyalamayi
 *    Windows'a yaptirir. (30 Agu 2026'da ikisi de denendi.)
 * ⚠ Yüklemeden önce panoyu KAPAT — portu tutuyor.
 */

#include <Arduino.h>

// ── PİNLER ──────────────────────────────────────────────────────────────────
// Ana firmware'le (F767_FIRMWARE/src/config.h) AYNI. Tezgahta başka pin
// kullanmak, testin elemesi gereken hata sınıfını geri davet eder.
#define BNO_TX_PIN     PD5     // USART2_TX → modülün RX'i
#define BNO_RX_PIN     PD6     // USART2_RX ← modülün TX'i
#define BNO_RESET_PIN  PD7     // modülün RST'si (LOW = reset)

HardwareSerial Bno(BNO_RX_PIN, BNO_TX_PIN);

// ── BNO055 kayıtları ────────────────────────────────────────────────────────
#define R_CHIP_ID   0x00
#define R_ACC_ID    0x01
#define R_MAG_ID    0x02
#define R_GYR_ID    0x03
#define R_SYS_STAT  0x39
#define R_SYS_ERR   0x3A
#define R_UNIT_SEL  0x3B
#define R_OPR_MODE  0x3D
#define R_EUL       0x1A       // yaw, roll, pitch — 6 bayt, 1/16 derece
#define R_CALIB     0x35
#define R_TEMP      0x34

#define MOD_CONFIG  0x00
#define MOD_IMU     0x08       // manyetometre YOK — yaw serbest sürüklenir
#define MOD_NDOF    0x0C       // tam füzyon, mutlak yön
#define CHIP_ID     0xA0

// ── UART çerçevesi ──────────────────────────────────────────────────────────
//   yaz → AA 00 <reg> <len> <veri...>   yanıt: EE 01 (başarı)
//   oku → AA 01 <reg> <len>             yanıt: BB <len> <veri...>  / EE <kod>
static void temizle(void) { while (Bno.available()) Bno.read(); }

static bool bekle(uint8_t n, uint32_t ms) {
  uint32_t bitis = millis() + ms;
  while ((uint32_t)Bno.available() < n)
    if ((int32_t)(millis() - bitis) >= 0) return false;
  return true;
}

static bool bno_yaz(uint8_t reg, uint8_t deger) {
  temizle();
  uint8_t c[5] = {0xAA, 0x00, reg, 0x01, deger};
  Bno.write(c, 5);
  if (!bekle(2, 30)) return false;
  return (Bno.read() == 0xEE) && (Bno.read() == 0x01);
}

// Hata durumunda `son_hata`ya BNO'nun kendi kodunu bırakır — "çalışmıyor"
// demekle "0x07 yazma reddedildi" demek arasında saatler fark var.
static uint8_t son_hata = 0;

// Hata sinifina gore ayri sayaclar. "414 okuma dustu" tek basina bir sey
// soylemiyor; HANGI hata oldugu dogrudan sebebi gosteriyor:
uint32_t h_cevapsiz = 0;       // 0xFF — hic cevap yok  -> kablo/PS1/besleme
uint32_t h_overrun  = 0;       // 0x07 — BUS_OVER_RUN   -> cok hizli sorguluyoruz
uint32_t h_bicim    = 0;       // 0xFE/0xFD — bozuk/eksik cerceve -> sinyal butunlugu
uint32_t h_diger    = 0;       // cipin baska hata kodu
uint8_t  son_kod    = 0;

static void hata_say(uint8_t kod) {
  son_kod = kod;
  if      (kod == 0xFF) h_cevapsiz++;
  else if (kod == 0x07) h_overrun++;          // BNO055 UART'in bilinen kusuru
  else if (kod == 0xFE || kod == 0xFD) h_bicim++;
  else h_diger++;
}

static bool bno_oku(uint8_t reg, uint8_t len, uint8_t *hedef) {
  temizle();
  uint8_t c[4] = {0xAA, 0x01, reg, len};
  Bno.write(c, 4);
  if (!bekle(2, 30)) { son_hata = 0xFF; hata_say(0xFF); return false; }  // hiç cevap
  uint8_t bas = Bno.read();
  uint8_t n   = Bno.read();
  if (bas == 0xEE) { son_hata = n; hata_say(n); return false; }
  if (bas != 0xBB || n != len) { son_hata = 0xFE; hata_say(0xFE); return false; }
  if (!bekle(len, 30)) { son_hata = 0xFD; hata_say(0xFD); return false; }
  for (uint8_t i = 0; i < len; i++) hedef[i] = Bno.read();
  son_hata = 0;
  return true;
}

// ── Durum ───────────────────────────────────────────────────────────────────
bool    var = false;
uint8_t mod = MOD_NDOF;
int16_t yaw10 = 0, roll10 = 0, pitch10 = 0;
int16_t yaw_ofset10 = 0;
uint8_t kalib = 0, sys_stat = 0, sys_err = 0;
int8_t  sicaklik = 0;
uint32_t okuma = 0, dusen = 0;
uint8_t  opr_geri = 0xFF;      // cipten GERI OKUNAN OPR_MODE — yazdigimiz degil


static bool kur(void) {
  Bno.begin(115200);

  pinMode(BNO_RESET_PIN, OUTPUT);
  digitalWrite(BNO_RESET_PIN, LOW);
  delay(20);
  digitalWrite(BNO_RESET_PIN, HIGH);
  delay(700);                       // POR sonrası çip ~650 ms sessiz

  uint8_t id = 0;
  for (uint8_t i = 0; i < 5 && id != CHIP_ID; i++) {
    bno_oku(R_CHIP_ID, 1, &id);
    if (id != CHIP_ID) delay(60);
  }
  if (id != CHIP_ID) {
    Serial.print(F("# CHIP_ID okunamadi (gelen 0x"));
    Serial.print(id, HEX);
    Serial.println(F(", olmasi gereken 0xA0)"));
    Serial.println(F("# SIRAYLA BAK:"));
    Serial.println(F("#   1) PS1 pini VCC'de mi? Degilse cip I2C modunda"));
    Serial.println(F("#      kalir ve UART'tan TEK BAYT BILE gelmez."));
    Serial.println(F("#   2) TX/RX capraz mi? Modulun TX'i PD6'ya gitmeli."));
    Serial.println(F("#   3) Modul 3,3 V mi 5 V mi besleniyor?"));
    Serial.println(F("#   4) GND ortak mi?"));
    return false;
  }

  if (!bno_yaz(R_OPR_MODE, MOD_CONFIG)) return false;
  delay(25);
  bno_yaz(R_UNIT_SEL, 0x00);          // derece, derece/s, m/s^2
  delay(10);
  if (!bno_yaz(R_OPR_MODE, mod)) return false;
  delay(25);
  return true;
}

// ── Sorgu hizi — 30 Ağustos'ta ÖLÇÜMLE değişti ─────────────────────────────
// Önce her turda (50 Hz) BEŞ ayrı işlem yapılıyordu: açı + kalib + sys_stat +
// sys_err + sıcaklık = saniyede 250 sorgu. Tezgahta `dusen` %8'e çıktı.
// BNO055'in UART'ı arka arkaya sorguya `BUS_OVER_RUN` (0x07) ile karşılık
// veriyor; sıcaklığı saniyede 50 kez okumanın da bir anlamı yok.
// Şimdi: açı 50 Hz, durum baytları 2 Hz → saniyede ~58 sorgu.
static void oku_aci(void) {
  if (!var) return;
  uint8_t v[6];
  if (bno_oku(R_EUL, 6, v)) {
    int16_t y = (int16_t)((uint16_t)v[0] | ((uint16_t)v[1] << 8));
    int16_t r = (int16_t)((uint16_t)v[2] | ((uint16_t)v[3] << 8));
    int16_t p = (int16_t)((uint16_t)v[4] | ((uint16_t)v[5] << 8));
    // 1/16 derece → onda bir derece. Ondalık YOLLAMIYORUZ: newlib-nano'da
    // float printf kapalı, pano ölçekliyor.
    yaw10   = (int16_t)((int32_t)y * 10 / 16);
    roll10  = (int16_t)((int32_t)r * 10 / 16);
    pitch10 = (int16_t)((int32_t)p * 10 / 16);
    okuma++;
  } else {
    dusen++;
  }
}

static void oku_durum(void) {
  if (!var) return;
  uint8_t k;
  if (bno_oku(R_CALIB, 1, &k))    kalib = k;
  if (bno_oku(R_SYS_STAT, 1, &k)) sys_stat = k;
  if (bno_oku(R_SYS_ERR, 1, &k))  sys_err = k;
  if (bno_oku(R_TEMP, 1, &k))     sicaklik = (int8_t)k;
  // ⚠ Cipten GERI OKU. Yazdigimiz mod ile cipin gercekten bulundugu mod ayri
  // seylerdir; `kmag` hic dolmuyorsa ilk bakilacak yer burasidir (IMU modunda
  // manyetometre kapalidir ve kmag hep 0 kalir).
  if (bno_oku(R_OPR_MODE, 1, &k)) opr_geri = k & 0x0F;
}

static int16_t yaw_bagil(void) {
  int32_t d = (int32_t)yaw10 - yaw_ofset10;
  while (d < 0)     d += 3600;
  while (d >= 3600) d -= 3600;
  return (int16_t)d;
}

// ── Komutlar ────────────────────────────────────────────────────────────────
//   z  yaw sifirla (su anki yon ileri kabul)   r  cipi resetle + yeniden kur
//   n  NDOF modu   i  IMU modu (manyetometresiz)   k  kimlik dokumu
static void komut(char c) {
  if (c == 'z') {
    yaw_ofset10 = yaw10;
    Serial.println(F("# yaw sifirlandi (su anki yon = 0)"));
  } else if (c == 'r') {
    Serial.println(F("# cip resetleniyor..."));
    var = kur();
    Serial.println(var ? F("# yeniden kuruldu") : F("# KURULAMADI"));
  } else if (c == 'n' || c == 'i') {
    mod = (c == 'n') ? MOD_NDOF : MOD_IMU;
    bno_yaz(R_OPR_MODE, MOD_CONFIG); delay(25);
    bno_yaz(R_OPR_MODE, mod);        delay(25);
    Serial.println(c == 'n'
      ? F("# NDOF — manyetometre dahil, yaw MUTLAK (kuzeye gore)")
      : F("# IMU — manyetometresiz, yaw SURUKLENIR. Manyetik gurultuyu"));
    if (c == 'i')
      Serial.println(F("#   elemek icin; mutlak yon isteniyorsa NDOF'a don."));
  } else if (c == 'k') {
    uint8_t a = 0, m = 0, g = 0, ci = 0;
    bno_oku(R_CHIP_ID, 1, &ci); bno_oku(R_ACC_ID, 1, &a);
    bno_oku(R_MAG_ID, 1, &m);   bno_oku(R_GYR_ID, 1, &g);
    Serial.print(F("# CHIP=0x")); Serial.print(ci, HEX);
    Serial.print(F(" ACC=0x"));   Serial.print(a, HEX);
    Serial.print(F(" MAG=0x"));   Serial.print(m, HEX);
    Serial.print(F(" GYR=0x"));   Serial.print(g, HEX);
    Serial.println(F("   (beklenen: A0 FB 32 0F)"));
  }
}

uint32_t t_oku = 0, t_durum = 0, t_rapor = 0;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println(F("# BNO055 tezgah testi — F767ZI, UART modu"));
  var = kur();
  if (var) {
    Serial.println(F("# cip bulundu, NDOF modunda."));
    Serial.println(F("# KALIBRASYON: sys 3 olmadan yaw'a GUVENME."));
    Serial.println(F("#   gyro -> karti 3 sn kimildatmadan tut"));
    Serial.println(F("#   acc  -> alti farkli yuzeye yatir, her birinde bekle"));
    Serial.println(F("#   mag  -> havada sekiz ciz"));
  }
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c >= 'a' && c <= 'z') komut(c);
  }

  uint32_t simdi = millis();
  if (simdi - t_oku >= 20) {          // 50 Hz — yalniz aci
    t_oku = simdi;
    oku_aci();
  }

  if (simdi - t_durum >= 500) {       // 2 Hz — kalibrasyon/hata/sicaklik/mod
    t_durum = simdi;
    oku_durum();
  }

  if (simdi - t_rapor >= 100) {       // 10 Hz — panonun okudugu satir
    t_rapor = simdi;
    Serial.print(F("D yaw="));    Serial.print(yaw_bagil());
    Serial.print(F(" ham="));     Serial.print(yaw10);
    Serial.print(F(" roll="));    Serial.print(roll10);
    Serial.print(F(" pitch="));   Serial.print(pitch10);
    Serial.print(F(" ksys="));    Serial.print((kalib >> 6) & 3);
    Serial.print(F(" kgyr="));    Serial.print((kalib >> 4) & 3);
    Serial.print(F(" kacc="));    Serial.print((kalib >> 2) & 3);
    Serial.print(F(" kmag="));    Serial.print(kalib & 3);
    Serial.print(F(" var="));     Serial.print(var ? 1 : 0);
    Serial.print(F(" sicak="));   Serial.print(sicaklik);
    Serial.print(F(" syserr=")); Serial.print(sys_err);
    Serial.print(F(" opr="));     Serial.print(opr_geri);
    Serial.print(F(" dusen="));   Serial.print(dusen);
    Serial.print(F(" hcevapsiz=")); Serial.print(h_cevapsiz);
    Serial.print(F(" hoverrun="));  Serial.print(h_overrun);
    Serial.print(F(" hbicim="));    Serial.print(h_bicim);
    Serial.print(F(" hdiger="));    Serial.print(h_diger);
    Serial.print(F(" sonkod="));    Serial.println(son_kod);
  }
}
