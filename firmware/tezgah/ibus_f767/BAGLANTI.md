# FlySky alıcı → NUCLEO-F767ZI · iBUS tezgah testi

Kart üstünde başka hiçbir şey bağlı değil. Üç kablo + bir direnç.

## Kablo

| Alıcı (iBUS/SERVO çıkışı) | Nereye | Not |
|---|---|---|
| **sinyal** (`iBUS` yazan uç) | **1 kΩ seri** → `PD2` = **CN11-4** | 1 kΩ şart değil ama kalsın: kart kapalıyken alıcı pini sürerse koruma diyodu akımını sınırlar |
| **+5V** | `CN8-9` (kartın 5 V'u) | ⚠ **yalnız tezgahta.** Araçta bu uç **UBEC'e** gider — orada kartın 5 V payı yetmez |
| **GND** | `CN8-11` | şart; sinyalin referansı bu |

`PD2` = `UART5_RX` (AF8, `PeripheralPins.c`'den doğrulandı) → **donanım UART**,
yazılım seri yok. `PD2` FT (5 V toleranslı), seviye çevirici gerekmiyor.
iBUS ters değil (SBUS'tan farkı bu), inverter de gerekmiyor.

⚠ Alıcının **iBUS** çıkışı kullanılacak — servo kanallarından biri değil.
FS-iA10B'de ayrı `SENS`/`iBUS` başlığı var.

## Çalıştırma

```bash
# yükleme (pano KAPALIYKEN)
cd ~/ika/testler/ibus_f767 && ~/.local/bin/pio run
#   -> .pio/build/nucleo_f767zi/firmware.bin  dosyasini NOD_F767ZI diskine surukle

# pano
python.exe ibus_pano.py --port COM6     # http://localhost:8776
```

Kart üstü LED'ler panoya bakmadan da söylüyor:
**yeşil** = veri akıyor · **kırmızı** = değerler donmuş (failsafe şüphesi).

## Bu test neyi cevaplıyor

### 1. Hangi kol hangi kanalda — 29 Ağustos'ta kapanmadı
O gece CH1/CH2/CH4'ün hangi kola ait olduğu iki kayıtta **tutarsız** çıktı.
Yöntem: **her kolu tek tek, 3 sn oynat, arada 3 sn dur.** Ham kütüğe
`>>> CH2 oynadi 1102 .. 1876` satırı düşer. Panodaki etiketler *beklentiyi*
yazıyor; ölçülen farklıysa **etiket değil gerçek doğrudur**, `RC_CH_*` ona göre yazılır.

### 2. SwC üç kademe mi
`ch9` → kapalı 2000 · orta 1500 · açık 1000 olmalı. Ara değer varsa anahtar değil pottur.

### 3. Polariteler
- **SwA (CH7) — kesme:** *düşük = kes.* Ters bırakılacak.
- **SwB (CH8) — taret aktif:** *düşük = kapalı* (Reverse).
- **SwC (CH9) — mod:** *düşük = manuel* (Reverse).

⚠ 29 Ağustos ölçümü: **CH7–CH10 vericide ters geliyor** (kapalı=2000, açık=1000).
Vericide düzeltilecek olan bu; test bunu görmek için var.

### 4. 🔴 FAILSAFE — asıl mesele
**Alıcı, verici kapalıyken de yayın yapıyor** (29 Ağu'da ölçüldü). Yani
firmware'in "çerçeve gelmiyor" koruması **hiçbir zaman tetiklenmez**; gelen şey
alıcının failsafe değerleridir. Bu yüzden test çerçeve yokluğuna değil
**değerlerin donmasına** bakıyor (`DONUK`).

**Deneme:** veri akarken **vericiyi kapat.**
- `sessiz` küçük kalmalı (çerçeveler gelmeye devam eder)
- `DONUK` **1** olmalı
- Kanallar hangi değere düşüyor → **failsafe ayarı budur**

Hedef ayar: **`CH7`=1000 (kes) · `CH2`=1500 (gaz nötr) · `CH3`=fren uygula.**
⚠ Eski `CH3=1500` ayarı **geçersiz** — o dönemde gaz CH3'teydi, artık CH2'de.
Ölçülen değerler bunlardan farklıysa vericide failsafe yeniden kurulacak.

### 5. Hat seviyesi — 29 Ağustos'tan kalan soru
O gece A0'dan 3,86 V / 3,39 V okunmuştu, "5 V değil" kesindi ama net değildi.
Artık gerek yok: `PD2` FT, doğrudan bağlanıyor. Merak edilirse multimetreyle
sinyal ucu–GND arası ölçülür; **çizim kararı bundan etkilenmiyor.**

## Arıza tablosu

| Belirti | Muhtemel sebep |
|---|---|
| `hz`=0, `sessiz` büyüyor | sinyal ucu yanlış (servo kanalına takılmış), GND yok, ya da alıcı beslenmiyor |
| `bozuk` artıyor, `saglam` az | baud/gürültü — kablo uzun ya da GND kötü |
| Bütün kanallar 0 | çerçeve hiç tamamlanmıyor; RX pini yanlış (`PD2` = CN11-4) |
| `hz` iyi ama kanallar hiç oynamıyor | verici bağlı değil (bind), ya da zaten failsafe'te |
| Boşta ~30 kenar/300 ms tarzı çöp | pin boşta, 50 Hz şebeke uğultusu — 29 Ağu'da tam bu yaşandı |
