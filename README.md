<div align="center">

**🇹🇷 Türkçe** &nbsp;·&nbsp; [🇬🇧 English](README.en.md)

# İKA · MAGNESİA · LYDİA

Otonom kara aracının sürüş kartı mimarisi, seri protokolü ve emniyet zinciri

[![Lisans](https://img.shields.io/badge/kod-AGPL--3.0--only-3DA639?style=flat-square)](LICENSE)
[![Belgeler](https://img.shields.io/badge/belgeler-CC%20BY--SA%204.0-EF9421?style=flat-square)](NOTICE)
[![MCU](https://img.shields.io/badge/STM32-Nucleo--F767ZI-03234B?style=flat-square&logo=stmicroelectronics&logoColor=white)](#)
[![Platform](https://img.shields.io/badge/Jetson-Orin%20Nano-76B900?style=flat-square&logo=nvidia&logoColor=white)](#)
[![ROS](https://img.shields.io/badge/ROS%202-Humble-22314E?style=flat-square&logo=ros&logoColor=white)](#)

</div>

TEKNOFEST 2026 İnsansız Kara Aracı yarışmasında **finalist** olan **LYDİA**
aracının gömülü tarafı. MCBÜ **MAGNESİA** takımı.

> Kartın tam firmware'i yayımlanmamıştır. Yayımlanan şey **mimari, arayüz ve
> firmware'in yeniden kullanılabilir çekirdeği**: çerçeveleme, iBUS çözümü,
> emniyet mandalları ve direksiyon jog profili platformdan bağımsız modüller
> hâlinde, **masaüstünde koşan 75 birim testiyle** birlikte. Araca ait ölçülmüş
> kalibrasyon sayıları yer almaz; yerlerine nasıl ölçüldükleri yazılıdır.

---

## Sistem

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="varlik/mimari-koyu.svg">
  <img alt="LYDİA mimarisi: Jetson algı katmanı, F767 sürüş ve emniyet katmanı, donanım acil stop" src="varlik/mimari-acik.svg">
</picture>

Aracın **bütün sürüş ve sensör yazılımı tek bir STM32 F767ZI'de** toplanmıştır:
kumanda, gaz, fren, direksiyon, geri vites, taret ve güç kesme. Üstünde Jetson
Orin Nano yalnız algı ve planlama yapar; araca doğrudan hiçbir aktüatör komutu
gitmez, her şey kartın kip hakeminden geçer.

Bu ayrımın pratik sonucu şu: **üst bilgisayar tamamen çökse bile manuel sürüş
çalışmaya devam eder.** Kumanda alıcısı karta doğrudan bağlıdır.

## Neyi nerede bulursunuz

| Belge | İçerik |
|---|---|
| [`PROTOKOL.md`](docs/PROTOKOL.md) | 8 baytlık çerçeve, 14 telemetri + 9 komut paketi, tüm bayraklar |
| [`ACIL_STOP.md`](docs/ACIL_STOP.md) | İki kanallı acil stop bağlantısı ve gerekçesi |
| [`KALIBRASYON.md`](docs/KALIBRASYON.md) | Her sabitin **nasıl ölçüldüğü** — değerler değil, yöntem |
| [`BAGLANTI_HARITASI.md`](docs/BAGLANTI_HARITASI.md) | Araçtaki tüm elektronik bağlantılar, kesit ve uzunluklarıyla |
| [`KONTROLCU_KABLO_HARITASI.md`](docs/KONTROLCU_KABLO_HARITASI.md) | Ana motor kontrolcüsünün kablo çözümlemesi |
| [`USB_BAGLANTI.md`](docs/USB_BAGLANTI.md) | Kart–bilgisayar bağlantısı için seçenek değerlendirmesi |
| [`ARIZA_GUNLUGU.md`](docs/ARIZA_GUNLUGU.md) | Sahada yaşanmış yedi arıza ve nasıl bulundukları |
| [`firmware/OKUBENI.md`](firmware/OKUBENI.md) | Çekirdek modüller, birim testler ve tezgâh testleri |

---

## Öne çıkan tasarım kararları

### 🛑 Acil stop, kesmeyi yazılıma sormaz

```mermaid
flowchart LR
    B["🔴 Mantar buton<br/>mandallı"] --> M["ana motor<br/>beslemesi"]
    B -. "2. NC blok" .-> P["PF14<br/>durum girişi"]
    P --> F["firmware<br/>durumu raporlar<br/>komutları keser"]
    J["üst bilgisayar<br/>E-STOP ilanı"] --> F
```

Mantar buton ana motor beslemesini **kendi kontağıyla** keser. Araya röle,
kontaktör ya da transistör konulmamıştır — çünkü konulsaydı, kesme o elemanın
çalışmasına bağımlı hâle gelirdi. Kart donsa, resetlense ya da USB'si çekilse
bile motor durur.

Butona **ikinci bir NC bloğu** eklenmiştir; o blok yalnız kartın durumu
görmesi içindir. Yani kesme yolu ile izleme yolu fiziksel olarak ayrıdır.

Üst bilgisayar da `0x07` paketiyle E-STOP ilan edebilir — ama bu **yazılım
seviyesinde** bir durdurmadır ve donanım kesmesinin yerine geçmez, yanına
eklenir.

### 🔀 Kip hakemi kartta

Manuel, otonom ve kesme kipleri arasındaki geçişe **kart** karar verir, üst
bilgisayar değil. Otonom kipteki bir yazılım hatası, kumandanın yetkisini
elinden alamaz.

Kip geçişinde kart Jetson komutlarını **sıfırlar** ve taze bir `0x01` bekler.
Geçişten önce gönderilmiş bir hız komutunun, geçişten sonra uygulanması
mümkün değildir.

### 🤝 Arayüz, uygulamadan bağımsız

İki taraf birbirinin kodunu görmeden çalışır. Sözleşmenin tamamı 8 baytlık
çerçeve, sabit paket tablosu ve bir sürüm numarasıdır. Kart aldığı komutu
**anladığı haliyle geri yankılar** (`0x38`) — ölçek, işaret ve kırpma hataları
araç hareket etmeden ortaya çıkar.

### 📏 Kalibrasyon derlemede değil, çalışma anında

Hiçbir kalibrasyon sabiti koda gömülü değildir; `0x09` ile yazılır, kalıcı
bellekte tutulur, `0x3E` ile geri okunur. Sabit değişince firmware yeniden
derlenmez — sahada ölçüp sahada yazarsınız. Geri okuma, "yazdım ama tuttu mu"
sorusunu tahminden çıkarır.

---

## Depoda ne var

```
firmware/cekirdek/    Ana firmware'den ayrılmış platformdan bağımsız modüller
firmware/test/        Masaüstünde koşan 75 birim test — kart gerekmez
firmware/tezgah/      16 tezgâh testi: sketch + pano + kablo belgesi
docs/                 Protokol, emniyet, kalibrasyon, bağlantı, arıza günlüğü
bms/                  İki BMS'i BLE üzerinden okuyan servisler ve protokol çözümü
kamera/               Kamera akışı, 180° çevirme ve kayıt sunucusu
varlik/               Mimari şeması (açık / koyu tema)
kalibrasyon.ornek.h   Sabitlerin yer tutucu tanımları — değerler boş
```

### Kartsız çalışan testler

```bash
cd firmware/test && make
```

```
cerceve                     23 gecti, 0 kaldi
emniyet                     23 gecti, 0 kaldi
ibus                        16 gecti, 0 kaldi
direksiyon                  13 gecti, 0 kaldi
```

Sürüş mantığının donanıma dokunmayan kısımları ayrı modüllere çıkarıldı;
hiçbiri `digitalRead`, `millis` ya da `Serial` çağırmıyor. Aynı kod hem kartta
koşuyor hem `g++` ile masaüstünde test ediliyor
(`-Wall -Wextra -Wpedantic -Werror`). Ayrıntı:
[`firmware/OKUBENI.md`](firmware/OKUBENI.md).

### Tezgâh testleri

Her parça **araca takılmadan önce, masada, tek başına** doğrulanır: kartta
koşan sketch, tarayıcıda açılan pano, `BAGLANTI.md` kablo belgesi. Testi geçen
parçaların klasörlerinde ölçülen değerlerle birlikte `SONUC.md` var.

**Neden tek tek:** araca beş şey birden takıp "çalışmıyor" demek, beş arızayı
birbirine karıştırmak demektir. Bu projede iki kez yaşandı.

### BMS okuma

Araçta iki ayrı batarya ve iki ayrı BMS var: traksiyon hattı ve elektronik
hattı. İkisi de BLE üzerinden okunuyor ve JSON olarak yayınlanıyor.

`bms/` altındaki çözümleme, cihazların **yayınlanmamış** paket biçimlerinin
tersine mühendislikle çıkarılmasına dayanıyor; yöntemi ve tuzakları
[`bms/OKUBENI.md`](bms/OKUBENI.md) anlatıyor. En önemli tuzak: bağlantı
koptuğunda değerler **donuyor**, sıfırlanmıyor — yani "bağlı mı" alanına değil,
verinin **yaşına** bakmak gerekiyor.

---

## Bu depoda bilerek olmayanlar

- **Kartın tam firmware'i ve derlenmiş ikilileri.** Çekirdek modülleri,
  arayüzü ve tezgâh testleri burada; sürüş kararı, kip hakemi ve telemetri
  döngüsünün tamamı değil.
- **Araca ait ölçülmüş kalibrasyon değerleri.** Yerlerinde yer tutucu ve ölçüm
  yordamı var.
- **Otonomi paketi ve yer istasyonu arayüzü.** Takımın diğer üyeleriyle ortak
  geliştirildi; onların emeği bana ait değil.
- **Ağ yapılandırması, erişim bilgileri ve cihaz adresleri.** Yer tutucuyla
  değiştirildi.

## Lisans

Kod **AGPL-3.0-only** ([LICENSE](LICENSE)), belgeler ve çizimler
**CC BY-SA 4.0** — bkz. [NOTICE](NOTICE).
