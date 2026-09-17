# Fren aktüatörü + BTS7960B · tezgah bağlantısı (NUCLEO-F767ZI)

`fren_mega`nın F767 portu. 30 Ağustos 2026'da Mega tasarımdan çıktı.
**Ölçümler aynı, kart ve pinler değişti.** Araca takmadan, masada.

---

## ⚠ Dört kural — bağlamadan önce

1. **`B+` 12 V hattına gider.** Modülün tavanı ~27 V.
   **48 V'a bağlarsan modül anında ölür.** (Araçta bu hat 4S LiPo.)
2. 🔴 **`RPWM`, `LPWM`, `EN` — üçüne de 10 kΩ pull-down ŞART.**
   Mega'da bu bir iyi alışkanlıktı, **F767'de zorunlu**: STM32 reset'ten
   `setup()`a kadar pinleri **giriş-yüzer** bırakır. O aralıkta köprüyü kapalı
   tutan tek şey bu dirençler. Yoksa her reset/yükleme aktüatörü seyirtebilir.
3. **`B−` ile kartın `GND`si ORTAK olmalı** — yoksa PWM'in referansı olmaz.
4. **Aktüatörü serbest bırak** — bir yere bağlıysa uca dayandığında ne
   olduğunu göremezsin, üstelik bağlı olduğu şeyi kırar.

> `D12 → GND` jumper'ı bu kartta konu değil (o Mega'nın pini). Ama eski
> belgelerde geçiyorsa: **takılmayacak**, 24 Ağustos'ta söküldü.

---

## 1. Şema

```
    NUCLEO-F767ZI                  BTS7960B                    Aktüatör
    ┌───────────────┐           ┌──────────────┐              ┌────────┐
    │ PC6  CN12-4   ├───────────┤ RPWM     M+ ├──────────────┤ +      │
    │ PC7  CN12-19  ├───────────┤ LPWM     M− ├──────────────┤ −      │
    │ PC8  CN12-2   ├────┬──────┤ R_EN         │              └────────┘
    │               │    └──────┤ L_EN         │
    │ +5V  CN11-18  ├───────────┤ VCC          │
    │ GND  CN11-19  ├───────────┤ GND          │
    │               │           │              │
    │ A0/PA3        ├───────────┤ R_IS  ┐      │   ← OPSİYONEL (akım okuma)
    │ A1/PC0        ├───────────┤ L_IS  ┘      │
    └───────────────┘           │              │
       ⚠ üç girişte de          │ B+  ├────────── 12 V (+)   ⚠ 48 V DEĞİL
         10k pull-down          │ B−  ├────────── 12 V (−) ── kart GND ile ortak
                                └──────────────┘
```

| Sinyal | Pin | Morpho | Dxx | Zamanlayıcı |
|---|---|---|---|---|
| `RPWM` — "fren UYGULA" | `PC6` | **CN12-4** | D16 | TIM3_CH1 |
| `LPWM` — "fren SERBEST" | `PC7` | **CN12-19** | D21 | TIM3_CH2 |
| `R_EN`+`L_EN` köprülü | `PC8` | **CN12-2** | D43 | (düz GPIO) |
| `R_IS` (ops.) | `PA3` = `A0` | CN9-1 | A0 | ADC123_IN3 |
| `L_IS` (ops.) | `PC0` = `A1` | CN9-3 | A1 | ADC123_IN10 |

✅ **Zamanlayıcı çakışması yok:** `PC6`/`PC7` aynı TIM3'ün iki ayrı kanalı
(çekirdeğin pin haritasından doğrulandı). TIM3, gösterge hattı (`PB4`)
30 Ağustos'ta tasarımdan çıkınca boşalmıştı.

⚠ `R_EN`/`L_EN` **tek pine köprülenir** — köprüyü topluca açıp kapatmak için.

---

## 2. 🔴 F767'ye özel soru: girişler 3,3 V ile sürülüyor

Mega'da bu girişler **5 V** ile sürülüyordu; F767'nin çıkışı **3,3 V**.
BTS7960B'nin giriş eşiği `VCC`ye göre tanımlı ve `VCC` 5 V. Bu, tasarımdaki
**tek doğrulanmamış madde** olarak duruyordu (`CIZIM_YAPILACAKLAR.md` §2).
Bu test onu da yokluyor.

| Belirti | Yorum |
|---|---|
| Normal döner, `PWM_MIN` makul (~20–60) | ✅ 3,3 V yetiyor, konu kapanır |
| Hiç dönmez, tavana kadar çıkar | ❌ eşik aşılmıyor |
| Döner ama zayıf/kararsız; `PWM_MIN` beklenmedik yüksek | ❌ eşiğin sınırındayız — **en tehlikeli hâli**, tezgahta çalışıp araçta bırakır |

**Çıkmazsa iki yol var** (ikisi de bu testten sonra karara bağlanır):
- **74HCT244 / 74HCT125**, 5 V beslemeli tampon. Çip 30 Ağustos'ta *direksiyon
  için* iptal edilmişti (DM860H ortak anot çözdü) — **fren için geri gelebilir.**
  3 hat yeter, tek çip fazlasıyla karşılar.
- Ayrık **N-MOSFET seviye çevirici ×3** (2N7002 + 10k). Daha çok lehim,
  daha az parça bekleme.

⚠ Karar bu ölçümden önce verilmeyecek. Muhtemelen çalışır — ama "muhtemelen"
fren hattında yeterli değil.

---

## 3. Akım okuma (opsiyonel ama denemeye değer)

`R_IS`/`L_IS` uçları akımla orantılı gerilim verir (modülde genelde 1 kΩ ile
GND'ye çekili). Bağlarsan pano akımı canlı gösterir ve **uca dayanma anı
akımdaki sıçramadan görülür** — ACS712'nin çıkarılmasıyla kaybedilen şey
ek donanım almadan geri gelebilir.

**⚠ Bağlamadan önce gerilimi ölç.** F767'nin ADC girişi 5 V toleranslı
*değildir* — analog kipte pin `VDDA`yı (3,3 V) aşamaz.

Kaba hesap: BTN7960'ın akım oranı ~1/8500, modülde 1 kΩ →
**~0,118 V/A** → 3,3 V'a **~28 A**'de ulaşılır. 12 V lineer aktüatör
çalışırken birkaç amper, stall'da ~10–15 A çeker → beklenen **1–2 V**,
pay geniş. Yine de:

- Önce multimetreyle `IS` ucunu ölç (tam güçte, stall dâhil).
- **3 V'u geçiyorsa** araya 2:1 bölücü koy ve panodaki mV'yi ikiyle çarp.
- Modülde 1 kΩ yoksa (bazı klonlarda yok) uç **doğrudan 5 V'a fırlar** — o
  hâlde bağlama, önce 1 kΩ'u kendin ekle.

Pano hem ham ADC'yi (0–4095, 12 bit) hem **mV**'yi gösterir.

---

## 4. Üç ölçüm — bu sırayla

### Ölçüm 1: yön (`FREN_TERS`)

`FREN UYGULA` bas. Aktüatör **uzuyor mu kısalıyor mu**, ve bu frene **basmak**
mı **bırakmak** mı?

- Doğruysa: bir şey yapma.
- Tersse: `FREN_TERS çevir` düğmesine bas, tekrar dene.

⚠ Bu yanlış kalırsa araçta "fren uygula" komutu freni **bırakır**. Otonom
sürüşte bunun anlamı açık.

⚠ Kumanda eşlemesi 30 Ağustos'ta kesinleşti: **`ch3` YUKARI = fren uygula.**
`FREN_TERS`, o komutun köprüde hangi yöne çıktığını ayarlar.

### Ölçüm 2: `BRAKE_PWM_MIN`

`PWM_MIN ara` düğmesine bas. PWM sıfırdan başlayıp her 250 ms'de 1 artar.
**Aktüatör ilk kımıldadığı anda `KIMILDADI`'ya bas.**

Config'de bugün `BRAKE_PWM_MIN 0` yazılı. Sıfır kalırsa küçük komutlar
motoru döndürmez, sadece **ısıtır** — akım akar, hareket olmaz.

Bulunan sayıya biraz pay bırakıp `config.h`'ye yaz.

### Ölçüm 3: tam stroke süresi

`Stroke ölç` düğmesine bas — tam güçte uygula yönünde gider, sayaç işler.
**Uca dayanıp durduğunda `DAYANDI`'ya bas.**

```
BRAKE_STALL_MS  >  tam stroke süresi
```

Bugün 2500 ms yazılı ve **tahmin**. Kısaysa fren yolun ortasında kesilir
(frene tam basılmaz). Uzunsa aktüatör uca dayanmış hâlde boşuna zorlanır.
Pano ölçtüğü sürenin ~1,4 katını öneriyor.

Akım okumayı açtıysan **akım eğrisine de bak**: dayanma anında sıçrama
görüyorsan zaman aşımı yerine akım eşiğiyle koruma kurulabilir.

---

## 5. Tezgah koruması — Mega sürümünde yoktu

Sketch'te `KORUMA_MS = 8000`: hiçbir komut köprüyü 8 saniyeden uzun süremez,
sonra köprü kesilir ve seri porta uyarı düşer. Amaç **gözetimsiz kalan
aktüatörü uca dayanmış hâlde bırakmamak** (tarayıcı sekmesi kapanır,
`DAYANDI`'ya basılmaz…).

- `PWM_MIN` araması **hariç tutuldu** — orada sürüş zaten 250 ms'de bir
  yenileniyor ve akım kademe kademe artıyor.
- 8 sn, beklenen stroke süresinin kat kat üstünde; **ölçümü kesmez.**
  Kesiyorsa aktüatör bu testte gerçekten çok yavaş demektir, o da bir bulgudur.

---

## 6. Ne aranıyor — özet

| # | Ölçüm | Düğme | Çıktı nereye gider |
|---|---|---|---|
| 1 | Yön | `FREN UYGULA` → `FREN_TERS çevir` | `config.h: FREN_TERS` |
| 2 | En düşük PWM | `PWM_MIN ara` → `KIMILDADI` | `config.h: BRAKE_PWM_MIN` |
| 3 | Stroke süresi | `Stroke ölç` → `DAYANDI` | `config.h: BRAKE_STALL_MS` |
| 4 | 3,3 V mantık yetiyor mu | (1–3'ün yan ürünü) | 74HCT244 alınacak mı |
| + | Akımdan stall | `Akım okumayı aç` | yeni koruma mantığı (varsa) |

Ayrıca kısmi güçle dene: `FREN UYGULA 300`, `500`, `700`. Aktüatörün
kademeli davranıp davranmadığı görülür — araçta fren binde cinsinden
komut ediliyor.

---

## 7. Yükleme ve pano

```bash
cd ~/ika/testler/fren_f767
~/.local/bin/pio run                                # derle
~/ika/testler/yukle.sh              # <- WSL'de calisan tek yol    # yukle (D: = NOD_F767ZI)
# 🔴 `pio run -t upload` WSL'den calismaz: USB gecisi yok, ST-LINK gorunmez.

python.exe fren_pano.py --port COM6      # http://localhost:8775
```

⚠ **Yüklemeden önce panoyu KAPAT** — seri portu tutuyor.

---

## 8. "DUR" ne yapar

Köprüyü keser. Aktüatör **vidalıdır**, sürüş kesilince **bulunduğu konumu
korur** — geri yaylanmaz. Bu yüzden araç firmware'inde `0` komutu "bulunduğu
yerde kal" anlamına geliyor, "serbest bırak" değil.

Freni gerçekten bırakmak için `SERBEST BIRAK` gerekir.

---

## 9. Ters giderse

| Belirti | Sebep |
|---|---|
| Hiç hareket yok, tavana kadar çıktı | `B+` beslemesi yok · `EN` LOW kalmış · `B−`/GND ortak değil · **3,3 V eşiği aşmıyor (§2)** |
| Isınıyor ama dönmüyor | PWM `BRAKE_PWM_MIN`'in altında — ölçüm 2 tam bunu buluyor |
| Tek yöne gidiyor, geri gelmiyor | Bir PWM kanalı bağlı değil · modülün bir yarısı yanmış |
| Modül ısınıyor / duman | **İki kanal birden yüksek** ya da `B+` 48 V'a bağlı — derhal kes |
| Reset/yükleme anında aktüatör seyiriyor | **Pull-down yok** (kural 2) |
| Akım hep 0 | `IS` uçları bağlı değil, ya da `Akım okumayı aç` basılmamış |
| Akım hep tavanda (4095) | `IS` ucu 3,3 V'u aşıyor — bölücü şart (§3) · modülde 1 kΩ yok |

---

## 10. Bu testin çözMEdiği şey

**Fren aktüatörünün araçtaki yükü tezgahtakinden farklıdır.** Masada boşta
çalışan bir aktüatör, fren pabucuna bastığında çok daha fazla akım çeker ve
stroke süresi kısalır (uca daha erken dayanır).

Yani buradan çıkan `BRAKE_STALL_MS` bir **üst sınırdır**, araçta tekrar
doğrulanacak. `BRAKE_PWM_MIN` ise yük altında **yükselir** — masada kımıldatan
PWM, frene basarken kımıldatmayabilir.

İkisi de Manisa'da tekrar bakılacak; bu test o ölçümleri sıfırdan yapmak
yerine **doğrulanacak bir başlangıç değeri** veriyor.

⚠ §2'nin (3,3 V mantık) **yük altında tekrar bakılması şart** — eşiğin
sınırındaysak boştaki aktüatör döner, yüklü aktüatör dönmez.
