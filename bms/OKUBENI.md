# İki BMS okuma altyapısı

**Durum: kod yazıldı ve donanımsız doğrulandı, gerçek BMS'lerle HENÜZ denenmedi.**
Manisa'da yapılacak deneme için hazırlandı.

## Cihazlar

| | Ana batarya | Elektronik bataryası |
|---|---|---|
| Paket | 16S5P LiFePO4, 51,2 V 30 Ah | ProFuse LiPo **4S 14,8 V** 8000 mAh 65C |
| BMS | JK Smart 60A | **DALY Smart Active Balance, 4–8S** |
| Etiket | — | 60 A şarj / 60 A deşarj, balans 1 A, `BT ID: JHB-D2191012A` |
| Kablolu port | **YOK** (yalnız şarj jakı + XT90) | **VAR** — `UART`, `CAN/485`, ayrıca `KEY`, `DIO`, `NTC-A/B` |
| Okuma yolu | BLE (mecbur) | **UART (tercih)** ya da BLE |

## Dosyalar

| Dosya | Ne yapar |
|---|---|
| `daly_protokol.py` | DALY çerçevesi kurma/çözümleme. G/Ç yok, donanımsız test edilebilir |
| `bms_oku.py` | İkisini aynı anda okur, CSV yazar, koparsa geri bağlanır |
| `bms_tara.py` | Eski betik — JK'yı bulmak ve ham çerçeve dökmek için hâlâ geçerli |
| `kayit/` | CSV çıktıları (cihaz başına ayrı dosya) |

Donanımsız doğrulama (ikisi de geçti, WSL'de çalıştırıldı):

```bash
python3 daly_protokol.py --test     # 20 test: ölçekler, çok çerçeve, bozuk çerçeve reddi
python3 bms_oku.py --daly-sahte     # tüm boru hattı, uydurma veriyle
```

## Seçilen yol: ikisi de BLE

**Karar (17 Ağu): şimdilik UART çekilmiyor, iki BMS de BLE'den okunuyor.**
Bu çalışır; iki eşzamanlı BLE bağlantısı hiçbir denetleyici için sorun değil.

```bash
python3 bms_oku.py --tara                              # adresleri bul
python3 bms_oku.py --daly-ble AA:BB:.. --jk-ble CC:DD:..
```

Kabul edilen bedeller — sürpriz olmasın diye yazıyorum:

- **Jetson bağlıyken telefon iki BMS'e de giremez.** Sahada bir hücre gerilimine
  uygulamadan bakmak istersen önce betiği Ctrl+C ile durdurman gerekir.
- **2,4 GHz paylaşımı.** BLE ile WiFi aynı bandı, çoğu M.2 kartta aynı anteni
  kullanıyor. Ana bağlantın 5 GHz'de (`LYDIA`) olduğu sürece etkisi küçük;
  `LYDIA-24`'e düşersen ROS gecikmeleri artar.
- **Kopma normaldir.** Metal şasi BLE'yi yer. Bu yüzden her cihazın kendi
  gözcüsü var: kopunca 2→30 sn artan beklemeyle geri bağlanıyor ve **sessiz
  ölümü de yakalıyor** (20 sn çerçeve gelmezse bağlantıyı ölü sayıp yeniler).
  Bu davranış taklit kopmayla test edildi.

### Manisa'dan önce 30 saniyelik tek kontrol

`python3 bms_oku.py --tara` çalıştır. **DALY listede görünüyor mu?**

- **Görünüyorsa** BLE modülü — yapacak bir şey yok, `--daly-ble` ile devam.
- **Görünmüyorsa** modül klasik Bluetooth SPP'dir (BLE değil). O zaman:
  ```bash
  bluetoothctl                      # pair <MAC> ile eşleştir
  sudo rfcomm bind 0 <MAC> 1        # /dev/rfcomm0 oluşur
  python3 bms_oku.py --daly-seri /dev/rfcomm0
  ```
  **Kod değişikliği gerekmiyor** — seri taşıyıcı `/dev/rfcomm0`'ı da açar.

JK'yı ararken `--tara` çıktısındaki ad ipuçlarına bak; DALY genelde `DL-…`
ya da `JHB…` ile başlıyor.

## Sonradan UART'a geçmek istersen

BMS'te `UART` portu duruyor, altyapı hazır (`--daly-seri`). Kazancı: telefon
kilidi kalkar, BLE bütçesi JK'ya kalır, kopma olmaz. Geçersen:

### Kabloyu çekmeden önce — ÖLÇ, tahmin etme

DALY'nin `UART` soketi modele göre farklı pinlenmiş olabiliyor ve bazı
sürümlerde bir pinden **paket gerilimi** çıkıyor. Yanlış pini USB-TTL'e
bağlamak dönüştürücüyü de Jetson'ın portunu da yakar.

1. BMS bataryaya bağlıyken multimetreyle **her pini `B-`'ye göre ölç**.
   Beklenen: bir pin 0 V (GND), iki pin ~3,3 V civarı boşta (TX/RX),
   varsa bir pin paket gerilimi (**ona dokunma**).
2. GND'yi süreklilikle teyit et: `B-` ile 0 Ω olan pin GND'dir.
3. **3,3 V mantıklı USB-TTL** kullan. 5 V'luk bir dönüştürücünün TX'i
   BMS'in RX'ine doğrudan bağlanmaz.
4. Bağlantı: USB-TTL `TX` → BMS `RX`, `TX` ← BMS `TX` (çapraz), `GND` → `GND`.
   USB-TTL'in `VCC`'sini BMS'e **bağlama** — besleme BMS'te zaten var.
5. Ortak toprak şart: elektronik bataryasının `B-`'si ile Jetson toprağı
   aynı noktada olmalı, yoksa UART gürültüden okunmaz.

Bağlandıktan sonra:

```bash
ls -l /dev/serial/by-id/          # kalıcı ad; ttyUSB numarası reboot'ta kayar
python3 bms_oku.py --daly-seri /dev/serial/by-id/<uzun-ad>
```

> Not: udev kuralı yazılana kadar `by-id` yolunu kullan. İki CP2102'nin aynı
> seri numarayla gelme tuzağı bu projede bir kez yaşandı — USB-TTL'i takınca
> `by-id` altında gerçekten ayrı görünüyor mu bak.

## Manisa'da sıra

1. **Telefondaki BMS uygulamalarını kapat.** İkisi de tek istemci kabul ediyor;
   uygulama açıksa Jetson bağlanamaz ve sebebi anlaşılmaz görünür.
2. **`--tara`** ile iki adresi de not al. DALY görünmüyorsa yukarıdaki
   klasik-Bluetooth yoluna geç.
3. **Önce yalnız DALY:** `python3 bms_oku.py --daly-ble <MAC> --aralik 1`
   Gerilim/akım/SOC akıyorsa protokol katmanı doğrulanmış olur.
4. **Ölçekleri teyit et** (aşağıdaki tablo). Bu adımı atlama.
5. **Sonra ikisi birden:** `--daly-ble <MAC> --jk-ble <MAC>`.
   JK'dan ham döküm toplanacak; çözümleyici ondan sonra yazılacak.
   Bu adımda ayrıca iki eşzamanlı bağlantının kararlılığını görmüş olursun —
   ekranda "⚠ n sn'dir veri yok" uyarısı çıkıyorsa not al.
6. Toplanan CSV'leri geri getir, JK çözümleyicisini birlikte yazalım.

### Teyit edilecek dört sayı

Protokol belgeden alındı, sağlama toplamı tutuyorsa **çerçeve yapısı** doğrudur.
Ama ölçek katsayıları modele göre değişebiliyor — şunları gözle doğrula:

| Ne | Beklenen | Tutmazsa |
|---|---|---|
| Toplam gerilim | Multimetreyle ölçtüğünle aynı (4S dolu ≈ 16,8 V) | ölçek 0,1 V değil, 0,01 V olabilir |
| **Akım işareti** | Yük çekerken **negatif** | 30000 ofsetinin yönü ters, `daly_protokol.py`'de düzelt |
| Hücre sayısı | **4** (`0x94`) | BMS 4S'e ayarlanmamış |
| Hücre gerilimleri | Toplamları paket gerilimine eşit | `0x95` birleştirme sırası yanlış |

## İki uyarı

**1. BMS akımı traksiyon ölçümünün yerine geçmez.** Her iki BMS de ~1 Hz ve
ortalanmış akım veriyor; kalkıştaki 200 ms'lik tepe orada görünmez. Ana
bataryanın BMS'i **60 A**'da kesiyor ve seni parkur ortasında kapatacak olan
tam o tepe. XT90 hattına **ACS758** hâlâ gerekli — sipariş listesindeki
eksik kalemlerden biri.

**2. DALY'nin kimyası LiPo'ya ayarlı mı?** Bu BMS ailesi LiFePO4 ve Li-ion/LiPo
varyantlarıyla satılıyor ve etikette görünmüyor. Pakette LiPo var (hücre başına
4,2 V dolu, 3,0 V dip). BMS LiFePO4 varyantıysa:

- şarj kesmesi ~3,65 V/hücre → paket hiç dolmaz (14,6 V'ta durur),
- **deşarj kesmesi ~2,5 V/hücre → LiPo'yu aşırı boşaltır**, bu kalıcı hasar
  ve şişme sebebi.

Uygulamadan (ya da `0x5A` parametre komutlarıyla) **hücre üst/alt kesme
eşiklerine bak.** Üst eşik 4,2–4,25 V değilse bu BMS bu pakete takılmamalı.
İlk çalıştırmada bunu doğrula — kod tarafı değil, güvenlik tarafı.

## Sonraki adım

Sahada veri aktığı doğrulanınca `bms_oku.py` bir ROS 2 düğümüne sarılıp
`/batarya/ana` ve `/batarya/elektronik` başlıklarını yayımlayacak; panodaki
BMS widget'ı zaten bu veriyi bekliyor. Şimdilik CSV yeterli — önce sayıların
doğru olduğundan emin olalım.

---

## ✅ 3 Eylül 2026 — JK ana batarya BLE'den OKUNDU (ilk kez)

**Durum artık "denenmedi" değil.** Jetson'dan gerçek cihazla çalıştı:
1102 çerçeve / 78 KB, kesintisiz.

| | |
|---|---|
| Adres | `AA:BB:CC:DD:EE:FF` |
| Yayın adı | **`50912AA35102300`** — içinde "JK" ya da "BMS" geçmiyor |
| Servis / karakteristik | `0xFFE0` / **`0xFFE1`** (oku + yaz aynı uç) |
| Çerçeve formatı | **`55 AA EB 90` = "yeni JK"** — artık biliniyor, çözümleyici yazılabilir |

### Düzeltilen üç kusur

1. **JK kendiliğinden yayın YAPMIYOR.** Eski `jk_dongu` yalnız dinliyordu;
   cihaz bağlanıyor ama tek bayt göndermiyordu. Bağlantıdan sonra **istek
   çerçevesi** yazılması gerekiyor: `jk_komut(0x97)` (cihaz bilgisi) +
   `jk_komut(0x96)` (hücre bilgisi), sonra düzenli tazeleme.
   Çerçeve: `AA 55 90 EB <kod> 00…00 <ilk 19 baytın toplamı & 0xFF>`.
   **0x96 ve 0x97 salt okumadır** — bilerek yalnız bu ikisi kullanılıyor.
2. **İstek yanlış uca gidiyordu.** JK'ya aday listesi verilmediği için yazma
   karakteristiği olarak GATT'taki ilk yazılabilir uç (`0xFFE3`) seçiliyordu.
   `JK_BLE_ADAYLAR` eklendi: oku ve yaz ikisi de `0xFFE1`.
3. **Tarama JK'yı kaçırıyordu.** İsimde "jk"/"bms" arıyordu, cihazın adı ise
   seri numarası. Tarama artık **servis kimliğine** bakıyor (`0xFFE0`/`0xFFF0`);
   isim yalnız yardımcı ipucu.

### Yeni araç

`ble_incele.py <adres>` — bir BLE cihazına bağlanıp GATT ağacını döker ve BMS
imzalarını işaretler. **Sadece okur.** Adı tanınmayan bir cihazın BMS olup
olmadığını bu ayırt etti.

### 🔴 Hâlâ açık

- **JK çözümleyicisi yok** — çerçeveler ham kaydediliyor, gerilim/akım/hücre
  değerlerine ayrıştırılmıyor. Format artık bilindiği için önü açık.
- **DALY hiç görünmedi** — ne BLE ne klasik Bluetooth taramasında. Elektronik
  bataryası bağlı değilken bakıldı; batarya takılıyken tekrarlanmalı.
- **DALY'nin kimya ayarı doğrulanmadı** (hücre üst kesme 4,2–4,25 V olmalı).
  Güvenlik maddesi, duruyor.
