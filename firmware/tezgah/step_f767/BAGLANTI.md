# Direksiyon step motoru — tezgah bağlantısı (NUCLEO-F767ZI)

23IP65-20 (NEMA23, 2,0 Nm, 5,0 A) · DM860H sürücü · FLE57-05SW 1:10 redüktör

> 🔴 **`step_mega`ya göre İKİ şey değişti, ikisi de elektriksel:**
> kart Mega'dan F767'ye geçti **ve** bağlantı **ortak katottan ortak anota**
> döndü. Eski dosyadaki "PUL−/DIR− Arduino GND'ye" satırı **geçersiz.**

---

## 1. Şema — ORTAK ANOT, AKTİF-DÜŞÜK

```
                  ┌──────────────────────────────────────────┐
                  │              DM860H sürücü               │
   +5 V (UBEC) ───┤ PUL+                             A+ ├──── motor SİYAH
   PE9  CN12-52 ──┤ PUL−  (açık drenaj)              A− ├──── motor YEŞİL
                  │                                          │
   +5 V (UBEC) ───┤ DIR+                             B+ ├──── motor KIRMIZI
   PE10 CN12-47 ──┤ DIR−  (açık drenaj)              B− ├──── motor MAVİ
                  │                                          │
      (TAKMA) ────┤ ENA+                            AC ├──── 48 V
      (TAKMA) ────┤ ENA−                            AC ├──── 48 V
                  └──────────────────────────────────────────┘
```

**Neden ortak anot:** F767 **3,3 V** sürüyor, DM860H'nin giriş direnci **5 V**'a
göre boyutlanmış. Ortak katotta opto LED'i yetersiz akım çeker ve sürücü
darbeyi kaçırabilir. Ortak anotta MCU yalnız **0 V'a çekiyor**, opto akımı
5 V'tan geliyor, **3,3 V hiç devreye girmiyor.**
`74HCT244` tam bu yüzden iptal edildi.

⚠ **Açık drenaj şart.** `HIGH` = Hi-Z → opto **kesin** söner. Push-pull
sürersen 3,3 V ile 5 V arasında ters akım yolu açılır.
`PE9`/`PE10` FT (5 V toleranslı), hattın 5 V'a çıkması güvenli.

---

## 2. ⚠ ÜÇ YASAK

1. 🔴 **ORTAK TOPRAK ÇEKME.** Girişler optokuplörlü; opto akımının dönüş yolu
   MCU pininin kendisi. Sürücünün `GND`/`AC` ucu **48 V'un eksisi**, mantık
   toprağı değil — birleştirmek izolasyonu bozar ve 48 V gürültüsünü karta taşır.
2. 🔴 **ENA'yı hiç takma.** Boşta ENA = sürücü açık. Kablo takmak yalnız yeni
   bir arıza yolu açar. Hareket hâlinde direksiyonun serbest kalması,
   tutulan tekerlekten tehlikelidir.
3. 🔴 **48 V'u faz eşlemesini ÖLÇMEDEN verme.** Aşağı bak.

---

## 3. DIP anahtarı — firmware'le AYNI olmak zorunda

| Anahtar | Ne ayarlar | Test için |
|---|---|---|
| SW1–SW3 | Faz akımı | **düşük başla** — `off · on · on` ≈ 3,08 A tepe |
| SW4 | Duruş akımı | **on = Full Current** |
| SW5–SW8 | Mikroadım | **1600 adım/tur** → `on · off · on · on` |

Sketch'te `MIKROADIM = 1600`. DIP başka bir şey diyorsa "2 tur" komutu 2 tur
döndürmez ve bunu sayaçtan **göremezsin** — sayaç yine "tam yerindeyim" der.

⚠ `SW4`'ü yarım akıma alma: direksiyon açık çevrim, duruşta yol kuvveti motoru
bir adım kaydırırsa **referans tamamen kaybolur** ve görecek sensör yok.

1600 × 1:10 = **16.000 darbe / çıkış turu = 44,4 darbe/derece.**

---

## 4. Faz eşlemesi — renk koduna güvenme, ÖLÇ

Multimetre direnç kademesinde:

- siyah ↔ yeşil ≈ **0,5–2 Ω** → A fazı
- kırmızı ↔ mavi ≈ **0,5–2 Ω** → B fazı
- siyah ↔ kırmızı = **sonsuz**

Bir fazın iki ucu ters takılırsa motor sadece ters döner (zararsız).
**İki farklı fazın telleri karışırsa motor titrer/inler ve dönmez.**

---

## 5. Sıra

1. Faz eşlemesini ölç (§4). **48 V yok.**
2. Sinyal kablolarını çek (§1). Ortak toprak **yok**, ENA **yok**.
3. **Mile bant yapıştır ve işaretle.** Testin tamamı buna dayanıyor.
4. Kartı USB'den bağla, sketch'i yükle: `~/ika/testler/yukle.sh step_f767`
5. Panoyu aç: `python.exe step_pano.py --port COM6` → http://localhost:8771
6. **Şimdi 48 V'u ver.** Sürücünün `PWR` LED'i yanmalı.
7. Hız tavanı **1000 Hz**'te başlar. `SAG >` 2 tur.

⚠ 4. adımdan önce 48 V verirsen sürücü tanımsız pin seviyeleri görür.
Sketch açılışta `PUL`/`DIR`'i Hi-Z'ye çeker, ama bu ancak yüklüyse geçerli.

---

## 6. Ne aranıyor

🔴 **PANONUN SAYACI KANIT DEĞİLDİR.** Açık çevrimde sayaç her zaman "tam
yerindeyim" der — **gönderdiği darbeyi** sayar, **dönen mili** değil.
Kanıt mildeki işarettir.

| Adım | Yapılacak | Doğru sonuç |
|---|---|---|
| İlk hayat | `SAG >` 2 tur | Mil dönüyor, ses düzgün, ısınma yok |
| Yön | `< SOL` 2 tur | Ters yöne dönüyor |
| **Adım kaçırma** | `GIT-GEL ×5` | **Banttaki işaret tam başladığı yerde** |
| Hız sınırı | Tavanı 500'er artır, git-gel'i tekrarla | İşaretin kaymaya başladığı hız = **gerçek tavan** |
| Moment | Mili elle tutmayı dene (dikkat: 2 Nm × 10) | Kaçırıyorsa SW1–SW3 akımını yükselt |

**Bu testin asıl çıktısı iki sayı:** adım kaçırmadan çıkılabilen en yüksek
**hız tavanı** ve **ivme**. Firmware'de bugün `2700 Hz` / `6000 Hz/s` yazıyor
ve **ikisi de masa başı tahmini** — buradan çıkanla değişecek.

---

## 7. Ters giderse

| Belirti | Sebep |
|---|---|
| Mil hiç dönmüyor, `PWR` LED yanıyor | ENA takılı olabilir — çıkar · ortak anot ters bağlanmış (`PUL+` 5 V'ta mı?) |
| `PWR` LED hiç yanmıyor | 48 V yok. **Tezgahta paket bağlı değilse sürücü açılmaz** — bunu arıza sanma |
| Titriyor, dönmüyor, inliyor | Faz çiftleri karışmış (A ile B) |
| Dönüyor ama tur tutmuyor | DIP mikroadımı 1600 değil, ya da redüktör 1:10 değil |
| Yüksek hızda duruyor / çığlık | Adım kaçırma — tavanı düşür |
| Sağ komutu sola döndürüyor | **Normal.** Araçta `DIR` seviyesi ters çevrilir |
| Isınıyor (elle tutulamıyor) | Faz akımı fazla — SW1–SW3'ü düşür |
| Rastgele/hayalet adım | Açık drenaj yerine push-pull sürülmüş · ortak toprak çekilmiş |

---

## 8. Bu testin çözMEdiği şey

**Referans (homing) anahtarı hâlâ yok ve listede de yok.** Step açık çevrim,
E6B2 traksiyon milinde — direksiyonda **hiç geri besleme yok.** Açılışta
direksiyonun nerede olduğu bilinmiyor. Bu test yalnız motorun mekaniğini
doğrular.

⚠ **Akımdan stall algılamayı deneme.** Step motor dururken de nominal akımını
çeker; DC motordaki temiz akım sıçraması olmaz. Eski `STEER_AKIM_ESIK`
8 A / 80 ms mantığı step motorda çalışmaz.

⚠ Test bitince **ana firmware'i geri yükle** — bu sketch onun üzerine yazdı.
