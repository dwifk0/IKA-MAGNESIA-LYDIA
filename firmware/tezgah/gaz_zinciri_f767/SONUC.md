# Uçtan uca gaz zinciri — sonuç (30 Ağustos 2026)

**FlySky iBUS → NUCLEO-F767ZI → MCP4725 → gaz voltajı.**
İlk kez zincirin ortası birleştirildi. `VOUT` kontrolcüye bağlı değil, multimetreyle ölçüldü.

## ✅ Zincir ayakta

| Ne | Ölçülen |
|---|---|
| Stick merkezde | 0,76 V (rölanti) |
| Stick tepede, %45 sınırı | **2,30 V** |
| Stick tepede, %100 sınırı | **4,25 V** |
| **SwA kesme, stick tepedeyken** | **anında rölantiye düştü** ✅ |
| Çerçeve akışı | `hz` 125–128, bozuk ~%0,2 |
| I²C | `i2c_hata = 0` |

**Kumandadan gelen değer DAC'a kadar gidiyor ve emniyet kesmesi çalışıyor.**
Bu, sürüş zincirinin ilk uçtan uca doğrulaması.

## ✅ Hedef 4,20 → 4,10 V'a çekildi (30 Ağu, ana firmware'de uygulandı)

Tam gazda **4,25 V** ölçüldü, hedef 4,20'ydi. Kontrolcünün üst sınırı 4,20 V ve
üstü **"gaz sensörü aralık dışı" arızası** tetikleyebilir.

`V_TAM` = **4,10 V** yapıldı: +50 mV sapmayla bile 4,15'te kalır, %98 gaz eder,
pratikte kayıp yok.

✅ `F767_FIRMWARE/src/config.h` → `GAZ_V_TAM 4.10f`.
Güç sınırı %45'teyken tavan hiç kullanılmadığı için bu madde uykudaydı;
**güç sınırı %100'e çıkarılınca canlandı** ve o sırada uygulandı.

## 🔴 VDD seans içinde 4,42 → 4,97 V'a kaydı — zaafın canlı kanıtı

Aynı `ham` değeri (741) sabah **0,80 V**, akşam **0,90 V** verdi. Kart değişmedi,
kod değişmedi — **besleme değişti.** Röle modülünün `VCC`'si 5 V'a alınınca ray
düzeldi ve gaz onunla birlikte kaydı.

MCP4725'in referansı kendi beslemesidir; bu yüzden 5 V rayındaki her değişiklik
doğrudan gaz voltajına yansıyor. Sabah teorik olarak yazılan madde gün içinde
kendiliğinden gerçekleşti. **AD5693R'ın dahili 2,5 V referansı tam bunu
ortadan kaldırıyor** — geldiğinde gaz hattı besleme oynamalarından bağımsız olur.

⚠ Ders: MCP4725 ile devam edilirse **kalibrasyon 5 V rayına bağlıdır** ve
ray her değiştiğinde (yeni tüketici, farklı kaynak, UBEC'e geçiş) yeniden
ölçülmelidir. Tek seferlik bir iş değil.

## NİHAİ kalibrasyon — VDD = 4,97 V

| | Volt | ham | Ölçülen |
|---|---|---|---|
| Rölanti | 0,80 | **659** | **0,80–0,81** ✅ |
| %30 güç | 1,82 | 1500 | — |
| %45 güç (stick tepesi) | 2,33 | 1920 | **2,30** ✅ |
| Tam gaz | 4,20 | 3461 | — |
| Çip tavanı | 4,97 | 4095 | — |

## Eski ölçüm: VDD = 4,42 V

Modülün besleme ucu ↔ GND. Kalibrasyon tabanı buna göre kuruldu:
rölanti **741** · %45 tepe **2159** · tam gaz **3891**. Çip tavanı 4,42 V,
tam gaza payı 0,22 V.

## ⚠ Milivolt kovalamayın — ölçüm aletinin sınırındayız

Ara turlarda "alt uçta ofset var", "5 V rayı sarktı" diye iki çıkarım yapıldı,
**ikisi de sonraki ölçümle çürüdü.** Sapmalar zıt yönlerde ve 25–40 mV
mertebesinde; tipik bir el multimetresinin toleransı ±(%0,5 + 2 hane) = bu
aralıkta ±25–30 mV. Yani gürültüye eğri uyduruluyordu.

**Pratik ölçüt:** 40 mV = **%1,2 gaz** ≈ 0,04 m/s. Kararları etkilemiyor.
Rölanti hareket eşiği 0,96 V'un çok altında, tavan da yerinde.
Daha ince kalibrasyon isteniyorsa el multimetresi değil, bilinen referanslı
bir ölçüm gerekir.

## ✅ SSR-100 DD ile donanımsal güç kesme — çalıştı (30 Ağu)

Kullanıcının kararı: kumandadan ana gücü kesmek. Riskler bir kez söylendi
(aşağıda), karar tekrarlandı, kuruldu ve **çalıştı.**

**Sürme:** `PE15` (CN12-53, D37) → SSR girişi. `PF15`'e yanlış bağlanmıştı,
düzeltilince çalıştı.
**Kumanda: SwA / CH7.** Tek kesme anahtarı iki katmanı birden yapıyor:
gazı rölantiye çeker **ve** SSR'ı bırakıp hattı keser.
Doğrulandı: `ch7=1000` → `kes=1` → `ssr=0`.

**Aktif-yüksek seçildi:** MCU ölür/reset atarsa pin Hi-Z olur, SSR bırakır,
güç kesilir — arıza güvenli tarafa düşer. Bedeli: her yükleme ve her reset
gücü kesiyor.

### ⚠ Traksiyon hattına takılırken geçerli kalan üç madde
1. **Gerilim payı 1,6 V.** SSR sınırı 60 V, 16S LiFePO4 tam şarjda **58,4 V**.
   60 A'i mikrosaniyede kesmek kablo endüktansında sıçrama üretir.
   **RC snubber şart:** SSR uçlarına 100 nF/100 V + 47 Ω. Kablo kısa ve kalın.
   TVS ile korunamıyor — 58,4 V'ta iletmeyip 60 V altında kırpan parça yok.
2. **Isı:** 60 A × ~1,5 V ≈ **90 W**. Kanatlı soğutucu gerekiyor.
3. **Yarı iletkenler kapalı arızalanır.** Mantar butonun mekanik kontağı
   seri kaldığı için emniyet gerilemesi yok — ama SSR yapışırsa yazılımsal
   kesme sessizce çalışmaz hâle gelir. Periyodik doğrulanmalı.

**Riski sıfırlayan alternatif duruyor:** traksiyon hattı yerine kontrolcünün
**kontak (ignition) hattını** kesmek. Aynı davranış, miliamper akım,
üç maddenin üçü de ortadan kalkar.

## 🔴 iBUS iki kez tamamen sustu — UART gözcüsü eklendi

Belirti: `bayt=0`, tek bayt bile gelmiyor. **Verici kapalı olsa bile alıcı
failsafe çerçevesi basar** (bugün kanıtlandı), yani `bayt=0` normal bir durum
değil. İki kez oldu, ikisinde de **yalnız yeniden yükleme çözdü.**

Bu, STM32 UART'ının taşma/çerçeve hatasını mandallayıp almayı durdurmasına
uyuyor: bayrak temizlenmedikçe bir daha bayt gelmez, reset temizler.
`bozuk` oranının yüksek olması (röle bağlandıktan sonra %2) aynı yöne işaret ediyor.

**Araçta bu, kumandanın sessizce ölmesi demek** — firmware çalışmaya devam eder,
"çerçeve gelmiyor" korumasına düşer, araç durur, sebebi anlaşılmaz.

✅ **Gözcü eklendi:** 1 sn hiç bayt gelmezse UART yeniden kuruluyor,
`uart_yeniden` sayacı telemetride. Kök neden kanıtlanmadı — sayaç artmaya
başlarsa teşhis doğrulanmış olur.

## ✅ ÇÖZÜLDÜ (30 Ağu): verici kapanınca SSR düşmüyordu

> **Kapanış:** vericide failsafe kaydı yapıldı, **doğrulandı — verici kapatılınca
> ana güç kesiliyor.** Teşhis birebir tuttu: kodda değişiklik yok, eksik olan
> tek şey kayıttı. Aşağısı teşhis kaydı olarak duruyor.

**Belirti:** SwA ile kesme çalışıyor (`ch7=1000` → `kes=1` → `ssr=0`, doğrulandı),
ama **vericiyi kapatınca ana güç kesilmiyor** — SSR açık kalıyor.

**Sebep — firmware kusuru değil.** Mantık zaten doğru:

```
ssr_istek = ssr_elle || (!kesme && !cerceve_yok)
```

İki koruma da bu senaryoda tetiklenmiyor:

| Koruma | Neden tetiklenmiyor |
|---|---|
| `cerceve_yok` | **Alıcı, verici kapalıyken de yayın yapıyor** (ölçüldü: 37 sn boyunca `hz`=128, `sessiz` hiç büyümedi). Bu araçta ölü kod |
| `kesme` (`ch7 < 1500`) | Failsafe'te `ch7 = **2000**` ölçüldü — yani "kesme yok". Vericide failsafe kaydı **hiç yapılmadı**, alıcı `ch7` için ham 2000'i saklıyor |

**Çözüm vericide, kodda değil:** `Functions setup → RX setup → Failsafe`'te
**`CH7 = 1000`** kaydedilecek. O an SSR yolu kendiliğinden kapanır — tek bir
kayıt hem gazı rölantiye çeker hem ana gücü keser, kod değişmeden.

🔴 **28 Ağustos'ta yazılan failsafe tablosu bu ağırlığı taşımıyordu.** O gün
`CH7` yalnız *yazılımsal* kesmeydi; 30 Ağustos'ta SSR SwD'den SwA/CH7'ye alınınca
aynı kanal **ana güç kesmesi** oldu. Yani `CH7` failsafe kaydı artık
"iyi olur" değil, **sinyal kaybında aracın gücünü kesen tek mekanizma.**

⚠ **Sıra şart:** önce reverse, sonra failsafe. Failsafe değerleri alıcıda
*ham çıkış* olarak saklanıyor; reverse sonradan yapılırsa kayıt ters tarafa düşer
(SwB/SwC'de bu bir kez yaşandı — `ibus_f767/SONUC.md`).

⚠ **Kayıttan sonra tezgahta doğrulanacak:** kart enerjili, SSR sürülüyorken
vericiyi kapat → `ANA GUC -> KESILDI` satırı düşmeli ve `ssr=0` olmalı.
Doğrulanmadan traksiyon hattına takılmaz.

⚠ **Kapsam sınırı değişmedi:** bu hâlâ yazılımsal bir kesme. Firmware donarsa
ya da UART mandallanırsa (aşağıdaki gözcü maddesi) çalışmaz; SSR aktif-yüksek
sürüldüğü için MCU'nun tamamen ölmesi güvenli tarafa düşer, **donması düşmez.**

## Sırada

- [x] ✅ **Vericide `CH7 = 1000` failsafe kaydı** — yapıldı, verici kapatılarak doğrulandı (30 Ağu)
- [ ] SwB (taret) açılınca gaz rölantiye düşüyor mu — tasarım kuralı, denenmedi
- [ ] `V_TAM` 4,10'a çekilecek
- [ ] 3,24 V'un üstünde araç gerçekten hızlanıyor mu (25–30 m'lik geçişler)
- [ ] Firmware'de VDD sabiti yerine iki noktalı kalibrasyon (istenirse)
