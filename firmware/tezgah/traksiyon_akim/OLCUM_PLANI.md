# Sürüş motoru akım ölçümü — plan

**Neden:** ana bataryanın BMS'i **60 A**'da keser. Kalkışta ya da rampada bu
aşılıyorsa araç koşu ortasında kendini kapatır ve geri gelmesi BMS'e kalır.
ANL 60 A sigorta bunu yakalamaz — 60 A'yı süresiz taşır, ancak ~2× akımda
atar; yani aşırı yükte **her zaman önce BMS keser**.

**Cevaplanacak tek soru:** hangi manevrada kaç amper çekiyoruz ve **kaç saniye**?

---

## 0. Bedava adım: önce BMS'in kendi ayarını oku

Telefondaki JK uygulamasından, ölçüm yapmadan önce:

- **Deşarj aşırı akım koruma eşiği** (kaç A?)
- **Koruma gecikmesi** (kaç saniye?)
- Varsa **kayıtlı tepe akım** / son koruma olayı

Bu üç sayı olmadan ölçüm yorumlanamaz. Eşik 60 A / gecikme 10 sn ise 1
saniyelik 80 A'lık bir tepe sorun değildir; eşik 60 A / gecikme 0,5 sn ise
sorundur. **En ucuz ve en bilgilendirici adım bu.**

---

## 1. Ne bekliyoruz — kaba hesap

| | |
|---|---|
| Motor etiketi | 1200 W |
| Paket nominal | 51,2 V |
| **Sürekli akım** | 1200 / 51,2 ≈ **23 A** |
| BLDC kontrolcü tepe | tipik 2–3× sürekli → **45–70 A** |

Yani seyir hâlinde sıkıntı yok; risk **yalnız kalkış, rampa ve stall**'da.
Ölçümün amacı da bu üç anı yakalamak.

---

## 2. Ölçüm yolu A — pens ampermetre (hızlı, kaba)

20 dakikada büyüklük mertebesi verir.

- **DC ölçebilen** (Hall etkili) bir pens şart. Yalnız AC ölçen model işe yaramaz.
- **Peak hold / inrush** kipi olmalı; olmayan bir pens saniyede 2–5 kez
  yenilenir ve kalkış tepesini **kaçırır**.
- Pens **tek iletkeni** kavramalı (batarya artısı ya da eksisi). İkisini
  birden kavrarsan toplam sıfır okursun.

Sonuç: "60 A'ya yaklaşıyor muyuz" sorusuna evet/hayır. Süre bilgisi vermez.

---

## 3. Ölçüm yolu B — ACS758 + kayıt (asıl cevap)

Akımın zaman içindeki izini verir; tepe ve süre birlikte görünür.

### Malzeme

| | |
|---|---|
| **ACS758LCB-100B** | ±100 A, çift yönlü (geri vites ve rejeneratif fren için), 20 mV/A, 3 kV izolasyon |
| Bağlantı | Barasından geçmeli tip — 10 mm² kabloya uygun, M4 civatalı |
| Besleme | 5 V (Arduino'dan) |
| Çıkış | analog, boştayken Vcc/2 = 2,5 V |

> **ACS712 kullanma.** Elimizdeki ACS712-30A direksiyon içindir, 30 A tavanı
> var; traksiyon hattında hem doyar hem yanar.

### Nereye takılır

Batarya (XT90) ile motor kontrolcüsü **arasına**, hattın artı ya da eksi
ucundan birine — fark etmez, ikisi de aynı akımı taşır. Böylece sensör
**BMS'in gördüğü akımın aynısını** görür; karşılaştırma anlamlı olur.

```
[Batarya XT90] ──┬── ACS758 ──── [Motor kontrolcüsü]
                 │
              ANL 60 A          (sigorta yerinde kalır)
```

### Firmware'de kritik nokta

Telemetriye **ortalama değil, pencere TEPESİ** yazılmalı. 100 ms'de bir
gönderilen ortalama, 80 A'lık 50 ms'lik bir tepeyi 45 A gibi gösterir —
ölçümün bütün anlamı kaybolur. Hazır betik (`traksiyon_akim.ino`) her
100 ms penceresinde **ortalama, tepe ve dip** üçünü birden veriyor, ayrıca
eşik üstünde geçen toplam süreyi sayıyor.

---

## 4. Ön koşul: gaz DAC'ı

Araç şu an gaz almıyor (§5c) — DAC komut alıyor, 0 V veriyor. Ölçüm için
gaz gerekiyor. İki seçenek:

- **Önce DAC'ı onar** (I²C adres taraması, `testler/i2c_tarama/`), sonra ölç.
- **Ya da DAC'ı atla:** kontrolcünün gaz girişine geçici bir 10 kΩ
  potansiyometre bağla (+5 V / orta uç sinyal / GND). Ölçüm böylece DAC
  arızasından bağımsız yapılır ve elle kademe kademe gaz verilebilir —
  aslında kontrollü test için daha da iyi.

---

## 5. Test protokolü

Her adımda kayıt açık; her adım arasında motorun soğumasını bekle.

| # | Manevra | Bakılan |
|---|---|---|
| 1 | **Teker havada**, gaz %0 → %100 kademeli | boşta akım, kontrolcü limiti |
| 2 | Yerde, düz zeminde **duruştan tam gaz kalkış** | kalkış tepesi ve süresi |
| 3 | Düz zeminde **sabit seyir** | sürekli akım (hesapla tutuyor mu) |
| 4 | **Rampa kalkışı** (yokuşta duruştan) | en kötü hâl, muhtemel tepe burada |
| 5 | **Stall — en çok 2 saniye** (tekerlek engelli, tam gaz) | mutlak tavan |
| 6 | **Geri vites kalkışı** | çift yönlü sensör bunun için |

**Güvenlik**
- 1. adım krikoda, teker havada. Tam gazda araç kaçarsa iş ciddileşir.
- Elin acil stopta bir kişi dursun; yazılım acil stopu değil, **fiziksel** olanı.
- 5. adım motoru ve kontrolcüyü ısıtır — 2 saniyeyi geçme, tekrarlama.
- Sigorta yerinde kalsın. Ölçüm için sigortayı çıkarma.

**Kaydedilecek:** zaman, akım (ort/tepe/dip), paket voltajı, gaz %, hız.
Paket voltajının çökme miktarı bize iç direnci de verir — bedava bilgi.

---

## 6. Sonuca göre ne yapılır

| Bulgu | Yapılacak |
|---|---|
| Tepe < 50 A | Sorun yok, konu kapanır |
| Tepe 50–60 A, kısa | BMS gecikmesine bak; muhtemelen tolere edilir |
| Tepe > 60 A | Sırayla: kontrolcünün **akım limitini** düşür · **yumuşak kalkış** (gaz rampası) firmware'de · sürüş oranını değiştir · son çare daha yüksek akımlı BMS/paket |
| BMS koşuda kesiyor | Yarışma için kabul edilemez — yukarıdakiler zorunlu |

**Not:** yumuşak kalkış zaten firmware'de yapılabilir ve bedavadır. Gaz
komutunu 0'dan hedefe rampa ile götürmek kalkış tepesini belirgin düşürür.
Ölçüm bunun ne kadar gerektiğini söyleyecek.
