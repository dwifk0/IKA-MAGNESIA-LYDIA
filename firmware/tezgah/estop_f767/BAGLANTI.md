# Acil stop → NUCLEO-F767ZI · tezgah testi

Kaynak belge: `genel/ACIL_STOP.md`. Bu, onun tezgah karşılığı.
**Mega sürümünün yerini alır** — o iki kanallı (D2/D3) düzene göreydi,
tasarım 29 Ağustos'ta **tek NC + `PF14`**'e indi.

## Kablo — iki damar, arada eleman yok

| Butonun **NC** bloğu | Nucleo |
|---|---|
| `21` | **GND** — CN8-11 |
| `22` | **`PF14`** — CN12-50 (D4) |

Pull-up kartın içinde (`INPUT_PULLUP`). Direnç/kondansatör/röle **yok**.

⚠ **3V3 tarafına bağlama.** Kablo koptuğunda pin LOW okur = "normal" görünür →
kopan kablo E-STOP'u görünmez yapar. GND tarafında kopuk kablo HIGH okur,
arıza güvenli tarafa düşer.
⚠ **NO bloğunu (`13-14`) kullanma** — mantık ters döner, araç sürekli E-STOP'ta kalır.

## Bağlamadan önce (buton elde, kablo takılı değilken)

| TP | Ölçüm | Beklenen |
|---|---|---|
| TP1 | `21`–`22` direnç, buton **bırakık** | **< 1 Ω** |
| TP2 | `21`–`22` direnç, buton **basılı** | **açık (OL)** |

Yanlışlıkla NO bloğu takılmışsa tam burada yakalanır — ters çıkarsa bağlama.

## Çalıştırma

```bash
cd ~/ika/testler/estop_f767 && ~/.local/bin/pio run
#   -> .pio/build/nucleo_f767zi/firmware.bin  →  NOD_F767ZI diskine surukle
python.exe estop_pano.py --port COM6        # http://localhost:8774
```

Kart üstü LED: **kırmızı** = kesme aktif · **yeşil** = mandal kapalı (normal).

## Sıra — dördü de yapılacak

1. **Kart enerjili, buton bırakık.** `ham`=0, `estop`=0 olmalı.
   🔴 **Burada 1 çıkıyorsa polarite ters ya da NO bloğu takılı** — araç bu
   hâlde açılışta sürekli E-STOP'ta kalır. 24 Ağustos'ta çevrilen polaritenin
   sınavı tam olarak bu adım.
2. **Bas.** `estop` 1, `mandal` 1, kırmızı LED yanar.
3. **Bırak.** `estop` 0'a döner ama **`mandal` 1 KALMALI** — kendiliğinden
   sürmeye dönmemeli. Panodaki "Mandalı aç" ile açılır; buton hâlâ basılıyken
   açmayı denersen firmware reddeder.
4. 🔴 **Soketi çek** (buton bırakıkken). `estop` **1** olmalı.
   Olmuyorsa kopuk kablo görünmez demektir — bu testin en önemli adımı.

## Ölçülecek yeni sayı: kontak zıplaması

`ziplama` = en kötü geçişteki kenar sayısı · `ziplama_ms` = zıplamanın sürdüğü süre.

Şimdiye kadar hiç ölçülmemişti; **100 nF gerçekten gerekli mi** sorusunun cevabı
bu. `ziplama_ms` sürekli **< 10 ms** kalıyorsa 30 ms yazılım filtresi yeter,
kondansatör gereksiz. 10 ms'i aşıyorsa ya filtreyi büyüt ya kondansatör ekle.

## Arıza tablosu

| Belirti | Sebep |
|---|---|
| Buton bırakıkken `estop`=1 | NO bloğu takılı, ya da kablo/soket bağlı değil |
| Basınca hiçbir şey değişmiyor | yanlış pin, ya da iki kablo da aynı uca girmiş |
| `estop` kendi kendine 0↔1 gidiyor | kontak kirli ya da kablo yarım temas |
| Soket çekilince `estop`=0 kalıyor | **kablo 3V3 tarafına bağlanmış** — polariteyi düzelt |
