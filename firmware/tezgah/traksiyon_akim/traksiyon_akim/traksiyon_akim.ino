/*
  Traksiyon akımı ölçümü — ACS758 + Arduino (Mega ya da boşta bir Uno/Nano)

  Neden ayrı bir kart: ölçüm kampanyası sırasında çalışan sürüş firmware'ine
  dokunmuyoruz. Sayılar netleştikten sonra kalıcı entegrasyon Mega'ya yapılır.

  ─── KRİTİK TASARIM NOKTASI ───────────────────────────────────────────
  Telemetriye ORTALAMA yazmak bu ölçümü anlamsız kılar. 100 ms'de bir
  gönderilen ortalama, 50 ms süren 80 A'lık bir tepeyi 45 A gibi gösterir —
  oysa aradığımız şey tam olarak o tepe. Bu yüzden her pencerede ortalama,
  TEPE ve dip birlikte veriliyor; ayrıca eşiğin üstünde geçen toplam süre
  sayılıyor, çünkü BMS'in kesmesi eşik KADAR süreye de bağlı.

  ─── BAĞLANTI ─────────────────────────────────────────────────────────
    ACS758  VCC  -> 5V
            GND  -> GND
            VOUT -> A0        (araya 1k + 10nF alçak geçiren koymak iyi olur)
    Sensörün BARASI batarya (XT90) ile motor kontrolcüsü arasına girer.
    Artı ya da eksi ucundan biri; ikisi de aynı akımı taşır.
    ANL sigorta yerinde KALIR.

  ─── SIFIRLAMA ────────────────────────────────────────────────────────
  Açılışta 2 saniye boyunca sıfır noktası ölçülür. O sırada hattan AKIM
  GEÇMEMELİ (kontak kapalı, gaz sıfır). Sıfır yanlışsa bütün ölçüm kayar.

  ─── ÇIKTI ────────────────────────────────────────────────────────────
  10 Hz, CSV:  ms,ort,tepe,dip,oturum_tepe,esik_ustu_ms
  Seri porta bağlanıp doğrudan dosyaya alınabilir:
      python3 - <<'EOF'
      import serial; p=serial.Serial('/dev/ttyACM0',115200)
      open('akim.csv','wb').writelines(iter(p.readline,b''))
      EOF
*/

/* ─── ayarlar ─── */
const uint8_t  PIN_AKIM   = A0;
const float    VREF       = 5.0;      /* karta giden gerçek 5 V — ÖLÇ ve yaz */
const float    DUYARLILIK = 0.020;    /* V/A · ACS758-100B = 20 mV/A         */
                                      /* -50B: 0.040 · -150B: 0.0133         */
const float    ESIK_A     = 60.0;     /* BMS deşarj kesme eşiği              */
const uint16_t PENCERE_MS = 100;      /* rapor penceresi                     */
const uint16_t SIFIR_MS   = 2000;     /* açılış sıfırlama süresi             */

/* ─── durum ─── */
float    sifirSayim = 512.0;          /* boştaki ADC okuması (Vcc/2)         */
float    lsbBasinaA;                  /* ADC adımı başına amper              */
uint32_t sonRapor;
/* Toplam tamsayı: float32 birkaç bin örnekten sonra her eklemede
   yuvarlıyor ve ortalama birkaç amper kayabiliyordu. */
uint32_t toplam = 0, adet = 0;
int      tepeSayim, dipSayim;
float    oturumTepe = 0;
uint32_t esikUstuMs = 0;

float sayimAmper(float s){ return (s - sifirSayim) * lsbBasinaA; }

void setup(){
  Serial.begin(115200);
  while (!Serial && millis() < 3000);

  /* ADC'yi hızlandır: varsayılan bölen 128 (~112 us/örnek) tepe yakalamak
     için yavaş. Bölen 32 -> ~26 us. Doğruluktan bir miktar veriyoruz,
     karşılığında pencere başına ~3800 örnek alıyoruz. */
  ADCSRA = (ADCSRA & 0xF8) | 0x05;

  lsbBasinaA = (VREF / 1024.0) / DUYARLILIK;

  Serial.println(F("# traksiyon akim olcumu"));
  Serial.println(F("# SIFIRLAMA: hattan akim GECMEMELI (2 sn)"));
  double s = 0; uint32_t n = 0;
  uint32_t t0 = millis();
  while (millis() - t0 < SIFIR_MS){ s += analogRead(PIN_AKIM); n++; }
  sifirSayim = s / n;

  Serial.print(F("# sifir="));   Serial.print(sifirSayim, 1);
  Serial.print(F(" sayim ("));   Serial.print(sifirSayim * VREF / 1024.0, 3);
  Serial.print(F(" V) · orneklem=")); Serial.println(n);
  Serial.print(F("# cozunurluk=")); Serial.print(lsbBasinaA, 3);
  Serial.print(F(" A/adim · esik=")); Serial.print(ESIK_A, 0);
  Serial.println(F(" A"));
  Serial.println(F("ms,ort,tepe,dip,oturum_tepe,esik_ustu_ms"));

  sonRapor = millis();
  tepeSayim = dipSayim = analogRead(PIN_AKIM);
}

void loop(){
  int s = analogRead(PIN_AKIM);
  toplam += s; adet++;
  if (s > tepeSayim) tepeSayim = s;
  if (s < dipSayim)  dipSayim  = s;

  uint32_t simdi = millis();
  if (simdi - sonRapor < PENCERE_MS) return;

  const float ort  = sayimAmper((float)toplam / adet);
  const float tepe = sayimAmper(tepeSayim);
  const float dip  = sayimAmper(dipSayim);

  /* Mutlak değerce en büyük olan "tepe" sayılır: geri viteste akım negatif
     akar ve orada da BMS'i zorlayabiliriz. */
  const float buyuk = max(fabs(tepe), fabs(dip));
  if (buyuk > oturumTepe) oturumTepe = buyuk;
  if (buyuk > ESIK_A) esikUstuMs += (simdi - sonRapor);

  Serial.print(simdi);        Serial.print(',');
  Serial.print(ort, 2);       Serial.print(',');
  Serial.print(tepe, 2);      Serial.print(',');
  Serial.print(dip, 2);       Serial.print(',');
  Serial.print(oturumTepe, 2);Serial.print(',');
  Serial.println(esikUstuMs);

  toplam = 0; adet = 0;
  tepeSayim = dipSayim = s;
  sonRapor = simdi;
}
