# Seri Protokol — F767 ⇄ Jetson

Sürüş kartı (STM32 F767ZI) ile üst bilgisayar (Jetson Orin Nano) arasındaki tüm
trafik, **8 baytlık sabit uzunlukta çerçevelerle** tek bir USB CDC bağlantısı
üzerinden akar. Bu belge o sözleşmenin tamamıdır: iki taraf birbirinin kodunu
görmeden, yalnız buraya bakarak çalışabilir.

> Kartın tam firmware'i yayımlanmamıştır, ama karar katmanının tamamı —
> çerçeveleme, iBUS çözümü, emniyet mandalları, kip hakemi, gaz profili ve jog
> mantığı — platformdan bağımsız modüller hâlinde
> [`firmware/cekirdek/`](../firmware/cekirdek/) altında ve masaüstünde 138
> birim testle doğrulanıyor.

---

## Çerçeve biçimi

Her paket **8 bayt**, sabit:

```
┌──────┬──────┬────────┬────────┬────────┬────────┬──────┬──────┐
│ 0xAA │ tip  │ v0 yük │ v0 düş │ v1 yük │ v1 düş │ XOR  │ 0x55 │
└──────┴──────┴────────┴────────┴────────┴────────┴──────┴──────┘
   0      1       2        3        4        5       6      7
```

- `v0` / `v1` işaretli 16-bit, **big-endian** (yüksek bayt önce)
- Anlamı tamamen `tip` alanına bağlı
- `XOR` = bayt[1] ^ bayt[2] ^ bayt[3] ^ bayt[4] ^ bayt[5]
- Bütünlük üç şartla doğrulanır: baş baytı `0xAA`, bitiş baytı `0x55`, XOR
  tutuyor. Üçünden biri tutmazsa çerçeve **sessizce atılır** ve `0x3C`
  sayacını artırır

Çalışan uygulama ve birim testleri:
[`firmware/cekirdek/cerceve.h`](../firmware/cekirdek/cerceve.h) ·
[`firmware/test/test_cerceve.cpp`](../firmware/test/test_cerceve.cpp)

> **Yarım çerçeve 20 ms'de bırakılır.** Bu olmazsa tek bir kayıp bayt senkronu
> kaydırabilir: toplayıcı bir sonraki `0xAA`'yı arar ve veri baytlarından biri
> `0xAA` ise çerçevenin **ortasından** senkron kurar — hat sessizce ölür, hata
> da vermez. Kaymanın kalıcı olup olmadığını verinin içeriği belirlediği için
> bu zaman aşımı şansa bırakılamaz.

**Sürüm kontrolü:** `0x3B` paketi protokol sürümünü ve firmware yapı numarasını
taşır. Köprü **protokol sürümüne** bakar; yapı numarası yalnız izleme içindir.
Protokolü değiştiren her şey sürümü artırır, artırmayan hiçbir değişiklik
protokolü bozamaz.

---

## Karttan çıkanlar (telemetri)

| Tip | Ad | `v0` | `v1` |
|---|---|---|---|
| `0x30` | Enkoder | sayım yüksek | sayım düşük *(birlikte int32)* |
| `0x31` | Hız | enkoder hızı [mm/s] | gösterge [mHz] |
| `0x32` | IMU açı | yaw ×10 [°] | roll ×10 [°] |
| `0x33` | IMU pitch | pitch ×10 [°] | kalibrasyon baytı *(aşağıda)* |
| `0x34` | E-STOP | basılı (0/1) | uyuşmazlık (0/1) |
| `0x35` | Sağlık | hata bayrakları | çalışma süresi [s] |
| `0x36` | RC | gaz [binde, −1000…1000] | durum bayrakları |
| `0x37` | Sürüş | gaz [mV] | fren [binde, −1000…1000] |
| `0x38` | Jetson yankısı | **anlaşılan** hız [mm/s] | **anlaşılan** açı [1/100°] |
| `0x39` | Mod | mod (0/1/2) | `JDR_*` bayrakları |
| `0x3A` | RC ham | CH1 ham [µs] | CH9 ham [µs] |
| `0x3B` | Sürüm | protokol sürümü | firmware yapı no |
| `0x3C` | Sayaç | alınan paket | bozuk paket *(alt 16 bit)* |
| `0x3D` | Batarya | paket gerilimi [mV] | en düşük hücre [mV] |
| `0x3E` | Ayar | ayar kimliği | kartta duran değer |

### `0x38` neden var

Kart, aldığı sürüş komutunu **anladığı haliyle geri yankılar**. Gönderilen değer
ile yankılanan değer arasındaki fark, ölçek/işaret/kırpma hatalarını daha araç
hareket etmeden ortaya çıkarır. Uzaktan hata ayıklamada en çok işe yarayan tek
paket budur.

### `0x33` kalibrasyon baytı

IMU'nun `CALIB_STAT` baytı **aynen** taşınır:

```
bit 7..6  sys    bit 5..4  gyr    bit 3..2  acc    bit 1..0  mag
```

Her alan 0–3. Ara katmanda yeniden paketlenmez — çipin söylediği neyse o gider.

### `0x3C` iki sayaç birden

Yalnız "bozuk paket" sayısı bir şey anlatmaz. İkisi birlikte okunur:

| Alınan | Bozuk | Anlamı |
|---|---|---|
| sabit | sabit | Hiç veri gelmiyor — kablo veya port |
| sabit | **artıyor** | Baud uyuşmazlığı ya da çerçeve kayması |
| artıyor | sabit | Sağlıklı |

---

## Karta girenler (komut)

| Tip | Ad | `v0` | `v1` |
|---|---|---|---|
| `0x01` | Sürücü | hız [mm/s, işaretli] | direksiyon [1/100°] |
| `0x02` | Acil dur | — | — |
| `0x03` | Lazer | 1 aç / 0 kapa | — |
| `0x04` | Heartbeat | — | — |
| `0x05` | Pan | açı [0…180°] | — |
| `0x06` | Tilt | açı [0…180°] | — |
| `0x07` | E-STOP ilanı | 1 ilan et / 0 kaldır | — |
| `0x08` | Fren | fren [binde 0…1000] | — |
| `0x09` | Ayar yaz | ayar kimliği | ölçekli değer |

**`0x02` yalnız durdurmaz, komutu sıfırlar.** Gaz rölantiye iner, tam fren
uygulanır ve kart **taze bir `0x01` bekler**; eski komut bayatlamış sayılır.
Aynı sıfırlama mod geçişinde de yapılır.

**`0x04` yalnız zaman aşımı sayacını tazeler.** Hiçbir aktüatöre dokunmaz.
Üst bilgisayarın canlı olduğunu söylemenin, yanlışlıkla araca komut vermeden
yapılan tek yolu budur.

---

## Bayraklar

### Hata bayrakları — `0x35` `v0`

| Bit | Ad | Anlamı |
|---|---|---|
| `0x01` | BNO yok | IMU cevap vermiyor |
| `0x02` | BNO kalibrasyon | Kalibrasyon yetersiz |
| `0x04` | Enkoder sessiz | Araç hareketliyken enkoder kımıldamıyor |
| `0x08` | Gösterge sessiz | Hız göstergesi darbe üretmiyor |
| `0x10` | E-STOP uyuşmazlık | İki kanal aynı şeyi söylemiyor |
| `0x20` | RC yok | iBUS çerçevesi gelmiyor — **kopuk kablo dahil** |
| `0x40` | Fren stall | Fren aynı yönde eşik süreyi aştı |
| `0x80` | Gaz yok | Gaz DAC'i cevap vermiyor |

### Durum bayrakları — `0x36` `v1`

| Bit | Ad |
|---|---|
| `0x01` | Kesme (SwA düşük) |
| `0x02` | Taret kipi (SwB yüksek) |
| `0x04` | Geri vites rölesi çekili |
| `0x08` | Ana güç *(donanım söküldü — kalıcı 0)* |
| `0x10` | Yön değiştirme sürüyor, gaz kilitli |
| `0x20` | Aydınlatma rölesi çekili |
| `0x40` | Lazer rölesi çekili |

> `0x08` bitini "ana güç açık" diye okumayın — ilgili donanım araçtan
> söküldü, bit kalıcı olarak sıfır basıyor. Kaldırılmadı, çünkü kaldırmak
> protokol sürümünü artırırdı.

### Jetson durum bayrakları — `0x39` `v1`

| Bit | Ad | Anlamı |
|---|---|---|
| `0x01` | Link | Son paket zaman aşımı penceresi içinde |
| `0x02` | E-STOP | Üst bilgisayar E-STOP ilan etti |
| `0x04` | Dur | Son komut acil dur; taze sürüş komutu bekleniyor |
| `0x08` | Elle kip | Direksiyon tezgâh kipinde |

---

## Ayar kimlikleri — `0x09` / `0x3E`

Kalibrasyon sabitleri firmware'e gömülü değildir; çalışma anında yazılır ve
kalıcı bellekte tutulur. `0x3E` ile kartta **duran** değer geri okunabilir —
yani "yazdım mı, tuttu mu" sorusu tahminle değil ölçümle cevaplanır.

| Kimlik | Ayar | Birim |
|---|---|---|
| 1 | Tekerlek yuvarlanma çevresi | mm ×10 |
| 2 | Dişli oranı (enkoder turu : teker turu) | ×1000 |
| 3 | Direksiyon oranı (kolon° / teker°) | ×1000 |
| 4 | Gösterge darbe/tur | ×10 |
| 5 | Direksiyon işareti | +1 / −1 |
| 6 | Lazer polaritesi | 0 aktif-düşük, 1 aktif-yüksek |
| 7–8 | Taret pan alt / üst sınırı | derece |
| 9–10 | Taret tilt alt / üst sınırı | derece |
| 11 | Direksiyon sınırı (kolon) | derece |
| 12 | Merkeze dönüş hızı | Hz |
| 13–14 | Pan / tilt ekseni ters mi | 0/1 |
| 15 | Taret jog hızı | derece/s |
| 16 | Kumanda üstel eğrisi | 0…100 |
| 17 | IMU otomatik sıfırlama | 0/1 |

> **Bu depoda ölçülmüş değerler yer almaz.** Yukarıdaki tablo hangi sabitin ne
> olduğunu ve nasıl ölçüldüğünü tanımlar; araca ait sayılar araçta durur.
> Ölçüm yöntemleri [`KALIBRASYON.md`](KALIBRASYON.md) altında.
