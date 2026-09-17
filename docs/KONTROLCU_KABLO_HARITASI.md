# Ana motor kontrolcüsü — kablo haritası

**Kontrolcü:** 48V–60V 1200W akıllı sürücü (Pilmak, 4.427,50 TL).
Bu sınıftaki beyinler Çin standardı BLDC kalıplarına göre üretiliyor; marka
(Pilmak / Greentime / Brainpower) fark etmeksizin kablo renkleri ve işlevleri
büyük ölçüde aynı.

> ## ⚠ DOĞRULAMA DURUMU
> Aşağıdaki tablo **iki genel kaynaktan** derlendi (letrigo.com kablo rehberi +
> bir montaj videosunun dökümü), **araçtaki kontrolcü üzerinde ölçülerek
> doğrulanmadı.** Renkler bu ailede genelde tutuyor ama **soket içi pin sırası
> üreticiye göre değişiyor.** Herhangi bir uca güç vermeden önce multimetreyle
> teyit et. Bir kablo bağlarken bu belgeye güvenip ölçüm atlama.

## Kablo tablosu

| Grup | Kablo | İşlev |
|---|---|---|
| **Ana güç** | Kalın kırmızı | Batarya (+) |
| | Kalın siyah | Batarya (−) / şase |
| | **İnce kırmızı** | **Kontak / ignition.** Ana (+) buraya köprülenince beyin uyanır |
| **Motor fazları** | Kalın sarı / yeşil / mavi | U / V / W fazı — motordaki aynı renklerle birebir |
| **Hall sensör** (5–6'lı geniş soket) | ince kırmızı | sensör +5 V |
| | ince siyah | sensör GND |
| | ince sarı / yeşil / mavi | Hall A / B / C |
| **Gaz** (3'lü soket) | ince kırmızı | +5 V |
| | ince siyah | GND |
| | ince yeşil (bazen beyaz/mavi) | **Gaz sinyali, 1–4,2 V** |
| **Self-learning** | **iki tekli BEYAZ** | Birbirine takılınca faz/yön öğrenme. **Normalde AÇIK kalmalı** |
| **Fren — düşük seviye** | siyah + mor/gri ikili soket | **Birbirine temas edince** motoru keser (kuru kontak) |
| **Fren — yüksek seviye** | tekli mor | **+12 V** görünce keser (fren lambasından) |
| **Gösterge (kilometre)** | tekli mor/pembe | Hıza orantılı **frekans** çıkışı |
| **Geri vites** | siyah + kahverengi ikili soket | **Kısa devre edilince** motor ters döner |
| **Vites (3 kademe)** | gri + siyah + kahverengi | Hız limiti kademesi. (Başka kaynakta siyah/mavi/sarı) |

## 🚨 İKİ TANE TEKLİ MOR KABLO VAR — KARIŞTIRMA

Tabloda görüleceği gibi hem **yüksek seviye fren** hem **gösterge** çıkışı
"tekli mor" diye tarif ediliyor. Bunlar tamamen farklı iki şey:

- Gösterge ucu bir **çıkış** — beyin oradan hız frekansı basıyor.
- Yüksek seviye fren ucu bir **giriş** — +12 V bekliyor.

Karıştırmanın bedeli: göstergenin çıkışına 12 V vermek beyni bozabilir.
**Ayırt etme yöntemi** (akü bağlı, tekerlek serbest, gaz yok):

1. Multimetreyi DC'ye al, siyah probu şaseye tak.
2. Tekerleği elle çevir. **Gerilimi oynayan / titreşen uç GÖSTERGEDİR.**
3. Boşta sabit 0 V duran ve tekerlek dönerken hiç değişmeyen uç fren girişidir.
4. Emin olamazsan sinyali osiloskopla ya da Mega'ya `pulseIn` ile bak —
   gösterge ucunda tekerlek dönerken kare dalga vardır.

## İKA'ya bağlanma noktaları

### Geri vites — ZATEN BAĞLI, ama renk uyuşmuyor

Araçta D7'deki tek röle bu çifti kısa devre ediyor. `ANA_NOT.md` "mavi uç
şaseye" diyor, bu iki kaynak ise **siyah + kahverengi** diyor. İkisinden biri
yanlış ya da bizim kontrolcümüz farklı renklendirilmiş. **Çalışıyor olması
yeterli, dokunma** — ama kablo demeti sökülürse bu not yanıltır, o zaman
yeniden ölç.

### D13 fren kesme — HENÜZ BAĞLI DEĞİL

Firmware'de `BRAKE_CUTOFF_PIN = 13` var ve `fren_uygula()` içinde HIGH/LOW
yazılıyor, ama **pin fiziksel olarak hiçbir yere gitmiyor** (2026-08-17'de
doğrulandı). Yani "fren varken gazı kes" koruması kodda var, gerçekte yok.

**Doğrudan bağlanamaz.** İki fren girişinin ikisi de mantık seviyesi kabul
etmiyor: düşük seviye **kısa devre** bekliyor, yüksek seviye **+12 V**.
Mega'nın 5 V pini ikisini de süremez.

**Önerilen: düşük seviye fren + N-kanal MOSFET.**

```
Mega D13 ──[10k seri]── MOSFET gate
                        MOSFET source ── GND (kontrolcüyle ORTAK)
                        MOSFET drain  ── frenin mor/gri ucu
             gate ──[100k]── GND   (Mega reset'teyken kapalı kalsın)
```

Ortak toprak zaten var (gaz DAC'ı için şart). D13 HIGH → MOSFET iletir →
fren çifti kısa devre → beyin gazı keser. Firmware mantığı bu kurulumda
**doğru yönde**: `binde > 0` iken HIGH yazıyor.

**Önce ölç:** frenin sinyal ucunun boştaki gerilimi. ~5 V ise mantık seviyesi
MOSFET yeter; ~12 V ise 12 V'a dayanan biri gerekir (akım küçük, 2N7000 bile
taşır). Gate direnci şart değil ama sığa akımını sınırlar.

Röle de olur ama gereksiz: kontak ömrü, ses ve gecikme getiriyor.

### Gösterge kablosu — enkoder gelene kadar ODOMETRİ KÖPRÜSÜ

**Bu belgedeki en değerli şey bu olabilir.** Gösterge ucu hıza orantılı frekans
üretiyor ve **kontrolcünün üstünde hazır duruyor**: kaplin yok, braket yok,
mil çapı ölçümü yok, mekanik montaj yok. Mega'nın bir kesme pinine alıp
frekans saymak birkaç saatlik iş.

Neden önemli: enkoder ve gövde IMU'su takılı olmadığı için odometri yok,
EKF beslenemiyor, **Nav2 zinciri hiç denenemedi** — Mardin'in en büyük
takvim riski bu.

**Sınırı dürüstçe:** yön bilgisi vermiyor, yalnız hız büyüklüğü. Sponsor
enkoderi de tam bu yüzden elenmişti. Ama:

- Geri vites **bizim komutumuzla** oluyor (D7 rölesi), yani yönü firmware
  zaten biliyor. Bilinmeyen tek durum rampada istem dışı geri kayma; orası
  için IMU var.
- E6B2 gelene kadar odometriyi ayağa kaldırır → Nav2 denemesi beklemez.
- E6B2 gelince **çapraz doğrulama** için kalır.

**Ölçeklemesi kalibrasyon işi:** darbe/tur sabiti bilinmiyor, bilinen mesafeyi
sürüp sayarak çıkarılır. Sinyal seviyesini önce ölç — 12 V ise Mega'ya
doğrudan bağlama, bölücü koy.

### İnce kırmızı (kontak) — traksiyon gücünü kesmenin DOĞRU yeri

Traksiyonu uzaktan kesmek gerekirse ana batarya kablosunu değil **bu ucu**
anahtarla. Ana hattı röleyle kesmek 60 A'lık ark demek.

**Ama E-STOP'un yerine geçmez:** acil durdurma donanımdan olmalı, yazılımdan
sürülen bir röleye bağlanmamalı.

## Montaj sırası ve tuzaklar

1. **Ana güç kablolarını EN SON bağla.** Önce bütün soketler, en sonda akü.
2. **Kalın kırmızıyı takarken çatlama/ark normaldir** — beynin içindeki
   kondansatörler dolarken oluyor. 51,2 V 30 Ah'lik pakette bu ark ciddi;
   ön şarj direnci kullanmak kontakları korur.
3. **İki beyaz self-learning ucu AÇIK kalmalı.** Takılı unutulursa kontrolcü
   faz öğrenme moduna girer ve **tekerlek kendi kendine döner.** Araçta
   bunların ayrı ve yalıtılmış olduğunu teyit et.
4. Soketleri numaralandır/bantla — renk uysa bile pin sırası uymayabilir.
5. Faz kablolarını iyi izole et; kısa devre beyni anında bitirir.

## Gaz aralığı notu

Kontrolcü **1–4,2 V** kabul ediyor. Bizim firmware %60'ta 2,84 V, şu anki
%30'da 1,82 V üretiyor — yani aralığın **üst yarısı hiç kullanılmıyor.**
Güç yetmezse çözüm DAC'ta değil, `MANUEL_MAX_HIZ` sabitinde. Ölçülen kalkış
eşiği ~1,22 V ile de tutarlı.

---

Kaynaklar: letrigo.com "Types of Wires in eBike Systems" · pilmak.com ürün
sayfası · montaj videosu dökümü (youtube R5zqWQiaCw8). Hiçbiri araçtaki
kontrolcüde doğrulanmadı.
