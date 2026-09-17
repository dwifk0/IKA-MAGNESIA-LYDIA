# BNO055 → Nucleo-F767ZI · tezgah bağlantısı

UART modu. Masada, araç yokken. Toplam 5 tel.

---

## 1. Şema

**Modül: Boardoza BNO055** (`tasarim/fritzing/Boardoza-BNO055.fzpz`).
Uç isimleri ve kart üstündeki parçalar oradan doğrulandı.

```
   Boardoza BNO055                        Nucleo-F767ZI
   ┌───────────────┐                      ┌──────────────────┐
   │ J1-1  VCC     ├──────────────────────┤ 3V3   ⚠ 5 V DEĞİL │
   │ J1-2  SCL =RX ├───────◄  ÇAPRAZ  ────┤ PD5  (USART2_TX) │
   │ J1-3  SDA =TX ├───────►  ÇAPRAZ  ────┤ PD6  (USART2_RX) │
   │ J1-4  GND     ├──────────────────────┤ GND              │
   │               │                      │                  │
   │ J2-5  PS1  ───┼──► VCC (3V3)  ⚠⚠      │ (RST için PD7)   │
   │ J2-6  PS0  ───┼──► GND                │ (USB → laptop)   │
   │ J2-1  BOOT_LOAD  boşta                └──────────────────┘
   │ J2-2  BL_IND     boşta
   │ J2-3  ADR        boşta (UART'ta anlamsız)
   │ J2-4  INT        boşta
   └───────────────┘
```

⚠ **`J1` sırası: VCC · SCL · SDA · GND.** `SCL`, `SDA`'dan **önce** geliyor —
en kolay karışan yer burası, üstelik ikisi de UART'ta kullanılıyor.

### `SDA` = TX, `SCL` = RX

Protokol seçimi çipin **aynı iki pinini** yeniden görevlendiriyor:

| Uç | I²C modunda | **UART modunda (PS1=1)** |
|---|---|---|
| `SDA` | veri | **UART_TX** — modülün *çıkışı* |
| `SCL` | saat | **UART_RX** — modülün *girişi* |

Yani modülün `SDA`'sı kartın **RX**'ine (`PD6`), `SCL`'i kartın **TX**'ine
(`PD5`) gider. İsim isme değil, **çapraz**.

⚠ **Emin değilsen deneyerek çöz, tartışma:** iki olasılık var, biri yanlışsa
belirti `PS1` hatasıyla birebir aynı (hiç bayt gelmez). `CHIP_ID` gelmiyorsa
**iki teli yer değiştir**, 10 saniye sürer.
Bunu risksiz yapmak için **her iki hatta 220–330 Ω seri direnç** koy: yanlış
takıldığında iki çıkış karşı karşıya gelir, seri direnç akımı sınırlar.
115200 baud'da bu direnç sinyale hiçbir şey yapmaz.

### ⚠ PS1 — bu testin tek büyük tuzağı

BNO055 **varsayılan olarak I²C modunda açılır.** `PS1` pinini `VCC`'ye
çekmezsen çip UART'tan **tek bayt bile göndermez** ve pano "cip yok" der.
Kablolamanda hiçbir hata olmasa bile.

✅ **Bizim modülde sorun yok:** Boardoza kartında `PS1` ve `PS0` ayrı ayrı
`J2-5` ve `J2-6` olarak çıkarılmış. `PS1` → **VCC (3V3)**, `PS0` → **GND**.

🔴 **Fritzing parçası bunun TERSİNİ söylüyor** — parça I²C moduna göre
çizilmiş ("PS0, PS1 ve ADR uçlarını GND'ye bağla"). O açıklama **geçersiz**;
çizim UART'a göre güncellenecek (`CIZIM_YAPILACAKLAR.md`).

### ⚠ TX/RX çaprazdır

Modülün **TX'i** kartın **RX'ine** (PD6) gider. Aynı isme aynı isim bağlamak
en sık yapılan hata; belirti PS1 hatasıyla birebir aynı olur (hiç cevap yok).

Bazı modüllerde `SDA/SCL` pedleri UART modunda `TX/RX` görevi görür —
kartının ipek baskısına bak, `PS1=1` iken hangisinin ne olduğu yazar.

### Besleme

🔴 **`VCC`'yi 3V3'ten besle, 5 V VERME.** Modülde `TLV73333` 3,3 V LDO var
ve giriş 3,3–5,5 V kabul ediyor — ama **dış taraftaki 10 kΩ pull-up'lar
`VCC`'ye bağlı.** 5 V verirsen `SDA`/`SCL` hatları 5 V'a çıkar ve doğrudan
F767'nin 3,3 V'luk pinlerine girer.

TLV73333'ün ~12 mA'de düşümü ihmal edilebilir, yani 3,3 V girişte çip
3,29 V görür — sorun yok. Nucleo'nun 3V3 rayı fazlasıyla yeterli.

⚠ **HARİCİ PULL-UP EKLEME.** Modülde dört tane 10 kΩ zaten var.

### ⚠ Kart üstünde 2N7002 seviye çevirici var — UART için düşünülmemiş

`SDA`/`SCL` hatları **çift yönlü, açık-drenajlı bir I²C seviye çeviricisinden**
(Q1/Q2 + 10 kΩ pull-up'lar) geçiyor. UART'ta bu hatlar push-pull sürülüyor;
çevirici yine de geçirir, ama **tasarlandığı iş bu değil.**

Hesap: 10 kΩ × ~85 pF ≈ 0,85 µs yükselme; 115200 baud'da bit süresi 8,68 µs
→ bitin ~%12'si. **Çalışması bekleniyor.**

🔴 Ama `dusen` sayacı sürekli artıyorsa **ilk şüpheli budur** — kablo boyunu
kısalt, kablolamayı gözden geçir. Çözülmezse UART'tan vazgeçilmez; modül
değişir (PS0/PS1 doğrudan çipe giden, seviye çeviricisiz bir kart).

---

## 2. Sıra

1. `PS1`'i VCC'ye çek (lehim köprüsü ya da tel).
2. Beş teli bağla, TX/RX'i **çapraz** olduğundan iki kez emin ol.
3. Nucleo'yu `CN1` USB'den laptopa tak (kart üstü ST-LINK, harici dongle
   gerekmez). Windows'ta `D:` sürücüsü **`NOD_F767ZI`** olarak açılır.
4. Derle ve yükle:
   ```bash
   cd ~/ika/testler/bno055_f767
   ~/.local/bin/pio run
   ~/ika/testler/yukle.sh              # <- WSL'de calisan tek yol
   ```
   🔴 **`pio run -t upload` WSL'den çalışmaz** — WSL'de USB geçişi yok,
   ST-LINK görünmez. `cp ... /mnt/d/` de çalışmaz (WSL sürücüyü bağlamıyor).
   `yukle.sh` kopyalamayı Windows'a yaptırır; `FAIL.TXT` yoksa başarılı.
5. Panoyu aç: `python.exe bno_pano.py --port COM6`
   *(30 Ağustos'ta COM6'ydı; değişirse Aygıt Yöneticisi'nde
   "STMicroelectronics STLink Virtual COM Port" olarak görünür.)*

---

## 3. Ne aranıyor — üç ayrı soru

| # | Soru | Nasıl görülür | Geçti sayılır |
|---|---|---|---|
| 1 | Çip konuşuyor mu | "Cip" kutucuğu yeşil | CHIP_ID 0xA0 |
| 2 | Açılar anlamlı mı | Kartı yavaşça çevir | Yaw takip ediyor, roll/pitch doğru eksende |
| 3 | **Kalibre mi** | "Kalib SYS" kutucuğu | **sys = 3** |

### Kalibrasyon nasıl yapılır

BNO055 kendi kendine kalibre olur, ama **hareket vermen gerekir**:

| Sensör | Hareket | Süre |
|---|---|---|
| GYRO | Kartı kımıldatmadan düz tut | ~3 sn |
| ACC | Altı farklı yüzeye yatır (küpün altı yüzü gibi), her birinde birkaç saniye bekle | ~30 sn |
| MAG | Havada yavaşça sekiz çiz | ~20 sn |

🔴 **`sys` bu üçünün türevi DEĞİL — 30 Ağustos'ta ölçümle çürütüldü.**
Tezgahta `ksys = 3` iken `kmag = 0` çıktı. Yani `ksys`'e bakıp "her şey
kalibre" demek yanlış; **`kmag`'a ayrıca bakılacak.**

Bunun pratik sonucu: `kmag < 3` iken NDOF modunda bile **yaw mutlak değil.**
Manyetometre kalibre olmadan kuzey referansı yok, yaw sürüklenir.
Nav2 mutlak yön istiyor → sekiz çizme adımı atlanamaz.

⚠ `kmag` bir türlü dolmuyorsa **`opr` alanına bak**: `12` = NDOF (manyetometre
açık), `8` = IMU (manyetometre KAPALI, `kmag` sonsuza kadar 0 kalır).

⚠ **Kalibrasyon güç kesilince kaybolur.** BNO055 offset kayıtlarını
saklayabiliyor ama bu test onları yazmıyor — araç firmware'inde açılışta geri
yükleme yapılacak, yoksa her açılışta baştan kalibrasyon gerekir.

---

## 4. NDOF mü IMU mu

Panodaki iki düğme farklı şeyler ölçüyor:

| Mod | Manyetometre | Yaw | Ne zaman |
|---|---|---|---|
| **NDOF** | var | **mutlak** (kuzeye göre), sürüklenmez | Varsayılan. Nav2 mutlak yön istiyor |
| **IMU** | yok | **bağıl**, yavaşça sürüklenir | Manyetik gürültü şüphesi varken |

⚠ **Araçta bu ayrım kritik olabilir:** 1200 W BLDC motor ve 51,2 V hatları
manyetometreyi bozar. Tezgahta NDOF mükemmel çalışıp araçta yaw'ın saçmalaması
beklenen bir sonuçtur — o zaman IMU moduna düşülüp yaw başka kaynaktan
(enkoder + direksiyon açısı) tamamlanır. **IMU'yu araca taktıktan sonra
kalibrasyonu bir daha kontrol et**, tezgah sonucu araç için geçerli değildir.

---

## 5. Ters giderse

| Belirti | Sebep |
|---|---|
| Hiç veri yok, "cip yok" | **PS1 VCC'de değil** (en sık) · TX/RX çapraz değil · GND ortak değil · **modülün kendi 3,3 V rayı düşük** (aşağı bak) |
| ↳ **`SDA`/`SCL` ters mi** | İki olasılık var, ikisinin belirtisi aynı. **Yer değiştir ve dene** — `SDA` kartın `PD6`'sına gitmeli. ⚠ `J1` sırası VCC·SCL·SDA·GND, `SCL` önce geliyor |
| ↳ **`BOOT_LOAD`** | Modülde 3V3'e 10 kΩ çekili görünüyor, boşta kalmalı. Çip hiçbir şeye cevap vermiyorsa burayı ölç |
| ↳ beslemeyi ölç | `VCC`–`GND` arası 3,3 V mu. ⚠ **5 V verme** — dış pull-up'lar `VCC`'ye bağlı, hatlar 5 V'a çıkar. *(iBUS testinde besleme kaynaklı aynı sınıf hata çıkmıştı: `saglam=0 && bozuk=0` → besleme/kablo.)* |
| CHIP_ID 0x00 geliyor | Hat var ama çip cevap vermiyor — besleme ya da RST takılı LOW |
| "Dusen okuma" sürekli artıyor | 🔴 **Tek başına sebebi söylemez** — altındaki dört sayaca bak (aşağı) |
| ↳ `BUS_OVER_RUN` (0x07) artıyor | Çipi **çok hızlı sorguluyoruz**. BNO055 UART'ının bilinen kusuru. 30 Ağustos'ta tam bu çıktı: her turda 5 ayrı sorgu × 50 Hz = saniyede 250 sorgu → `dusen` %8. Açı 50 Hz'de kaldı, durum baytları 2 Hz'e indi |
| ↳ `cevapsiz` (0xFF) artıyor | Kablo · `PS1` · besleme. Hiç bayt gelmiyor demek |
| ↳ `bozuk cerceve` (0xFE/0xFD) artıyor | Sinyal bütünlüğü: kablo uzun/gürültülü, ya da **2N7002 seviye çevirici** UART'ı zorluyor. Baud uyuşmazlığı da buraya düşer (BNO055 UART'ta **sabit 115200**) |
| Yaw sürükleniyor | Normal, `sys` 3 olana kadar. IMU modundaysan kalıcı olarak normal |
| SYS_ERR ≠ 0 | Çipin kendi hata kodu — 1: çevre birimi, 2: sistem başlatma, 3: kendi kendini test, 4: kayıt değeri hatalı |
| Roll/pitch yer değiştirmiş | Modül montaj yönü — firmware'de değil, **çizimde** düzeltilecek |

---

## 6. Sonrası

Bu test geçince BNO055 ana firmware'e (`F767_FIRMWARE`) girer; oradaki
sürücü **bu sketch'le aynı koddur**, kasıtlı olarak.

Sonraki iş kalibrasyon offset'lerinin saklanması: `CALIB_STAT` 3'e çıktığında
22 baytlık offset bloğu okunup flash'a yazılacak, açılışta geri yüklenecek.
Yoksa araç her açıldığında sekiz çizmek gerekir — sahada olmaz.
