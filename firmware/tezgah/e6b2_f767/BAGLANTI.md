# E6B2-CWZ6C → Nucleo-F767ZI · tezgah bağlantısı

Omron artımlı enkoder, 600 P/R, NPN açık kollektör. Masada elle çevirerek.

---

## 1. Şema

```
   E6B2-CWZ6C                              Nucleo-F767ZI
   ┌──────────────┐                        ┌──────────────────┐
   │ KAHVERENGİ   ├──── +12 V (harici) ────┤ (Nucleo'ya DEĞİL)│
   │ MAVİ    0 V  ├────────┬───────────────┤ GND   ← ortak    │
   │              │        │               │                  │
   │ SİYAH    A   ├────────┼───────┬───────┤ PD12 (TIM4_CH1)  │
   │ BEYAZ    B   ├────────┼───┬───┼───────┤ PD13 (TIM4_CH2)  │
   │ TURUNCU  Z   ├────────┼─┬─┼───┼───────┤ PD11             │
   └──────────────┘        │ │ │   │       │                  │
                           │ │ │   │       │  3V3 ──┐         │
                         (0 V)│ │   │       └────────┼─────────┘
                              │ │   │                │
                              └─┴───┴──[1 kΩ ×3]─────┘
                                  PULL-UP'LAR 3,3 V'A
```

### ⚠ Pull-up'lar 3,3 V'a gider, 12 V'a DEĞİL

E6B2'nin çıkışı **NPN açık kollektör**: transistör iletince hattı 0 V'a
çeker, kesince hat serbest kalır. Yüksek seviyeyi **pull-up'ın bağlı olduğu
gerilim belirler.** 3,3 V'a bağlarsan sinyal 0–3,3 V arasında salınır —
seviye çevirici gerekmez, bu yüzden bu enkoder STM32'ye doğrudan bağlanabilir.

**12 V'a bağlarsan pini yakarsın.** Enkoderin kendisi 12 V'tan beslenir,
sinyal hattı 3,3 V'tan pull-up'lanır. İkisi ayrı şeydir.

### ⚠ 1 kΩ şart, dahili pull-up yetmez

STM32'nin dahili pull-up'ı ~40 kΩ. Elle yavaş çevirirken çalışır, ama araçta
53 kHz'e çıkıldığında kablo kapasitesiyle birlikte kenarı yuvarlar ve sayım
kaçar. **1 kΩ dirençler malzeme listesinde var** (§9), takılmadan araç testi
yapılmayacak.

### ⚠ Push-pull varyantı bu şemayı bozar

E6B2 ailesinde gerilim çıkışlı (push-pull) modeller de var. O modelde çıkış
12 V'u aktif sürer ve **pull-up ile kurtaramazsın, bölücü gerekir.**
Kutu açılınca etiketten çıkış tipi doğrulanacak — `CWZ6C` = NPN açık
kollektör, ama sipariş edilenin gerçekten o olduğu görülmeli.

### Besleme

E6B2 5–24 V ister. **Tezgahta Nucleo'nun 5 V'u kullanılıyor (31 Ağu 2026) ve
bu DOĞRU seçim** — hem yeterli, hem daha güvenli.

Besleme gerilimi sinyal seviyesini **etkilemiyor**: çıkış NPN açık kollektör,
yüksek seviyeyi pull-up'ın bağlı olduğu gerilim (3,3 V) belirliyor. 5 V'tan da
24 V'tan da beslesen hat 0–3,3 V arasında salınır.

5 V'ın güvenlik payı: yukarıdaki push-pull uyarısı gerçekleşirse (etiket
yanlış, gerilim çıkışlı model gelmiş) pine 12 V yerine 5 V gider ve bu pinler
5 V toleranslı — kurtulur. 12 V'ta kurtulmaz.

⚠ **0 V'u Nucleo'nun GND'siyle ortakla** (yoksa sinyalin referansı olmaz ve
hiçbir şey okunmaz).

Araca geçerken 12 V tercih edilir: uzun kabloda ve motor gürültüsünde daha
dayanıklı. Tezgahta böyle bir sorun yok.

---

## 2. Sıra

1. 12 V kaynağı bağla, **0 V'u Nucleo GND'siyle ortakla**.
2. Üç sinyal telini pinlere, üç adet 1 kΩ'u pinlerle 3V3 arasına.
3. Nucleo'yu USB'den laptopa tak, yükle: `~/.local/bin/pio run -t upload`
4. Panoyu aç: `python.exe enk_pano.py --port COM6`
5. **Mili elle YAVAŞÇA çevir.**

---

## 3. Ne aranıyor — sayımın artması YETMEZ

Bu testin can alıcı noktası bu. Tek kanal çalışan bozuk bir enkoder de sayım
artırır; sponsorun 300B'si tam bu yüzden haftalarca sağlam sanıldı.

| # | Test | Nasıl | Geçti sayılır |
|---|---|---|---|
| 1 | Canlılık | Mili çevir | Sayım değişiyor |
| 2 | **Kuadratür** | **Ters yöne çevir** | **Sayım GERİ gidiyor** |
| 3 | Kanal sağlığı | Yarım tur çevir | A ve B kutucukları **ikisi de yeşil** |
| 4 | Ölçek | Tam 1 tur çevir | Sayım **2400** artıyor (±birkaç) |
| 5 | Z indeksi | Birkaç tur çevir | Her turda Z 1 artıyor |
| 6 | Gürültü | Yavaş çevirirken bak | "Yasak geçiş" 0'da kalıyor |

**2. test kritiktir.** Geri gitmiyorsa B kanalı ölüdür — sayım artıyor olması
seni yanıltmasın.

**3. test 2.'yi tamamlar:** kanal kutucuğu, o kanalın hem HIGH hem LOW
görüldüğünü söyler. Bir kanal hiç kımıldamıyorsa kırmızı kalır. 300B'de
beyaz tel 64 örneğin 64'ünde sabitti ve bu böyle yakalandı.

⚠ **Multimetreyle bu testi yapmaya çalışma.** İki kere denendi, ikisi de boşa:
beslemesiz direnç ölçümü sağlam kanalı da "OL" okur; beslemeliyken de DMM
saniyede 2–3 örnekliyor, 600 P/R'de bir darbe 0,6° şaft dönüşü. Doğru alet
mikrodenetleyicidir.

---

## 4. Ters giderse

| Belirti | Sebep |
|---|---|
| Sayım hiç değişmiyor | Pull-up yok · besleme yok · GND ortak değil |
| Sayım artıyor ama geri gitmiyor | **B kanalı ölü** — 300B'nin arızası |
| Sayım rastgele zıplıyor | Pull-up çok zayıf (dahili kullanılıyor) · kablo ekranlanmamış |
| Tam turda 2400 değil | P/R etiketten farklı · yanlış enkoder gelmiş |
| Z hiç artmıyor | Turuncu tel bağlı değil, ya da modelde Z yok |
| "Yasak geçiş" hızla artıyor | Gürültü — ekranlı kablo şart, ekran tek uçtan GND'ye |

---

## 5. Sonrası

Geçtikten sonra sırada mekanik var, ve o **hâlâ eksik**:
kaplin, braket ve **direksiyon mili çapı ölçümü** (Manisa listesi).
Enkoderin kendi kablosu yalnız **0,5 m** — motordan panoya yetmiyor, ekranlı
uzatma listede.

Ölçek sabitleri bu testten değil araçtan çıkar: motor–tekerlek dişli oranı ve
tekerlek çevresi ölçülmeden `ENK_TEKER_CEVRE_MM` 0 kalır ve ana firmware
mesafe yayınlamaz (uydurma odometri yayınlamaktansa susmayı seçiyor).
