# İKA / LYDİA — Elektronik Bağlantı Haritası

> **Kesin hâl — 24 Ağustos 2026.** Çizim bu belgeye göre yapılır.
> Kaynak liste: `(90K) İKA Malzeme Final Yolcusu 21.08.2026.xlsx` (55 kalem, 89.999,33 TL).
> Önceki taslak `CIZIM_BAGLANTI_KARARLARI.md` bu belgeyle değiştirildi.

---

## 1. Mimari kararlar

| Konu | Karar |
|---|---|
| **Sürüş beyni** | **Arduino Mega 2560.** Gaz, fren, direksiyon, geri vites, E-STOP burada. |
| **Sensör kartı** | **NUCLEO-F767ZI.** Yalnız BNO055 + E6B2 + gösterge ucu okur, Jetson'a verir. Mega'nın yerine geçmez. |
| **Direksiyon** | **Step motor 23IP65-20 + DM860H + FLE57-05SW 1:10 redüktör.** Röleli bang-bang terk edildi. |
| **Fren** | **BTS7960B H-köprü**, 4S LiPo'dan beslenir. İkinci köprü **yedek**. |
| **Odometri** | **E6B2 ana motor miline**, kaplinle 1:1. Direksiyonda enkoder yok. |
| **Ana güç kesme** | 🔴 **SSR İPTAL (1 Eyl 2026)** — kaçak akım kesmiyordu, donanım söküldü, `SSR_VAR false`. Tek kesme: mantar butonun mekanik kontağı. |
| **Taret** | Ayrı Arduino UNO. |

---

## 2. Güç ağacı

```
16S LiFePO4 51,2 V 30 Ah ──┬── [ANL 60 A] ── kontrolcü B+ ──── BLDC 1200 W
   (JK Smart 60A BMS)      │
                           ├── [sigorta] ── DM860H  AC/AC  ──── direksiyon step motoru
                           │
                           └── ❌ SSR-100DD İPTAL (1 Eyl 2026) — söküldü, hat doğrudan geçiyor

4S LiPo 14,8 V ──┬── UBEC Pro 5V/3A ──── 5 V mantık hattı
  (DALY BMS)     │                        Mega · UNO · röle · IRF520 · E6B2 · BNO055
                 │
                 └── BTS7960B  B+/B−  ──── fren aktüatörü (12 V)
```

- **UBEC Pro girişi 2S–12S (7,4–50 V)** → 4S LiPo'dan beslenir. 5 V için ayrı DC-DC gerekmez.
- **48→12 V DC-DC GEREKMİYOR.** Fren aktüatörü 12 V; BTS7960B'nin `B+` tavanı ~27 V
  olduğu için köprü doğrudan 4S LiPo'dan beslenir. Tam şarjdaki 16,8 V,
  `BRAKE_PWM_MAX` ~%75'e kırpılarak yazılımdan dengelenir.
- ⚠ **Direksiyon traksiyon paketine bağlı.** DM860H 24–110 V ister, tek kaynak 48 V.
  Ana paket kesilirse direksiyon da ölür; elektronik 4S LiPo'da hayatta kalır.
  E-stop açısından istenen davranış, ama bilinerek çizilmeli.
- ⚠ **ANL 60 A sigorta ve yuvası listede yok.** 48 V ana hattın tek koruması.

---

## 3. Mega 2560 pin haritası

> 🔴 **30 Ağustos 2026: Mega tasarımdan çıktı, her şey tek F767ZI'de.**
> Bu bölüm ve altındaki zamanlayıcı/kesme notları **tarihsel kayıttır**,
> kablo çekerken kullanılmaz. Geçerli pinler: **§10 F767ZI morpho tablosu**,
> fren için **§5**.

| Pin | İşlev | Bağlandığı yer |
|---|---|---|
| **D2** | **E-STOP kanal A (INT0)** | NC kontak → GND, `INPUT_PULLUP` |
| **D3** | **E-STOP kanal B (INT1)** | 2. NC kontak → GND, `INPUT_PULLUP` |
| D4 | Direksiyon `ENA+` | DM860H — *ilk denemede bağlanmaz* |
| D5 | — boş | taret UNO'da |
| D6 | — boş | taret UNO'da |
| D7 | Geri vites rölesi | Songle, aktif düşük |
| **D8** | Direksiyon `PUL+` | DM860H — **OC4C, donanım darbe treni** |
| **D9** | Direksiyon `DIR+` | DM860H |
| D10 | Fren `R_EN`+`L_EN` | BTS7960B |
| D11 | Fren `RPWM` (OC1A) | BTS7960B |
| D12 | Fren `LPWM` (OC1B) | BTS7960B — ✅ eski GND jumper'ı söküldü |
| D13 | Fren kesme | IRF520 `SIG` |
| USB (`Serial0`) | Jetson bağlantısı | Mega'nın native USB portu — ikili protokol köprüsü |
| D18/D19 | `Serial1` | FlySky FS-i6X |
| D20/D21 | `I²C` (SDA/SCL) | **Yalnız MCP4725 gaz DAC'ı** |
| D22 | — boş | lazer UNO'da |
| D24 | Direksiyon referans anahtarı | `INPUT_PULLUP` — ⚠ parça alınmadı |
| **A0** | **Gaz DAC geri okuma** | MCP4725 `OUT` → doğrudan A0 (bölücü yok) |
| A1–A15 | — boş | A1'deki sahte enkoder silinecek, ACS712 sistemden çıkarıldı |

**Zamanlayıcı paylaşımı — çakışma yok:**
`Timer0` = `millis()` · `Timer1` = fren PWM (D11/D12) · `Timer4` = adım darbe treni (D8) ·
`Timer5` = Servo kütüphanesi (pan/tilt).

**Kesme pinleri:** D2 (E-STOP), D18/D19 (Serial1), D20/D21 (I²C) dolu.
Lazer D3'ten taşındığı için **INT1 boşta** — gösterge ucu için yedek giriş.

**⚠ USB tek başına bir bağlantı noktası değil, aracın komut hattı.** Mega'nın USB'si
ölürse Jetson bağlantısı da ölür (`Serial1` FlySky'da dolu). Yedek kart, USB-TTL ile
D0/D1'den bağlama yolu ve 5 V besleme uyarısı: `MEGA_YEDEK_KARTI.md`.

---

## 4. Direksiyon — step motor zinciri

**Zincir:** **F767ZI** → DM860H → 23IP65-20 → FLE57-05SW 1:10 → direksiyon mili

`config.h`'daki `STEER_GEARBOX_ORANI 10.0f` bu redüktörle uyuşuyor.

### 4.1 Kart künyesi

`2H Microstep driver DM860H` · `VAC: 18–80 V` · `VDC: 24–110 V` · 2,4–7,2 A tepe · 400–40000 adım/tur
→ 48 V paket (44,8–58,4 V) aralığın içinde.

### 4.2 Signal bloğu — 🔴 ORTAK ANOT (30 Ağustos 2026'da DEĞİŞTİ)

> **Bu bölüm 30 Ağustos'ta tersine döndü.** Önceki hâli **ortak katot**tu ve
> Mega'nın **5 V** mantığına göreydi (`D8`/`D9`/`D4`, aktif HIGH). F767 3,3 V
> sürüyor; DM860H'nin giriş direnci 5 V'a göre boyutlanmış olduğu için ortak
> katotta opto LED'i yetersiz akım çeker ve sürücü darbeyi kaçırabilir.
> **74HCT244 tam bu yüzden iptal edildi** — ortak anotta MCU yalnız 0 V'a
> çekiyor, opto akımı 5 V'tan geliyor, 3,3 V hiç devreye girmiyor.

| DM860H | Nereye | Not |
|---|---|---|
| `PUL+` , `DIR+` | **+5 V (UBEC)** | ortak anot |
| `PUL−` | **`PE9`** — CN12-52 (`D6`) | ⚠ **açık drenaj** |
| `DIR−` | **`PE10`** — CN12-47 (`D40`) | ⚠ **açık drenaj** |
| `ENA+` , `ENA−` | **BOŞTA** | sürücü açık kalır |

- **Aktif seviye LOW.** Pin `0 V`'a çekince opto iletir, darbe olur.
  Açık drenajda `HIGH` = Hi-Z → opto **kesin** söner. Push-pull sürersen
  3,3 V ile 5 V arasında ters akım yolu açılır.
- `PE9`/`PE10` **FT** (5 V toleranslı), hattın 5 V'a çıkması güvenli.
- **Seri direnç gerekmez** — DM860H girişleri dahili dirençli.
- **`ENA` boşta bırakılıyor.** Boşta ENA = sürücü açık. Hareket hâlinde
  direksiyonun serbest kalması, tutulan tekerlekten tehlikelidir.
  `PE12` (eski `ENA`) bu yüzden boşaldı.
- **`DIR` yönü denemeyle bulunur**, tahmin edilmez.

### 4.3 Güç bloğu

| Uç | Nereye |
|---|---|
| `A+` `A−` | A fazı — **siyah + yeşil** |
| `B+` `B−` | B fazı — **kırmızı + mavi** |
| `AC` `AC` | **48 V ana hat**, kendi sigortasıyla. Etikette "AC" yazıyor ama `VDC:24–110V` de destekliyor (içinde köprü diyot var) |

### 4.4 ⚠ İki ayrı toprak — karıştırma

> 🔴 **30 Ağustos'ta bu tablo da değişti.** Ortak anotta `PUL−`/`DIR−` artık
> GND'ye değil **MCU pinlerine** gidiyor.

| Taraf | Kural |
|---|---|
| **Sinyal** — `PUL−`/`DIR−` | **`PE9`/`PE10`'a** gider (açık drenaj), GND'ye **DEĞİL**. `PUL+`/`DIR+` **+5 V**'a |
| **Ortak toprak** | 🔴 **GEREKMİYOR ve ÇEKİLMEYECEK.** Girişler optokuplörlü; opto akımının dönüş yolu MCU pininin kendisi |
| **Güç** — `AC`/`AC` (48 V) | Kart GND'sine **ASLA BAĞLANMAZ.** Sürücünün `GND`/`AC` ucu 48 V'un eksisi, mantık toprağı değil. İzolasyonu bozan ve 48 V gürültüsünü karta taşıyan şey budur |

### 4.5 DIP anahtar ayarları

**Motor:** 23IP65-20 = NEMA23, **2,0 Nm, 5,0 A/faz**, IP65, 4 telli bipolar.

| Ayar | Değer | Anahtarlar |
|---|---|---|
| **Akım** | 4,86 A REF / 5,83 A tepe | `SW1 off · SW2 on · SW3 off` |
| **Duruş akımı** | Full Current | `SW4 on` |
| **Çözünürlük** | 1600 pulse/rev | `SW5 on · SW6 off · SW7 on · SW8 on` |

- **Akım:** anma 5,0 A'nın bir altındaki kademe. Bir üstü (5,43 A) motoru aşar.
- **`SW4 = on` sebebi:** direksiyon açık çevrim. Duruşta yol kuvveti motoru bir adım
  kaydırırsa **referans tamamen kaybolur** ve bunu görecek sensör yok (E6B2 traksiyon
  milinde). Isıya katlanmak, açı kaybetmekten iyi. Motor aşırı ısınırsa `off`'a alınıp
  yük altında tutup tutmadığı test edilir.
- **Çözünürlük:** 1600 × 1:10 = **16.000 darbe / çıkış turu = 44,4 darbe/derece.**
  Çıkış milinde 90°'yi 1,5 s'de dönmek 4.000 darbe / 1,5 s = **2,7 kHz** — Timer4
  donanım darbe treni için rahat. Bir üst kademe (3200) darbeyi 5,3 kHz'e çıkarır, gerek yok.

**Tork kontrolü:** 2,0 Nm × 1:10 ≈ 18–20 Nm çıkışta, FLE57'nin 30 Nm sınırının altında.

### 4.6 Faz eşlemesi — renk koduna güvenme, ölç

Multimetre direnç kademesinde:

- siyah ↔ yeşil ≈ **0,5–2 Ω** → bunlar A fazı
- kırmızı ↔ mavi ≈ **0,5–2 Ω** → bunlar B fazı
- siyah ↔ kırmızı = **sonsuz**

Bir fazın iki ucu ters takılırsa motor sadece ters döner (zararsız).
**İki farklı fazın telleri karışırsa motor titrer/inler ve dönmez.**

### 4.7 ⚠ EKSİK PARÇA: referans (homing) anahtarı

Step motor açık çevrim; E6B2 traksiyon miline gittiği için direksiyonda **hiç geri
besleme yok.** Açılışta direksiyonun nerede olduğu bilinmiyor.

- **Gereken: 1 adet mekanik mikro switch. Listede yok**, ucuz, yerelden alınır.
- Yerleşim: bir uç kilidine. Açılışta yavaşça o uca git → anahtar değince sıfırla →
  bilinen adım sayısıyla merkeze dön.
- **Akımdan stall algılamayı deneme.** Step motor dururken de nominal akımını çeker;
  DC motordaki temiz akım sıçraması olmaz. Eski `STEER_AKIM_ESIK` 8 A / 80 ms mantığı
  step motorda çalışmaz.
- Yazılımda **sanal açı limiti** de kalmalı (referanstan ±N adım), mekanik uca dayanıp
  adım kaçırmasın.

---

## 5. Fren

### 5.1 BTS7960B (IBT-2) → F767ZI

> 🔴 **30 Ağu 2026'da Mega'dan F767ZI'ye taşındı.** Eski `D10/D11/D12`
> atamaları geçersiz. Tezgah testi: `testler/fren_f767/`.

| BTS7960B | Nereye | Morpho | Not |
|---|---|---|---|
| `B+` / `B−` | **4S LiPo 14,8 V** | — | ⚠ tavan ~27 V, 48 V'a ASLA bağlama |
| `M+` / `M−` | Fren aktüatörü (12 V) | — | |
| `VCC` | 5 V (UBEC) | CN11-18 | |
| `GND` | **Ortak şase** | CN11-19 | burada ortak GND ŞART |
| `RPWM` | **`PC6`** (D16) — "fren UYGULA" | **CN12-4** | TIM3_CH1 |
| `LPWM` | **`PC7`** (D21) — "fren SERBEST" | **CN12-19** | TIM3_CH2 |
| `R_EN` + `L_EN` | köprülenir → **`PC8`** (D43) | **CN12-2** | düz GPIO |
| `R_IS` / `L_IS` | ops. akım okuma → `A0`/`A1` | CN9-1 / CN9-3 | `PA3` / `PC0`, ADC123 |

✅ **Zamanlayıcı:** `PC6`/`PC7` aynı TIM3'ün CH1/CH2'si — çakışma yok.
TIM3, gösterge hattı (`PB4`) 30 Ağustos'ta iptal edilince boşalmıştı.

🔴 **`RPWM`/`LPWM`/`EN` üçünde de 10 kΩ pull-down ŞART.** Mega'da iyi
alışkanlıktı, F767'de **zorunlu**: STM32 reset'ten `setup()`a kadar pinleri
giriş-yüzer bırakır, o aralıkta köprüyü kapalı tutan tek şey bu dirençlerdir.

🔴 **Doğrulanmamış: girişler 3,3 V ile sürülüyor** (Mega'da 5 V'tu).
BTS7960B'nin eşiği `VCC`ye göre tanımlı. Tasarımdaki tek açık madde bu;
`testler/fren_f767/` onu ölçüyor. Çıkmazsa 74HCT244 (5 V tampon) geri gelir.

- ✅ D12 → GND jumper'ı 24 Ağu 2026'da SÖKÜLDÜ. Bu kartta zaten konu değil,
  ama eski belgelerde "şart" yazıyorsa o bilgi hem eski hem tehlikeli.
- ⚠ İlk denemede **`FREN_TERS` doğrulanmalı** — yanlışsa "fren uygula" komutu
  freni bırakır. Araç sabit, tekerlekler yerden kesik denenir.
- `BRAKE_PWM_MAX` ~%75'e (190/255) kırpılır — 16,8 V × 0,75 ≈ 12,6 V.
- ⚠ **`BRAKE_STALL_MS` (2500 ms) ve `BRAKE_PWM_MIN` (0) hâlâ ölçülmedi.**
  Fren hattında akım ölçümü yok; stall'a karşı tek koruma bu zaman aşımı.

### 5.2 Fren kesme (IRF520) — ❌ İPTAL (30 Ağu 2026)

Kontrolcüye giden fiziksel "fren varken gazı kes" hattı **tasarımdan çıktı.**
IRF520 bağlanmayacak, `D13`/`PE15` bu iş için kullanılmayacak.

⚠ **Sonucu bilinerek kabul edildi: bu koruma artık YALNIZ yazılımda.**
Firmware donarsa kontrolcü gaz komutunu görmeye devam eder.

> `PE15` (CN12-53, D37) önce SSR ana güç kesmesine verilmişti; **1 Eylül
> 2026'da SSR de iptal oldu.** Pin şu an BOŞTA, hiçbir şeye bağlı değil ve
> firmware onu sürmüyor (`SSR_VAR false` → `pinMode` bile çağrılmıyor).
> Eski "Fren kesme = PE15" satırı da geçersiz.

---

## 6. F767ZI sensör kartı

**Ethernet RMII şu pinleri tutuyor — sensör için kullanma:**
`PA1, PA2, PA7, PC1, PC4, PC5, PG11, PG13, PB13`

| Sinyal | F767ZI pini | Kart üstünde | Not |
|---|---|---|---|
| E6B2 `A` | **PD12** (TIM4_CH1) | **CN10-21**, `D29` | Donanımsal enkoder sayacı |
| E6B2 `B` | **PD13** (TIM4_CH2) | **CN10-19**, `D28` | Aynı sayaç |
| E6B2 `Z` (index) | PD11 (EXTI) | **CN10-23**, `D30` | İsteğe bağlı, tur başı sıfırlama |
| E6B2 besleme | 5 V + ortak GND | — | NPN açık kollektör; **pull-up 3,3 V'a** (5 V'a değil) |

> 🔴 **BNO055 I²C DEĞİL, UART.** Eski satırlar `PB8`/`PB9` (I2C1, adres 0x28)
> diyordu — **geçersiz.** Gerekçe: BNO055'in clock-stretching hatası I²C'yi
> kilitliyor, İKA'da bu kilit aracın tamamını donduruyordu. 30 Ağustos'ta
> I²C hattının tamamı tasarımdan çıktı (gaz DAC'ı da dahili DAC'a taşındı),
> araçta tek I²C cihazı kalmadı. Tezgah testi: `testler/bno055_f767/`.
| ~~Gösterge ucu (hız frekansı)~~ | ~~PB4 (TIM3_CH1)~~ | ~~CN7-19, `D25`~~ | ❌ **İPTAL (30 Ağu)** — hız tek kaynaktan, E6B2'den. 🔴 Boşalan **TIM3 fren PWM'ine verildi** (§5.1); bu hat geri gelirse PB4 değil, TIM3 dışı bir pin seçilecek |
| BNO055 `TX` → kart `RX` | **PD6** (USART2_RX) | **CN11-43**, `D52` | ⚠ **ÇAPRAZ** |
| BNO055 `RX` ← kart `TX` | **PD5** (USART2_TX) | **CN11-41**, `D53` | ⚠ **ÇAPRAZ** |
| BNO055 `RST` | PD7 | **CN11-45**, `D51` | LOW = reset, opsiyonel |
| BNO055 `PS1` | → **VCC** | — | 🔴 çekilmezse çip I²C modunda kalır, UART'tan tek bayt gelmez |
| F767ZI → Jetson | **USB** (ST-LINK VCP) | CN1 | Ek kablo yok, kart Jetson USB'sinden beslenir |

Konumlar **UM1974 Tablo 19** (NUCLEO-F746ZG / F756ZG / **F767ZI** pin atamaları)
ile doğrulandı. PD11/PD12/PD13 kartın QSPI hattı; Nucleo-144'te QSPI flash takılı
olmadığı için üçü de serbest GPIO.

⚠ **PB4 varsayılanda `NJTRST`** (JTAG) işlevinde. Nucleo SWD kullandığı için pin
serbest, ama CubeMX'te **Debug = "Serial Wire"** seçilmezse GPIO olarak
kullanılamaz. Saat yiyen tuzaklardan biri.

⚠ **Ethernet ortak pin uyarıları (UM1974 Tablo 19 dipnotları):** `PA7` (D11,
CN7-14) **JP6 kapalıysa** Ethernet PHY'nin `RMII_DV`'sine de bağlanır; `PB13`
(CN7-5) **JP7 kapalıysa** `RMII_TXD1`'e bağlanır. İkisinde de aynı anda tek işlev
kullanılabilir. Bizim seçtiğimiz altı pinin hiçbiri bu listede değil.

### Fritzing parçası

`~/ika/tasarim/fritzing/Nucleo-144-MB1137.fzpz` — gerçek kart grafiği, 276 pin
(Zio CN7–CN10 + morpho CN11/CN12). Kaynak: Fritzing forum, Bruno VIVIEN /
vanepp. **`connector134` orijinalinde `PD!!` yazıyordu, `PD11` olarak
düzeltildi**; kalan 275 pin UM1974 ile tutarlı.

⚠ **Jetson'da port adı kayar.** `serial_bridge_node` seri portu sabit kodlu
(`/dev/ttyCH341USB0`) — reboot sonrası yanlış karta yazabilir.
**`/dev/serial/by-id/` kullan.**



### F767ZI güç pinleri (UM1974 Tablo 19, CN8)

| Pin | Konum |
|---|---|
| `+3.3V` | **CN8-7** |
| `+5V` | **CN8-9** |
| `GND` | **CN8-11** ve **CN8-13** |
| `VIN` | CN8-15 |
| `NRST` | CN8-5 |

### E6B2 enkoder → F767ZI

> 🔴 **30 Ağu 2026 düzeltmesi:** 5 V tüketen çevre birimleri (E6B2, FlySky
> alıcı, röle bobini, BTS7960B lojiği) **UBEC 5 V/3 A çıkışından** beslenir,
> kartın `CN8-9` pininden DEĞİL. Kart `JP3`→`U5V` ile USB'den besleniyor;
> `CN8-9`'un payı dar (kart 300 mA, 500 mA'da LD5) ve UBEC'i oraya bağlamak
> Jetson USB'sine geri besleme olur. Ortak toprak yine şart.

| E6B2 kablo | Nereye | Kart konumu |
|---|---|---|
| Kahverengi (+Vcc) | **UBEC 5 V** | — |
| Mavi (0V) | GND | CN8-11 |
| **Siyah (A)** | **PD12** | CN10-21 (`D29`) |
| **Beyaz (B)** | **PD13** | CN10-19 (`D28`) |
| **Turuncu (Z)** | **PD11** | CN10-23 (`D30`) |
| Ekran | GND — **tek uçtan** | CN8-13 |

Artı **3 × 1 kΩ pull-up**: A, B, Z'den **3V3'e (CN8-7)**.

- ⚠ **Pull-up 3,3 V'a, 5 V'a DEĞİL.** E6B2 NPN açık kollektör; çıkış yalnız toprağa
  çeker, "1" seviyesini pull-up gerilimi belirler. 5 V'a çekilirse STM32 pini 5 V görür.
  Besleme 5 V'tan olabilir, o ayrı mesele.
- **1 kΩ seçildi** (4,7 kΩ değil): kablo motor yanından geliyor; düşük direnç hem
  gürültü bağışıklığı hem hızlı kenar verir.
- **Ekran tek uçtan topraklanır** — iki uçta toprak döngüsü olur, ekran antene döner.
- Kablo motor faz kablolarıyla aynı kanaldan geçmesin; A/B çifti burkulu olsun.
- ✅ **Renk kodu datasheet'ten doğrulandı** (`q085_e6b2-c` — *"Output signal
  (Black: phase A, White: phase B, Orange: phase Z)"*, `Blue`, `Brown`, `Shield`).
  NPN sürüm: besleme **5 VDC −5% … 24 VDC +15%**, çıkış **35 mA / 30 VDC max**.
  1 kΩ @ 3,3 V = 3,3 mA — sınırın çok altında.
- ⚠ **KABLO BOYU 0,5 m.** Datasheet: *"E6B2-CWZ6C: 5-dia. vinyl-insulated round
  cable with 5 conductors … Standard length: 0.5 m"*. Enkoder traksiyon motorunda,
  kart panoda — **yetmez, uzatma gerekecek.** Uzatma ekranlı olmalı, A/B burkulu,
  ek yeri lehimli+makaronlu (klemens değil, titreşim var), ekran sürekliliği
  korunmalı ama yine tek uçtan topraklanmalı.
  Uzun kablolu sürüm (2 m) varsa onu almak daha temiz.

### BNO055 → F767ZI

Boardoza modülü — **KiCad şeması incelendi** (`hardware files/B-BNO055-Brk-01Mbr-R01.kicad_sch`):

| Modülde ne var | Sonuç |
|---|---|
| `U2` = **TLV73333PDBVR** 3,3 V LDO | Giriş 3,3–5,5 V kabul ediyor |
| `Q1`,`Q2` = **2N7002**, netler `SDA`/`SCL` ↔ `SDA_3.3`/`SCL_3.3` | **Çift yönlü seviye çevirici var** |
| **4 × 10 kΩ pull-up** — 2'si `VCC` altında, 2'si `+3V3` altında | **Harici pull-up GEREKMİYOR** |
| `Y1` = 32,768 kHz kristal | Harici saat, füzyon kalitesi için |

**J1 (I²C):** `VCC` · `SCL` · `SDA` · `GND` — ⚠ **SCL, SDA'dan ÖNCE geliyor**
**J2 (ek):** `BOOT_LOAD` · `BL_IND` · `ADR` · `INT` · `PS1` · `PS0`

| BNO055 | Nereye | Kart konumu |
|---|---|---|
| `VCC` (J1) | **3V3** | CN8-7 |
| `SCL` (J1) | **PB8** | CN7-2 (`D15`) |
| `SDA` (J1) | **PB9** | CN7-4 (`D14`) |
| `GND` (J1) | GND | **CN7-8** |
| `PS0` (J2) | **GND** | J1 GND'sine köprü |
| `PS1` (J2) | **GND** | J1 GND'sine köprü |
| `ADR` (J2) | **GND** | J1 GND'sine köprü |
| `BOOT_LOAD`, `BL_IND`, `INT` | boşta | — |

- **`VCC` mutlaka 3V3'ten.** Dış taraftaki I²C pull-up'ları `VCC`'ye bağlı; `VCC`'ye
  ne verilirse hat o gerilimde çalışır. 5 V verilirse SDA/SCL 5 V'a çıkar.
  Bu şemadan doğrulandı, tahmin değil.
- **`PS0`/`PS1`/`ADR` → GND.** Şemada bu üç net çipten doğrudan J2'ye gidiyor,
  üzerlerinde pull direnci görülmedi (kesin değil — direnç uç geometrisi çözülemedi).
  GND'ye bağlamak her iki durumda da doğru: pull-down varsa zararsız, yoksa
  havada kalmayı engelliyor. Sonuç kesin: **I²C modu, adres 0x28.**
  Üretici kütüphanesi de `BNO055::BNO055() { address = 0x28; }` diyor.
- `BOOT_LOAD` yanında 3V3'e giden 10 kΩ (`R9`) var — normal çalışma için yukarı
  çekili olmalı, öyle görünüyor. **Çip hiç cevap vermezse ilk bakılacak yer burası.**
- **GND için CN7-8 kullanıldı** (CN8-11 yerine): I²C ile aynı konnektörden çıkıyor,
  kablo kısa ve güzergâh temiz. İkisi de aynı net.
- **✅ 2026-08-24: çizimde bağlandı ve doğrulandı** — `VCC`→3V3, `SCL`→CN7-2,
  `SDA`→CN7-4, `GND`→CN7-8, `PS0`/`PS1`/`ADR` GND'ye köprülü.
- **Mekanik:** IMU aracın dönme merkezine yakın, sağlam, eksenleri `base_link` ile
  hizalı monte edilmeli. Sonradan yer değişimi EKF kalibrasyonunu bozar.

### Gösterge ucu → F767ZI (optokuplörle) — ❌ İPTAL (30 Ağu 2026)

> Gösterge hattı tümüyle çizimden çıktı; odometri **E6B2'ye** bağlandı.
> Aşağısı gerekçe ve tasarım kaydı olarak duruyor — **uygulanmayacak.**
> 🔴 Geri gelirse `PB4` kullanılamaz: TIM3 artık fren PWM'inin (§5.1).


Sinyal motor kontrolcüsünden geliyor — 48 V sisteminin toprağında ve gürültülü.
**Doğrudan bağlama, direnç bölücü de kullanma:** bölücü gürültüyü geçirir ve iki
toprağı birleştirir.

| Bağlantı | |
|---|---|
| Gösterge ucu → seri direnç → PC817 anot (1) | 12 V ise **2,2 kΩ**, 5 V ise **470 Ω** |
| PC817 katot (2) → **kontrolcünün GND'si** | izolasyonun kirli tarafı |
| PC817 kollektör (4) → **PB4** (CN7-19, `D25`) | + 4,7 kΩ pull-up 3V3'e |
| PC817 emiter (3) → **F767ZI GND** | izolasyonun temiz tarafı |

- Sinyal ters döner; frekans sayıldığı için önemsiz.
- **Sınır: PC817 geçiş süresi ~4 µs → ~20 kHz'e kadar güvenli.** Ölçümde daha
  yüksek çıkarsa 6N137 gerekir.
- ⚠ **Optokuplör listede yok.**
- Seri direnç değeri, §10'daki *"gösterge ucunun sinyal seviyesi"* ölçümüne bağlı.

### 🔴 SSR ana güç kesmesi neden iptal edildi (1 Eylül 2026)

**Ölçüldü, SSR kapalı komuttayken (`ssr=0`, `ch7=1000`):**

| | Çıkış gerilimi |
|---|---|
| Yüksüz | ~50 V (10 MΩ multimetre — hayalet gerilim, teşhis değeri yok) |
| Yüklü | **16-25 V arası salınım** |

Gerilimin yük altında çökmesi kaynağın yüksek empedanslı olduğunu gösteriyor:
polarite hatası ya da yapışma değil, **normal SSR kaçağı**. Ama kaçak
kontrolcünün giriş kondansatörlerini doldurmaya yetiyor — kontrolcü uyanıyor,
akım çekiyor, gerilim çöküyor, düşük gerilim korumasına girip bırakıyor,
tekrar doluyor. 16↔25 V salınımı saniyede birkaç kez resetlenen bir
kontrolcünün imzası.

Yani **"ana güç kesildi" denen durumda yük hâlâ kısmen besleniyordu.** Kaçak
bir arıza değil, yarı iletkenin karakteristiği; SSR değiştirmek aynı sonucu
verirdi. Donanım söküldü, `SSR_VAR false` yapıldı.

**Kesme katmanları bu karardan sonra:**

| Katman | Durum |
|---|---|
| Mantar butonun **mekanik kontağı** — 48 V'u doğrudan keser | ✅ firmware'den bağımsız |
| `PF14` E-STOP okuma (kart durumu görür) | ✅ |
| SwA/CH7 → gaz rölanti + fren bas | ✅ |
| SwA/CH7 → **donanımsal** güç kesme | ❌ kalktı — kablosuz durdurma artık YAZILIMSAL |

🔴 **AÇIK MADDE:** şartname ayrı bir **donanımsal kablosuz** acil stop istiyor
mu? Şartname hâlâ okunmadı (`ACIL_STOP.md:73`). İstiyorsa bu karar geri
alınmayacak; yerine kontrolcünün **kontak (ignition) hattı** kesilecek — orada
akım miliamper, kaçak kontrolcüyü uyandırmaya yetmez.

⚠ SSR sökülünce **ANL 60 A sigorta ve mantar buton seri kalmalı** — hattın
tek koruması ve tek gerçek kesmesi onlar.

### F767ZI → Jetson bağlantısı

> 🔴 **1 Eylül 2026: KARAR DEĞİŞTİ — veri yolu USB DEĞİL, UART.**
> Aşağıdaki "tek USB kablosu / USART3 PD8-PD9 / 921600 baud / udev" düzeni
> **geçersiz**, tarihsel kayıt olarak duruyor. Yürürlükteki bağlantı:
>
> | F767ZI | Jetson Orin Nano J12 | Yön |
> |---|---|---|
> | `PG14` = **D1** = **CN10-14** (USART6_TX) | **pin 10** UART1_RX | kart → Jetson |
> | `PG9` = **D0** = **CN10-16** (USART6_RX) | **pin 8** UART1_TX | Jetson → kart |
> | `GND` = **CN12-9** | **pin 6** GND | tek toprak teli |
>
> **115200 baud.** Jetson'da **`/dev/ttyTHS1`** — sabit ad, udev gerekmez,
> USB portu ve bant genişliği yemez.
> ⚠ Jetson'ın `3,3 V` / `5 V` pinlerine (J12-1/2/4) **bağlanmayacak**; kart
> 12 V'tan beslenir. İki cihaz arasında **tek ortak toprak** yeterli.
> ⚠ `/dev/ttyTHS1` fabrika çıkışında seri konsola ayrılmış olabilir:
> `sudo systemctl stop nvgetty && sudo systemctl disable nvgetty`.
> ⚠ **Kod atma ayrı yoldan:** `CN1` USB'si panele uzatılır (uzaktan
> `st-flash`), gerekirse çekilip laptop'a takılır. UART hep bağlı kalır.
>
> Pinler iki bağımsız kaynaktan doğrulandı: Arduino çekirdeğinin
> `PeripheralPins.c` + `variant_NUCLEO_F767ZI.h`, ve UM1974/Zephyr.
> **Komut yolunun firmware tarafı 2 Eylül 2026'da yazıldı** — `PKT_J_*`
> paketleri, heartbeat zaman aşımı ve SwC kip hakemi: `F767_FIRMWARE/OKUBENI.md`.

**Tek USB kablosu (❌ ESKİ KARAR):** `CN1` (kartın micro-USB'si, ST-LINK tarafı) → Jetson USB-A.
Aynı kablo debug/flash, sanal COM portu ve kart gücünü birlikte taşır.
⚠ **Micro-B kablo listede yok** (50. kalem USB-B, o Mega/UNO tipi).

**Güç — UM1974 §7.4.1:** `JP3` → `U5V` (pin 3-4, varsayılan).
USB takılınca önce yalnız ST-LINK beslenir (host 100 mA); enumerasyonda kart
**300 mA** ister. Host veremezse hedef STM32 hiç beslenmez ve **yeşil LD6 yanmaz**
— teşhis bu kadar basit. 500 mA aşılırsa kırmızı LD5 yanar.
Jetson'ın USB3 portu 900 mA verir, doğrudan sorun yok; **risk pasif hub'da.**
Olursa: kartı `E5V` (CN11-6) veya `VIN`'den besle, `JP3`'ü oraya al, USB yalnız veri.

**⚠ Firmware `USART3` kullanacak — `PD8` TX / `PD9` RX** (UM1974 §7.9).
Nucleo-64 örneklerinde VCP `USART2`'dir; Nucleo-144'te USART2 yazılırsa hiçbir şey
görünmez. `SB5`/`SB6` varsayılanda kapalı olduğundan PD8/PD9 morpho'dan kullanılamaz
(bizim pinlerimizi etkilemiyor).

**Jetson'da udev — mevcut düzene uy** (`/dev/mega`, `/dev/turret`, `/dev/kamera_nisan`):

```bash
udevadm info -a -n /dev/ttyACM0 | grep '{serial}' | head -1

# /etc/udev/rules.d/99-lydia.rules
SUBSYSTEM=="tty", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="374b", \
  ATTRS{serial}=="<SERI_NO>", SYMLINK+="stm32", MODE="0666"

sudo udevadm control --reload && sudo udevadm trigger
```

Seri numarayla eşle — ikinci bir ST-LINK'te VID/PID aynı olur. **Baud 921600.**

**⚠ USB port bütçesi.** Jetson'da 4 USB3 portu; sırada Mega, taret UNO, nişan
kamerası, LiDAR, Gemini 335L ve F767ZI var → hub şart (listede Anker 4 portlu).
**Gemini 335L'yi hub'a TAKMA** — USB3 bant genişliğini tek başına yiyor, paylaşınca
kare düşürür veya hiç açılmaz. Hub'a düşük hızlı cihazlar, kamera doğrudan Jetson'a.

**İleride seçenek:** kartın RJ45'i duruyor ve RMII pinleri bilerek boş bırakıldı.
Ethernet/UDP bağlantısı port kayması derdini bitirir ve gecikmeyi düşürür — ama
ciddi firmware işi, Mardin öncesi girilmez.

### E6B2 neden Mega'ya değil F767ZI'ye

600 P/R × kuadratür 4 = **2.400 sayım/tur.** Enkoder motor miline 1:1 takılıyor;
motor 4.000 rpm'de dönerse kanal başına 40 kHz, kuadratür ×4 = **160.000 sayım/s.**
Mega'da kenar başına ~6 µs düşer, bir AVR kesme rutini zaten 5–8 µs sürüyor ve `Wire`
I²C bloklarken sayım kaçıyor — **imkânsız.** F767ZI'nin TIM4 donanım sayacı bunu CPU'yu
hiç meşgul etmeden sayar.

⚠ **Motorun boştaki devri ölçülmeli.** E6B2-CWZ6C'nin mekanik limiti **6.000 rpm**;
48 V 1200 W BLDC'ler 3.000–4.500 bandında çalışır, sınır yakın. Ölçüm 6.000'i aşarsa
enkoder mile değil aks tarafına alınmalı.

---

## 7. Değişmeyen sistemler

| Sistem | Nasıl | Pin |
|---|---|---|
| Gaz | MCP4725 I²C DAC → kontrolcü gaz sinyali (1–4,2 V), %30 güç | D20/D21 |
| Geri vites | Tek 5 V aktif-düşük Songle rölesi, kontrolcünün geri vites çiftini kısa devre eder | D7 |
| E-STOP | Buton, `INPUT_PULLUP` | D2 |
| Taret | **Ayrı Arduino UNO** — pan/tilt servo + lazer. Mega'daki mükerrer dal silinecek | Mega'da D5/D6/D22 boşalır |

### ⚠ A0 firmware'de yanlış isimde

`config.h:191` A0'ı `ENC_SOL_PIN` diye tanımlıyor, `main.cpp:593` onu AS5600
enkoderi sanıp Jetson'a `PKT_ENC` ile **"sol enkoder"** olarak gönderiyor.
Fiziksel gerçek: **A0 gaz DAC'ının çıkışına bağlı**, A1 ise hiçbir yere bağlı değil
(havada duran ADC girişi → rastgele değer).

Yani Jetson bugün DAC voltajını "sol enkoder", gürültüyü "sağ enkoder" diye alıyor.

**A0 silinmemeli, işini yapmalı.** DAC çıkışını geri okumak gaz komutunun gerçekten
çıkıp çıkmadığını doğrular — 17 Ağu'daki DAC arızasını anında yakalardı.

| | |
|---|---|
| Bağlantı | MCP4725 `OUT` → **A0 doğrudan.** 0–5 V, AREF zaten 5 V, bölücü gerekmez; DAC çıkışı tamponlu, ADC'yi rahat sürer |
| Yeni isim | `ENC_SOL_PIN` → **`GAZ_DAC_GERI_PIN`** |
| Beklenen aralık | Gaz sinyali 1–4,2 V → 10-bit'te **~205–860 sayım** |
| Arıza ölçütü | Yazılan ile okunan **>30 sayım (≈0,15 V)**, **>100 ms** ayrışırsa → gaz yolu kopuk / DAC ölü → sür |

`ENC_SAG_PIN` (A1) gerçekten silinmeli. `PKT_ENC` paketini Jetson tarafında
kullanan bir düğüm varsa çöp veri besleniyor — kontrol edilmeli.

**ACS712 sistemden tamamen çıkarıldı** (24 Ağu kararı). Sonucu: direksiyonda sorun
değil, DM860H faz akımını kendi sınırlıyor. **Frende ise tek koruma
`BRAKE_STALL_MS` = 2500 ms zaman aşımı olarak kalıyor** — bu süre tam stroke
süresinden uzun olmalı, yani *"fren aktüatörünün tam stroke süresi"* ölçümü
Manisa listesinde zorunlu madde olarak duruyor (§10).

**Traksiyon akım ölçümü için yeni donanım gerekmiyor:** JK BMS paket akımını BLE'den
veriyor, `testler/bms_ble/bms_oku.py` altyapısı yazılı. ACS758 almaya gerek yok.

**Boşa çıkan donanım:** Songle röle ×4 (direksiyondan), 2. BTS7960B (fren yedeği),
AD5693R DAC (MCP4725 kalibre ve çalışır durumda), IRF520 ×3.

---

## 8. Acil stop — iki kanallı NC

> 🔴 **GEÇERSİZ (29 Ağu 2026).** Bu bölüm Mega D2/D3 planına göre yazıldı;
> kart F767ZI oldu ve kesme artık donanımdan (röle + 74HCT244 `1OE`) yapılıyor.
> **Geçerli belge: `genel/ACIL_STOP.md`.** Aşağıdaki mantık tablosu ve
> polarite uyarısı hâlâ doğru, pinler değil (`PF14`/`PF15`).

| Kanal | Pin | Kesme | Bağlantı |
|---|---|---|---|
| NC #1 | **D2** | INT0 | kontak bir ucu pine, öteki ucu GND |
| NC #2 | **D3** | INT1 | aynı |

İkisi de `INPUT_PULLUP`.

**Mantık (NC → GND):**

| Durum | Kontak | Pin |
|---|---|---|
| Buton basılı değil | kapalı | `LOW` |
| Buton basılı | açık | `HIGH` |
| **Kablo kopuk / soket çıkmış** | açık | **`HIGH` → E-STOP tetiklenir** ✅ |

Son satır NC kullanmanın bütün amacı: arıza güvenli tarafa düşüyor.

### ⚠ FIRMWARE POLARİTESİ TERS — bağlamadan önce düzelt

```
config.h:187   #define ESTOP_BTN_PIN 2   // INPUT_PULLUP (LOW = buton basılı)
main.cpp:795   bool btn_basili = (digitalRead(ESTOP_BTN_PIN) == LOW);
```

Mevcut kod `LOW = basılı` bekliyor. NC doğru bağlanırsa **araç açılır açılmaz
sürekli E-STOP'ta kalır.** `LOW` → `HIGH` çevrilmeden kablolama yapılmamalı.

### İki kanalın değerlendirilmesi

- Biri açılırsa → E-STOP.
- **İkisi 100 ms'den uzun süre farklı okursa → arıza bayrağı.** Bir kontak
  yapışmış ya da bir hat kopmuş demektir; tek kanalda bu asla görülemez.
  İki kanal koymanın asıl kazancı budur.
- Kablo uzunsa her iki hatta pin–GND arası **100 nF** — motor gürültüsü sahte
  tetikleme yapmasın.

### ⚠ Açık konu: donanımsal kesme hâlâ yok

İki NC'yi de Mega'ya vermek **durum okuma** demek; kesme yazılımdan oluyor ve
Mega donarsa (örn. I²C kilidi) ikisi de işe yaramaz. `KONTROLCU_KABLO_HARITASI.md:121`
kendi ilkemizi yazıyor: *"acil durdurma donanımdan olmalı, yazılımdan değil."*

Seçenek: **2. NC'yi kontrolcünün kontak (ignition) hattına seri koymak** →
Mega'dan tamamen bağımsız donanımsal kesme. SSR-100DD takılmayacağı için bugün
başka donanımsal kesme yolu yok.

---

## 9. Alınacak / eksik

| Ne | Neden |
|---|---|
| **PC817 optokuplör ×1** | Gösterge ucunu 48 V tarafından izole etmek için |
| **1 kΩ ×3, 4,7 kΩ ×1 direnç** | E6B2 pull-up'ları (×3) ve gösterge optokuplör pull-up'ı (×1). BNO055'e gerekmiyor — modülde 10 kΩ'lar var |
| **USB-A ↔ micro-B kablo** | F767ZI → Jetson (listedeki USB-B kablo Mega/UNO tipi) |
| **Ekranlı uzatma kablosu (E6B2)** | Enkoderin kendi kablosu yalnız **0,5 m**, motordan panoya yetmez |
| **Mekanik mikro switch ×1** | Direksiyon referansı. **Bu olmadan step motor açılışta açıyı bilmez, Nav2 yanlış açıdan başlar.** |
| **ANL 60 A sigorta + yuvası** | 48 V ana hattın tek koruması |
| **DM860H için sigorta** | 48 V hattından ayrı besleniyor |
| **E6B2 kaplini + braketi** | Motor miline bağlantı |

**Yanlış gelecek üç kalem** (link hataları, sipariş edilmiş):

| Sıra | Kalem | Sorun |
|---|---|---|
| 7 | "7.5 A Bıçak Sigorta" ×10 | Link `75a-maksi-oto-sigorta` → 75 A MAKSİ geliyor |
| 41 | "Sigorta seti — STANDART (ATO/ATC)" | Link mini set |
| 22 | "Silikon kablo 10 mm² SİYAH" ×2 | Link `10-awg-silikon-kablo-siyah` → 10 AWG = 5,26 mm². Kırmızısı gerçekten 10 mm² |

---

## 10. Manisa'da ölçülecekler

1. **Motorun boştaki devri** — E6B2'nin 6.000 rpm limitini aşıyor mu.
2. **Motor–tekerlek dişli oranı** — odometri ölçeklemesi için.
3. **Direksiyon mili → tekerlek açısı oranı** — mekanik bağlantıdan, adım/derece hesabı için.
4. **Fren aktüatörünün en düşük hareket PWM'i** → `BRAKE_PWM_MIN` (şu an 0).
5. **Kontrolcünün düşük seviye fren ucunun boştaki gerilimi** — 5 V mı 12 V mi.
6. **İki tekli mor kabloyu ayır** — biri yüksek seviye fren *girişi* (+12 V bekler),
   öteki gösterge *çıkışı*. Akü bağlı, gaz yok, tekerleği elle çevir:
   **gerilimi oynayan uç GÖSTERGEDİR.** Göstergeye 12 V vermek beyni bozabilir.
7. **İki beyaz self-learning ucu ayrı ve yalıtılmış mı** — takılı kalırsa kontrolcü faz
   öğrenme moduna girer ve **tekerlek kendi kendine döner.**
8. **Motor fazlarının direnç ölçümü** (§4.6).

---

## 11. Bu belgeyle geçersizleşenler

| Yer | Ne yanlış |
|---|---|
| `ARAC_FIRMWARE/config.h:96` | "Step motor + DM860H terk edildi (redüktör yok)" — redüktör alındı, geri dönüldü |
| `config.h:100–102` | `STEER_ROLE_*` pin tanımları — röleli direksiyon kalktı |
| `config.h:109–123` | `STEER_AKIM_*` bloğu (8 A / 80 ms sanal limit) — step motorda çalışmaz |
| `config.h:69` | "Direksiyonda konum sensörü yok, açı ölü hesapla" — artık adım sayımından, ama referans anahtarı şart |
| `config.h:191–192` + `main.cpp:592–597` | `ENC_SOL_PIN` (A0) aslında **gaz DAC çıkışı** — `GAZ_DAC_GERI_PIN` olarak yeniden adlandırılıp geri okuma yapacak. `ENC_SAG_PIN` (A1) boşta, silinecek. `PKT_ENC` bugün Jetson'a çöp gönderiyor |
| `config.h:177–184` | `PAN_SERVO_PIN` / `TILT_SERVO_PIN` / `LAZER_PIN` — taret UNO'da, Mega'daki dal silinecek |
| `config.h:170` | `IMU_I2C_ADDR 0x68` (BMI160) — IMU F767ZI'ye taşındı, Mega'daki Madgwick dalı silinecek |
| `config.h:187` + `main.cpp:795` | E-STOP `LOW = basılı` — **NC kablolamada TERS**, `HIGH`'a çevrilecek |

`ANA_NOT.md` §2 direksiyon satırları 24 Ağustos'ta güncellendi.

---

## 12. Morpho konumları — UM1974 Tablo 21 ile doğrulanmış

Kaynak: UM1974 Rev 11, **Tablo 21** (NUCLEO-F767ZI dahil ailenin ST morpho
tablosu). 29 Ağu 2026'da PDF'ten çıkarıldı; `CN12-2 = PC8` süreklilik ölçümüyle
araçta ayrıca teyit edildi.

⚠ **`CN11` ve `CN12` iki AYRI morpho başlığı**, kartın karşılıklı kenarlarında,
ikisinin de pin numarası 1'den başlıyor. Karışmanın kaynağı bu. (Belgedeki
`CN8-7`/`CN8-9`/`CN8-11` gibi atıflar **Zio** tarafıdır, morpho değil.)

| Sinyal | Pin | Morpho | Dxx |
|---|---|---|---|
| Gaz DAC | `PA4` | **CN11-32** | D24 |
| Fren `RPWM` | `PC6` | **CN12-4** | D16 |
| Fren `LPWM` | `PC7` | **CN12-19** | D21 |
| Fren `R_EN`+`L_EN` | `PC8` | **CN12-2** | D43 |
| Geri vites | `PE14` | **CN12-51** | D38 |
| ~~SSR ana güç kesme~~ | ~~`PE15`~~ | ~~CN12-53~~ | ~~D37~~ | ❌ **İPTAL (1 Eyl 2026)** — SSR kaçak akımdan kesmiyordu, söküldü. Pin BOŞTA, `SSR_VAR false`
| Step `PUL` | `PE9` | **CN12-52** | D6 |
| Step `DIR` | `PE10` | **CN12-47** | D40 |
| Step `ENA` | `PE12` | **CN12-49** | D39 |
| E-STOP okuma | `PF14` | **CN12-50** | D4 |
| (E-STOP 2. kanal, boş) | `PF15` | **CN12-60** | D2 |
| Enkoder A | `PD12` | **CN12-43** | D29 |
| Enkoder B | `PD13` | **CN12-41** | D28 |
| Enkoder Z | `PD11` | **CN12-45** | D30 |
| ~~Gösterge ucu~~ | ~~`PB4`~~ | ~~CN12-27~~ | ❌ **iptal (30 Ağu)** — hız tek kaynaktan, E6B2'den. TIM3 boşaldı, fren PWM'i aldı |
| iBUS | `PD2` | **CN11-4** | D48 |
| BNO055 TX | `PD5` | **CN11-41** | D53 |
| BNO055 RX | `PD6` | **CN11-43** | D52 |
| (BNO reset, kullanılmıyor) | `PD7` | **CN11-45** | D51 |
| Jetson TX (USART6_TX) | `PG14` | **CN10-14** | **D1** |
| Jetson RX (USART6_RX) | `PG9` | **CN10-16** | **D0** |

**Besleme ve toprak (morpho):**
`+3.3V` = CN11-16 · `+5V` = CN11-18 · `VIN` = CN11-24 · `E5V` = CN11-6 ·
`U5V` = CN12-8 · `AVDD` = CN12-7 · `AGND` = CN12-32
`GND` = CN11-8, 19, 20, 22, 49, 60 · CN12-9, 20, 39, 54, 63

⚠ Jetson uçları **Zio başlığında (CN10), morpho gerekmiyor** — bu iki satırın
`D` sütunu daha önce boştu ve kablo morphodan çekilmeye çalışılıyordu.

⚠ `PA13`/`PA14` (CN11-13/15) SWD — GPIO yapma, yükleme ölür.
⚠ `PD8`/`PD9` (CN12-10 / CN11-69) ST-LINK sanal COM portu; morphodan kullanmak
için SB5/SB6 OFF, SB4/SB7 ON olmalı.

### Fritzing parçasındaki üç hata (29 Ağu 2026'da düzeltildi)

Forumdan alınan `Nucleo-144-MB1137` parçasında UM1974 ile çelişen üç etiket
bulundu. **Konumlar doğruydu, yalnız isimler yanlıştı** — ama `CN8-2`'ye kablo
çekerken tooltip `PC3` gösterdiği için yanıltıyordu.

| Connector | Yanlış | Doğru | Kaynak |
|---|---|---|---|
| `connector71` — CN8-2 | `SDMMC_D0/PC3` | **`SDMMC_D0/PC8`** | Tablo 19 |
| `connector75` — CN8-6 | `SDMMC_D3/PC10` | **`SDMMC_D2/PC10`** | Tablo 19 |
| `connector67` — CN11-68 | `PG14` | **`PG13`** | Tablo 21 |
| `connector134` — CN12-45 | `PD!!` | **`PD11`** | (daha önce düzeltildi) |

Doğrulama: düzeltmeden önce `PC8` parçada yalnız 1 kez, `PG14` 3 kez geçiyordu;
sonra ikisi de tam 2 kez (Zio + morpho) — tutarlı.

Düzeltilmiş parça: `tasarim/fritzing/Nucleo-144-MB1137-rev2.fzpz`
(Fritzing dosyası bu depoda yayımlanmamıştır).

⚠ **Çizim sürerken parçayı DEĞİŞTİRME** — Fritzing parçayı kendi kütüphanesine
kopyaladığı için değiştirmek çizilmiş telleri götürür. Üçü de sadece etiket;
`Part → Edit (new parts editor)` ile yerinde yeniden adlandırmak da olur.
