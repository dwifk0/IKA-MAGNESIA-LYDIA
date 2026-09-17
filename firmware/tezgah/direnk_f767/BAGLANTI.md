# Direksiyon enkoderi — tezgah bağlantısı

**İkinci E6B2-CWZ6C, 600 P/R.** Traksiyon enkoderiyle aynı model, aynı
elektriksel davranış. Bu test onu **araca takmadan**, mil elle çevrilerek
doğrular.

## Kablo tablosu

| E6B2 teli | Renk | Nereye | Not |
|---|---|---|---|
| A | siyah | **`PE7`** — D41 | + 1 kΩ pull-up → 3V3 |
| B | beyaz | **`PE8`** — D42 | + 1 kΩ pull-up → 3V3 |
| Z | turuncu | **`PF12`** — D8 | + 1 kΩ pull-up → 3V3 · opsiyonel |
| +Vcc | kahverengi | kartın **5 V**'u | araçta UBEC'ten gelecek |
| 0 V | mavi | kartın **GND**'si | ortak |
| ekran | — | **tek uçtan** aynı GND | iki uçta toprak döngüsü olur |

## 🔴 Üç pull-up ŞART — yoksa test hiç saymaz

E6B2 **NPN açık kollektör** çıkışlı: yüksek seviyeyi çıkışın kendisi değil,
pull-up belirler. Bu yüzden 12/24 V'tan beslense bile **seviye çevirici
gerekmiyor** — pull-up 3V3'e gittiği sürece seviyeler doğru.

Dahili pull-up bilerek **kapalı** bırakıldı (`pinMode(..., INPUT)`), iki sebeple:

1. Dahili ~40 kΩ, harici 1 kΩ'a paralel girip kenarı yuvarlardı.
2. Pull-up'ı takmayı unutursanız test **hiç saymaz** ve hatayı ilk saniyede
   görürsünüz. Dahili açık olsaydı zayıf çalışır, sorunu araçta yaşardınız.

⚠ Gerilim çıkışlı (push-pull) bir varyant gelirse bu geçmez, bölücü gerekir.
Gelen modelin NPN açık kollektör olduğu 31 Ağustos'ta tezgahta doğrulanmıştı.

## Pinler neden bunlar

`config.h` §7'deki rezervasyonla **birebir aynı** — bu test yalnız enkoderi
değil, ana firmware'e geçilecek kabloyu da doğruluyor.

- **`PE7`/`PE8` aynı portta ve ardışık bit.** ISR ikisini tek register
  okumasıyla alıyor: `(GPIOE->IDR >> 7) & 0x3`. Ayrı portlara dağılsalardı her
  kenar iki okuma ister, aradaki gecikme hızlı dönüşte durumu bozabilirdi.
- **EXTI7 / EXTI8 / EXTI12 boş.** Hat numarası pin numarasıdır, port değil;
  ileride sahiplenecek olanlar (EXTI11 traksiyon Z, EXTI14/15 E-STOP, EXTI2
  gösterge) bilerek boş bırakıldı.

## Yükleme

```bash
cd ~/ika/testler/direnk_f767
pio run -e nucleo_f767zi -t upload
pio device monitor -b 115200
```

Kart üstünde ST-LINK/V2-1 var, **harici dongle gerekmez**.

Jetson üzerinden çalışacaksanız `~/arac_fw` yoluyla ya da
`/media/lydia/NOD_F767ZI`'ye `.bin` kopyalayarak da yüklenebilir.
