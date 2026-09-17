# E6B2-CWZ6C enkoder — sonuç (31 Ağustos 2026)

**Omron E6B2-CWZ6C, 600 P/R, NPN açık kollektör → NUCLEO-F767ZI, TIM4 donanım
sayacı.** Tezgahta, mil elle çevrilerek. Araca **takılı değil** — kaplin ve
braket hâlâ yok.

Bağlantı araçtaki hâliyle kuruldu: **3 × 1 kΩ pull-up 3V3'e**, besleme kartın
5 V'undan, 0 V kartın GND'siyle ortak. Yani bu test yalnız enkoderi değil
`BAGLANTI_HARITASI.md` §"E6B2 kablo" tablosunu da doğruladı.

## ✅ Altı ölçütün altısı da geçti

| # | Ölçüt | Sonuç |
|---|---|---|
| 1 | Sayım değişiyor | ✅ |
| 2 | **Ters çevirince GERİ gidiyor** | ✅ |
| 3 | A kanalı hem HIGH hem LOW görüldü | ✅ maske `3` |
| 4 | B kanalı hem HIGH hem LOW görüldü | ✅ maske `3` |
| 5 | **Tur başı sayım = 2400** | ✅ `zara` = **−2400, sapmasız** |
| 6 | **Gerçek yasak geçiş** | ✅ **0** |

**2. ve 4. ölçüt kritikti:** sponsorun 300B enkoderi 16 Ağustos'ta tam burada
elenmişti — A çalışıyor, B ölü, sayım artıyor ama yön yok. E6B2 ikisinden de
geçti.

**5. ölçüt işi kesinleştirdi.** Tur başı **tam** 2400 çıkması üç şeyi birden
kanıtlıyor: etiketteki 600 P/R doğru, ×4 kuadratür sağlam, Z darbesi temiz
(sahte tetikleme olsaydı Z arası küçük ve rastgele sayılar çıkardı).
Çapraz doğrulama: `sayim / 2400` panodaki `tur` alanıyla birebir tuttu.

## 🔴 "Yasak geçiş" sayısı YANILTICIYDI — ölçüm kusuru, enkoder değil

İlk denemelerde yasak geçiş 900–1100 arası çıkıyordu ve **elle yavaş
çevirirken de artıyordu**, yani "hızlı çevirdin" açıklaması tutmuyordu.

**Sebep:** sağlık örneklemesi 1 kHz'deydi, ama aynı döngü 10 Hz'de seri porta
~120 karakterlik satır basıyor. 115200 baud'da o satır **~11–13 ms** sürüyor ve
o süre boyunca örnekleme **duruyor**. Kör pencerede enkoder 5-6 adım
ilerleyince örnekleyici geri döndüğünde "iki bit birden değişti" görüyor.
Saniyede 10 rapor → saniyede ~10 sahte kayıt.

Ölçülen kör pencere: **`bosluk` = 11 743 → 13 394 µs.** Tahmin değil, sayıldı.

**Düzeltme** (`src/main.cpp`): örnekleme her döngüye alındı, örnekler arası
boşluk ölçülüyor ve iki bit birden değiştiğinde boşluğa bakılıyor:

| Sayaç | Anlamı |
|---|---|
| `hatali` — *Yasak gecis* | boşluk ≤ **250 µs** iken yakalandı → **gerçek** sinyal bütünlüğü sorunu |
| `atlanan` — *Yargilanamaz* | boşluk büyüktü → ölçüm atladı, **enkoder hakkında bir şey söylemez** |
| `bosluk` | görülen en uzun örnekleme arası (µs) |

Düzeltmeden sonra: `hatali` = **0**, `atlanan` = 33. Sinyal temiz.

> **Ders:** sayacın kendisi zaten kanıtı taşıyordu — tur başı tam 2400 çıkıyorsa
> darbe kaybı ya da fazlası **yoktur**. "Yasak geçiş" göstergesi ondan daha
> zayıf bir kanıt; çeliştiklerinde sayaca güvenilir.

## 🔴 Teşhis: üç hat da HIGH'ta takılıydı

İlk denemede sayaç hiç saymadı. Panoya ham seviye alanları eklendi
(`alvl`/`blvl`/`zlvl` ve `amask`/`bmask`) ve tablo tek bakışta okundu:
**üçü de HIGH'ta sabit, maskeler `2`** → pull-up'lar çalışıyor, ama hattı
aşağı çeken yok. Yani sorun kart tarafında değil, enkoder/kablo tarafında.
Kullanıcı bağlantıları düzeltti, sayım hemen geldi.

Bu alanlar sketch'te **kaldı** — bir daha aynı durumda "sinyal yok mu, sayaç mı
saymıyor" sorusu tek bakışta ayrılır.

## ⚠ Bu testin çözMEdiği şeyler

- **Kaplin ve braket yok.** Enkoder hiçbir mile bağlı değil.
- **`ENK_TEKER_CEVRE_MM` = 0.** Dişli oranı ve tekerlek çevresi ölçülmeden
  ana firmware mesafe yayınlamıyor (uydurma odometri yayınlamaktansa susuyor).
  Bu sayı **araçtan** çıkar, tezgahtan değil.
- **Yön işareti araçta belirlenecek.** A/B sırası ileri yöne ters denk gelirse
  odometri geri sayar. `config.h`'de **`ENK_TERS` ayarı YOK** —
  `FREN_TERS`/`DIR_TERS` var ama enkoderde karşılığı eksik. Eklenmezse sahada
  düzeltmenin tek yolu **kabloyu sökmek**.
- **Hız dayanıklılığı denenmedi.** Elle çevirmede en fazla ~1 tur/sn'ye çıkıldı;
  araçta ~53 kHz bekleniyor. 1 kΩ pull-up'lar tam bunun için takılı ama
  yük altında doğrulanmadı.
- **Ekran (shield) topraklaması** tezgahta konu olmadı; araçta tek uçtan
  bağlanacak, çift uçta toprak döngüsü olur.

## Yükleme ve pano

```bash
~/ika/testler/yukle.sh e6b2_f767
python.exe enk_pano.py --port COM6      # http://localhost:8773
```

⚠ Yüklemeden önce panoyu KAPAT — seri portu tutuyor.

Komutlar: `z` sayacı sıfırlar · `h` sağlık sayaçlarını sıfırlar (Z dahil) ·
`i` son Z'den beri geçen sayımı yazdırır.
