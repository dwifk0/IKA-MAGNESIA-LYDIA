# Direksiyon enkoderi — sonuç

> **DURUM: ENKODER HENÜZ ALINMADI. Test çalıştırılmadı.**
> Bu dosya doldurulmayı bekliyor. Firmware derlendi (RAM %0,3, Flash %1,1),
> bağlantı tablosu `BAGLANTI.md`'de, pinler `config.h` §7'de ayrıldı.

**İkinci Omron E6B2-CWZ6C, 600 P/R → NUCLEO-F767ZI, YAZILIM kuadratürü
(EXTI7/EXTI8).** Tezgahta, mil elle çevrilerek.

## Altı ölçüt — hepsi geçmeden araca takılmaz

| # | Ölçüt | Nasıl bakılır | Sonuç |
|---|---|---|---|
| 1 | Sayım değişiyor | mili çevir, `sayim` oynasın | ⬜ |
| 2 | **Ters çevirince GERİ gidiyor** | ters yöne çevir, `sayim` azalsın | ⬜ |
| 3 | A kanalı hem HIGH hem LOW | `amask=3` | ⬜ |
| 4 | B kanalı hem HIGH hem LOW | `bmask=3` | ⬜ |
| 5 | **Tur başı sayım = 2400** | `zara` = ±2400, sapmasız | ⬜ |
| 6 | **Yasak geçiş = 0** | `yasak=0` kalmalı | ⬜ |

### 2. ve 4. ölçüt neden kritik

Sponsorun 300B enkoderi **16 Ağustos'ta tam burada elendi**: A kanalı
çalışıyordu, B ölüydü, sayım artıyordu ama yön yoktu. Tek kanala bakan bir
test onu "çalışıyor" diye geçirirdi. Aynı tuzağa iki kez düşülmez.

### 6. ölçüt neden ayrı sayılıyor

Yasak geçiş = iki bitin aynı anda değişmesi = **kaçırılmış kenar**. Sayım bu
sırada "çalışıyor gibi" görünmeye devam eder ama sessizce adım kaybeder. Başka
hiçbir ölçüt bunu yakalamaz. Yazılım kuadratüründe bu sayaç, donanım sayacının
sağladığı güvencenin yerini tutan tek şey.

Beklenen: elle çevirmede **0**. Sıfırdan büyükse sebebi sırayla eleyin —
pull-up eksik/zayıf, kablo uzun ve ekransız, ya da kesme önceliği yetersiz.

## Ölçüm defteri

| Tarih | sayim | zara | amask | bmask | yasak | Not |
|---|---|---|---|---|---|---|
| — | — | — | — | — | — | *doldurulacak* |

## Test sonrası

Altı ölçüt de geçerse:

1. `config.h`'da `DIRENK_VAR` → `true`
2. `DIRENK_TERS` araçta belirlenecek — **direksiyonu SAĞA çevir, `dsayim`
   ARTMALI**. Azalıyorsa `true` yap.
3. Kolon/teker oranı (`OTONOM_DIR_ORAN`) bu enkoderle ölçülebilir hâle gelir:
   mili bilinen bir sayım kadar çevir, tekerin yerdeki açısını ölç, böl.
   Bu sayı yazılım ekibinin de beklediği eksik katsayı.
