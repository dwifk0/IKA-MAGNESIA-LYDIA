# Acil stop (E-STOP) — bağlantı

**Tarih:** 29 Ağustos 2026 · **Şema:** (şema Fritzing dosyasında)
**Durum:** ✅ çizime işlendi.

> **Kural:** Fritzing dosyasını kullanıcı düzenler. Bu belge bağlantı bilgisidir.

---

## Karar

**Kesme butonun kendi kontağında, zaten var.** Mantar buton ana motor
beslemesini doğrudan kesiyor — F767 dona, resetlense ya da USB'si çekilse bile
motor durur. Röle, kontaktör, transistör **eklenmiyor.**

Butona **bir NC kontak bloğu daha** takıldı; o blok yalnız kartın durumu
görmesi için. Mega'daki düzenin aynısı, tek fark pin numarası.

## Bağlantı — iki kablo, arada eleman yok

| Butonun NC bloğu | Nucleo-F767ZI |
|---|---|
| `21` | `GND` — CN8-11 |
| `22` | **`PF14`** · D4 |

`INPUT_PULLUP`. İki uç simetrik. Kablo 2 damar 0,5 mm².
Direnç, kondansatör, röle **yok** — pull-up kartın içinde.

⚠ **3V3 tarafına bağlama.** Kablo koptuğunda pin LOW okur, yani "normal"
görünür → kopan kablo E-STOP'u görünmez yapar. GND tarafında kopuk kablo HIGH
okur, arıza güvenli tarafa düşer.
⚠ **NO bloğu (`13-14`) kullanma** — mantık ters döner, araç sürekli E-STOP'ta kalır.

## Doğruluk tablosu

| Durum | Kontak | `PF14` |
|---|---|---|
| Buton bırakılmış | kapalı | **LOW** |
| Buton basılı (mandallı) | açık | **HIGH** |
| Kablo kopuk / soket çıkmış | açık | **HIGH** ✅ |

## Bağlamadan önce — ölçüm

| TP | Nokta | Beklenen |
|---|---|---|
| TP1 | `21`–`22` direnç, buton bırakık | < 1 Ω |
| TP2 | `21`–`22` direnç, buton basılı | açık (OL) |
| TP3 | `PF14`–GND, kart enerjili, buton bırakık | < 0,4 V |
| TP4 | `PF14`–GND, buton basılı | > 3,0 V |

TP1–TP2 buton elde, kablo takılı değilken — yanlışlıkla NO bloğu takılmışsa
burada yakalanır.

## Firmware

1. 🔴 **`HIGH = basılı`.** Eski Mega kodu (`config.h:187`, `main.cpp:795`)
   `LOW = basılı` bekliyordu; NC bağlanınca araç açılışta sürekli E-STOP'ta
   kalırdı. F767 firmware'i baştan doğru yazılacak.
2. **Mandalla.** Buton bırakılınca kendiliğinden sürme; çıkış açık bir reset
   komutu istesin. 30 ms debounce.
3. **Kesme algılanınca:** DAC → **0,80 V** (0 V verme — kontrolcü "gaz kablosu
   koptu" arızasına düşebilir) · step darbeleri dur · geri vites rölesi düş ·
   fren uygula.

## Bilinen sınır

**Tek kanal:** "kablo koptu" ile "buton basıldı" ayırt edilemiyor. Emniyet
bozulmuyor (ikisi de E-STOP tarafına düşüyor), kaybedilen yalnız arıza teşhisi.
Şartname §6.13 güvenlik donanımının kendi hatasının gözlemlenebilir olmasını
istiyor; sorulursa cevap **"buton kontağı gücü doğrudan kesiyor, pin yalnız
durum okuyor"**. İkinci blok istenirse `PF15` · D2 boş duruyor.

🔴 **Açık:** şartname ayrı bir **donanımsal kablosuz** acil stop istiyor mu?
Şartname okunmadı. Modül ~1.500 TL (`ihtiyac_disi.csv` #35).
**Kargo 6-7 Eylül → karar bu hafta.**

## Malzeme

NC kontak bloğu ×1 (`21-22` baskılı) · 2 damar 0,5 mm² kablo. Hepsi bu.
*(İleride motor gürültüsü sahte tetikleme yaparsa çare `PF14`–GND arası 100 nF —
şimdi takma.)*

---

## 🔴 Firmware'de yakalanan hata — 30 Ağustos 2026

Ana firmware `PF15`'i **ikinci kanal** sayıyordu ve şöyle karar veriyordu:

```c
estop_basili = (a || b);     // a = PF14, b = PF15
```

`PF15` hiçbir yere bağlı değil ve `INPUT_PULLUP` olduğu için **daima HIGH**
okuyor → `b` daima "basılı" → **`PF14` doğru bağlansa bile E-STOP asla
bırakmıyor.**

**Belirti aldatıcıydı:** kumanda geliyordu (iBUS 122 Hz), gaz komutu 0'da
kilitliydi, SSR hiç çekmiyordu. "Kumanda çalışmıyor" gibi görünüyordu; sebep
kumandada değildi.

Kaynağı: iki kanallı mantık **Mega döneminden** (D2/D3) kalmıştı. İKA'da karar
29 Ağustos'ta **tek NC blok + `PF14`** olmuştu ama firmware bunu takip etmemişti.

✅ `ESTOP_B_VAR false` eklendi; tek kanalda uyuşmazlık denetimi de kapalı —
karşılaştıracak ikinci ölçüm yok.

⚠ **Ders:** bağlı olmayan bir pini "ikinci kanal" saymak, olmayan bir emniyeti
var sanmaktan daha kötüdür — kalıcı olarak basılı okur ve aracı çalışmaz hâle
getirir. İkinci kanal gerçekten takılırsa `ESTOP_B_VAR true` yapılacak.

## Tezgahta E-STOP nasıl taklit edilir

Buton bağlı değilken `PF14` boşta kalır, pull-up onu HIGH'a çeker ve firmware
"basılı" okur. **Bu doğru davranış** (yukarıdaki doğruluk tablosu: kopuk kablo
→ HIGH → arıza güvenli tarafa düşer).

Tezgahta sürüşü denemek için **`PF14` (CN12-50) ↔ `GND` arasına jumper** tak.
Bu, NC kontağın kapalı hâlini birebir taklit eder — yani "buton bırakılmış".

⚠ Denemeden sonra **jumper'ı çıkar.** Araçta unutulursa E-STOP'un durum
okuması kalıcı olarak "bırakılmış" der ve butona basıldığı kart tarafından
görülmez. (Motor gücü yine kesilir — o mantar butonun kendi kontağında.)
