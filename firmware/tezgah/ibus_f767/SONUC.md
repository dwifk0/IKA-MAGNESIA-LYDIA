# iBUS tezgah testi — ölçüm sonuçları (30 Ağustos 2026)

Kart: NUCLEO-F767ZI, `UART5_RX = PD2` (CN11-4). Alıcı 5 V + GND + 1 kΩ seri sinyal.
Ölçüm yöntemi: seri kütük → yerel seri kütük dosyası, zaman damgalı çözümleme.

## Kart ve yol doğrulandı

- `f767_buton_led` çalıştı: üç LED + USER butonu. **F767 ilk kez çalıştırıldı.**
- Yükleme yolu: `CN1` USB → `D:` = `NOD_F767ZI`, `.bin` sürükle. `FAIL.TXT` yoksa başarılı.
- Sanal seri port **COM6**. ST-LINK firmware `V2J39M27`.
- Çerçeve kalitesi: **~128 çerçeve/sn**, bozuk oranı **~%0,4** (saniyede ~0,5 çerçeve).
  ⚠ Sıfır değil. Tezgahta jumper + 1 kΩ ile böyle; araçta motor gürültüsü altında
  büyüyüp büyümediğine bakılacak.

## 🔴 İlk arıza: alıcı 3,3 V'a bağlıydı

Belirti: `hz=0`, `saglam=0`, **`bozuk=0`**, `sessiz=9999`.
`bozuk`un da 0 olması ayırt ediciydi — saglama hatası olsa artardı; hiç artmaması
pinde **hiç UART verisi olmadığını** gösterdi. 5 V'a alınınca anında düzeldi.
**Ders:** `saglam=0 && bozuk=0` → besleme/kablo. `saglam=0 && bozuk>0` → baud/gürültü.

## Kanal eşlemesi — ÖLÇÜLDÜ, kapandı

Yöntem: her ekseni tek tek it, 4 sn tut, bırak, 3 sn bekle.
(29 Ağustos'ta "CH1/CH2/CH4 tutarsız" kalan madde bu şekilde kapandı.)

| Kanal | Fiziksel kaynak | İtiş yönü | Boşta | İşlev |
|---|---|---|---|---|
| **ch1** | sağ yatay | sağa → 1987 | 1502 | direksiyon / taret pan |
| **ch2** | sağ dikey | yukarı → 1985 | 1500 | **gaz** (yukarı ileri) |
| **ch3** | sol dikey | yukarı → 1994 | **1504, yaylı** | **fren** |
| **ch4** | sol yatay | sağa → 2000 | 1495 | boş |
| ch5 | **VRA potu** | sürekli | 1000 | — |
| ch6 | **VRB potu** | sürekli | 1000 | — |
| ch7 | **SwA** | — | 2000 | yazılımsal kesme |
| ch8 | **SwB** | — | 2000→**1000** (reverse sonrası) | taret aktif |
| ch9 | **SwC** | **1000/1500/2000 — üç kademe ✅** | 2000→**1000** | mod |
| ch10 | 4. anahtar (SwD) | 1000 ↔ 2000 | 2000 | — |

**29 Ağustos'ta tasarlanan düzen ölçümle bire bir tuttu.** Dört eksenin dördü de
sağa/yukarı gidince büyüyor. `ch5`/`ch6` anahtar değil **pot** — "CH6 = VRB şüpheli"
notu kapandı. `ch3` bırakılınca 1504'e dönüp 40 sn orada kaldı → **yaylı**.

⚠ `ch7`–`ch10` boşta 2000 (ters). SwA'da bu istenen yön (*düşük = kes*);
SwB/SwC reverse'lendi ve **doğrulandı**.

## ✅ Fren yönü kararı — 30 Ağustos 2026

**`ch3` YUKARI = fren uygula, tepede tam fren. `ch3` AŞAĞI = freni geri çek.**
Merkez 1504 = ölü bant, aktüatör durur.

Aktüatörde konum geri beslemesi olmadığı için iki yön de gerekli:
yukarı = ileri sür (fren bas) · aşağı = geri sür (fren çöz).
Firmware eşlemesi çift yönlü: `(ch3 − 1504)` işaretiyle birlikte BTS7960B'nin
`RPWM`/`LPWM`'ine gider, büyüklüğü PWM olur.
⚠ `BRAKE_STALL_MS` her iki yönde de geçerli — uca dayandığında motoru koruyan
tek şey o zaman aşımı (fren hattında akım ölçümü yok).
`ch3` yaylı olduğu için **kol bırakılınca fren kendiliğinden çözülür.**

🔴 **Vericide failsafe kurulurken `ch3` YUKARI DAYALI tutularak kaydedilecek** —
sinyal kaybında fren uygulanmalı.

## 🔴 FAILSAFE — asıl bulgu

**Alıcı, verici kapalıyken de yayın yapıyor.** Verici kapatıldıktan sonra 37 saniye
boyunca `hz` 128'de kaldı, `sessiz` hiç büyümedi.
→ **Firmware sinyal kaybını ASLA anlayamaz.** "Çerçeve gelmiyorsa dur" mantığı
bu araçta hiçbir zaman tetiklenmeyecek ölü koddur. Tek koruma alıcının failsafe kaydı.

### İlk sürümdeki kendi hatam
Teste "değerler donduysa failsafe" diye bir bayrak koymuştum. **Yanlıştı ve testi
sonuçsuz bıraktı:** verici AÇIKKEN de kimse kola dokunmazsa değerler donar.
Doğru yöntem ölçüm düzeninde: **bir kolu merkezden uzak tutarken vericiyi kapat.**
Değer yerinde kalırsa "son değeri tut" (tehlikeli), sıçrarsa failsafe kuruludur.

### Ölçülen failsafe parmak izi (iki bağımsız kütükte aynı)

```
ch1=1502 ch2=1500 ch3=1501 ch4=1500 ch5=1000 ch6=1000
ch7=2000 ch8=2000 ch9=2000 ch10=2000
```

✅ İyi haber: "son değeri tut" DEĞİL — `ch3` 2000'de tutulurken kapatıldı, 1501'e düştü.
En kötü senaryo (araç son gazla devam eder) gerçekleşmiyor.

❌ Ama hedeflenen değerler değil:

| Kanal | Failsafe'te | Hedef | Sonuç |
|---|---|---|---|
| ch2 gaz | 1507 ≈ nötr | 1500 | ✅ gaz kesilir |
| **ch3 fren** | **1501** | fren uygula | ❌ **fren yok** |
| **ch7 kesme** | **2000** | 1000 | ❌ **kesme yok** |

### 🔴 Reverse tek başına yapılınca failsafe ters tarafa düştü

Failsafe değerleri alıcıda **ham çıkış değeri** olarak duruyor. SwB/SwC reverse'lendi
ama failsafe yenilenmedi → ikisi koptu:

| Kanal | Boşta (verici açık) | Failsafe'te | Anlamı |
|---|---|---|---|
| ch8 SwB | 1000 = kapalı | **2000** | sinyal kaybında **taret moduna geçer** |
| ch9 SwC | 1000 = manuel | **2000** | sinyal kaybında **mod değişir** |

Reverse'den önce failsafe 2000 "boşta" demekti, artık "aktif" demek.
**Bu ara durum düzeltme öncesinden daha kötü.** Sıra şart: önce reverse, sonra failsafe.

## Açık kalanlar

- 🔴 **Failsafe kaydı yapılmadı** — menüde tamamlanmamış ya da kaydedilmeden çıkılmış.
  Kanalların karşısında `Off` mu `On` mu yazdığına bakılacak.
- 🔴 **Fren yönü kararı verilmedi:** `ch3` yaylı, merkez 1504. Aşağı mı yukarı mı
  fren uygulayacak? `config.h` ve failsafe kaydı ikisi de buna bağlı.
  (Öneri: aşağı = fren uygula, pedal sezgisi.)
- Bozuk çerçeve oranının araçta ölçülmesi.

## `RC_CH_*` yazılırken

⚠ **İndis tuzağı:** eski `config.h`'de `RC_CH_GAZ 2` = **CH3** (0 tabanlı).
Yeni tanımlar yazılırken taban açıkça belirtilecek, yoksa bir kayma hatası olur.
