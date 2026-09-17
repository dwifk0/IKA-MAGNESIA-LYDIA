# Tezgah testleri — parçalar geldiğinde

Her parça **araca takılmadan önce, masada, tek başına** doğrulanır.
Her testin üç parçası var: **sketch** (kart), **pano** (tarayıcı), **BAGLANTI.md** (kablo).

> **Neden tek tek:** araca beş şey birden takıp "çalışmıyor" demek, beş
> arızayı birbirine karıştırmak demektir. İKA'da bu zaten iki kez yaşandı —
> sponsor enkoderinin ölü B kanalı ve gaz DAC arızası, ikisi de ancak tezgahta
> tek başına denenince görüldü.

---

## Hızlı tablo — "şunu bağlıyorum" dediğinde

> 🔴 **30 Ağustos 2026 — Mega tasarımdan çıktı.** Aşağıdaki Mega testleri
> **geçersiz**: kart yanlış, ikisinde topoloji de değişti (E-STOP tek NC +
> `PF14`'e indi; step ortak anot oldu, 74HCT244 iptal). F767'ye taşınacaklar.

| Parça | Kart | Klasör | Pano | Port | Durum |
|---|---|---|---|---|---|
| **F767 buton/LED** | Nucleo-F767ZI | `f767_buton_led/` | — | — | ✅ **30 Ağu: ÇALIŞTI** |
| **FlySky iBUS** | Nucleo-F767ZI | `ibus_f767/` | `ibus_pano.py` | 8776 | ✅ **30 Ağu: ölçüldü** (`SONUC.md`) |
| **Gaz DAC** | Nucleo-F767ZI | `gaz_dac_f767/` | `gaz_pano.py` | 8777 | ✅ **30 Ağu: ölçüldü** (`SONUC.md`) |
| **MCP4725 harici DAC** | Nucleo-F767ZI | `mcp4725_f767/` | — | — | ✅ **30 Ağu: ölçüldü**, %100 gaz (`SONUC.md`) |
| **Uçtan uca gaz zinciri** | Nucleo-F767ZI | `gaz_zinciri_f767/` | — | — | ✅ **30 Ağu: çalıştı**, SwA kesmesi doğrulandı (`SONUC.md`) |
| **BNO055 IMU** | Nucleo-F767ZI | `bno055_f767/` | `bno_pano.py` | 8772 | denenmedi |
| **E6B2 enkoder** | Nucleo-F767ZI | `e6b2_f767/` | `enk_pano.py` | 8773 | denenmedi |
| **Step motor + DM860H** | Nucleo-F767ZI | `step_f767/` | `step_pano.py` | 8771 | ✅ **31 Ağu: ÇALIŞTI**, yüksüz 20 kHz temiz (`SONUC.md`) |
| **Acil stop** | Nucleo-F767ZI | `estop_f767/` | `estop_pano.py` | 8774 | ✅ **taşındı**, denenmedi |
| **Fren + BTS7960B** | Nucleo-F767ZI | `fren_f767/` | `fren_pano.py` | 8775 | ✅ **taşındı** (30 Ağu, derlendi), denenmedi |

**Yükleme yolu doğrulandı (30 Ağu):** kart `CN1` USB'den takılınca `D:` sürücüsü
`NOD_F767ZI` olarak açılıyor, `.bin` sürüklenince yazılıyor (`FAIL.TXT` yoksa
başarılı), sanal seri port **COM6**. ST-LINK firmware `V2J39M27`.

Hepsi aynı şekilde çalışıyor:

```bash
# 1. Kabloyu BAGLANTI.md'ye bakarak çek
# 2. Sketch'i yükle   (pano AÇIKSA önce kapat — portu tutuyor)
# 3. Panoyu aç
python.exe <pano>.py --port COM15
#    → http://localhost:<port>        telefondan: http://<PC-IP>:<port>
```

Kart yokken arayüzü görmek için: `--demo`
*(uydurma veri üretir, panonun tepesinde "DEMO" yazar)*

---

## Yükleme komutları

**STM32 (Nucleo-F767ZI)** — kart üstünde ST-LINK var, **harici dongle gerekmez**
(BlackPill'de bozulan dongle sorunu bu kartta yok):

```bash
~/ika/testler/yukle.sh bno055_f767      # derler + yukler, tek komut
```

Elle yapmak istersen:

```bash
cd ~/ika/testler/<test>
~/.local/bin/pio run
SRC=$(wslpath -w .pio/build/nucleo_f767zi/firmware.bin)
cd /mnt/c && powershell.exe -NoProfile -Command "Copy-Item -LiteralPath '$SRC' -Destination 'D:\\' -Force"
```

🔴 **İki şey WSL'de ÇALIŞMAZ, ikisi de 30 Ağustos'ta denendi:**

| Ne | Neden |
|---|---|
| `pio run -t upload` | WSL'de USB geçişi yok (`/dev/bus/usb` mevcut değil), ST-LINK görünmez |
| `cp .bin /mnt/d/` | WSL kaldırılabilir sürücüyü **otomatik bağlamıyor**; `/mnt/d` root'a ait boş bir klasör olarak duruyor → `Permission denied`. `mount` çıktısında hiç yok |

**Çalışan yol:** kopyalamayı Windows'a yaptırmak. Kart `CN1`'den takılınca
Windows'ta `D:` = `NOD_F767ZI` açılır, `.bin` oraya yazılınca yüklenir.
Doğrulama: **`FAIL.TXT` oluşmadıysa ve `.bin` sürücüde kalmadıysa başarılı.**
(Sürücü yazma bitince yeniden numaralandırılır, geriye `DETAILS.TXT` +
`MBED.HTM` kalır — bu normaldir.)

**Mega** — Windows tarafından, WSL'den:

```bash
ACLI=/mnt/c/Users/ahmet/AppData/Local/Temp/acli/arduino-cli.exe
"$ACLI" compile --fqbn arduino:avr:mega  <sketch klasoru>
"$ACLI" upload  --fqbn arduino:avr:mega -p COM11  <sketch klasoru>
```

COM numarası: Aygıt Yöneticisi. Nucleo **"STMicroelectronics STLink Virtual
COM Port"** olarak görünür, Mega **"Arduino Mega 2560"**.

---

## Her testte asıl aranan şey

Üçünün de kolay bir "yeşil ışığı" var ve **üçünde de o yeşil ışık yalan
söyleyebilir.** Panolar bilerek asıl soruyu öne koyuyor:

| Test | Kolay ama YETERSİZ | **Asıl soru** |
|---|---|---|
| BNO055 | "veri akıyor" | **Kalibrasyon `sys` = 3 mü?** Değilse yaw sürüklenen bir tahmindir |
| E6B2 | "sayım artıyor" | **Ters çevirince geri gidiyor mu?** Tek kanal da sayım artırır |
| Step | "sayaç doğru diyor" | **Mildeki işaret başladığı yere döndü mü?** Açık çevrimde sayaç kendini yalanlayamaz |
| E-STOP | "basınca duruyor" | **Bırakınca kurtuluyor mu?** Ve kopuk kablo tetikliyor mu? |
| Fren | "aktüatör hareket ediyor" | **Hangi yön frene BASIYOR?** Ters kalırsa "fren uygula" freni bırakır |

**E-STOP ve fren testleri en aciller** — ikisi de ölçülmemiş sayılara dayanan
güvenlik kodu içeriyor. E-STOP'un polaritesi 24 Ağustos'ta çevrildi ve hiç
denenmedi; `BRAKE_STALL_MS` 2500 ms bir tahmin ve frende başka koruma yok.

---

## Ortak altyapı

`tezgah_pano.py` — panoların hepsi bunu kullanıyor. Yeni bir test eklerken
arayüz/sunucu/seri köprü yazılmayacak, sadece alanlar ve düğmeler tanımlanacak:

```python
from tezgah_pano import Alan, Dugme, Pano

Pano(
    baslik="Fren aktüatörü",
    alanlar=[Alan("pwm", "PWM", grafik=True),
             Alan("stall", "Stall", iyi="v===0 ? null : false")],
    dugmeler=[Dugme("f", "Fren uygula", sayi=True), Dugme("d", "DUR", tehlike=True)],
    varsayilan_port="COM11", http=8774,
).calistir()
```

**Firmware tarafındaki tek kural:** ölçüm satırı `D ` ile başlar ve
`anahtar=tamsayı` çiftlerinden oluşur. Başka her satır ham kütüğe düşer
(mesajlar, uyarılar, hata açıklamaları oraya yazılır).

⚠ **Ondalık gönderme.** newlib-nano'da float printf kapalı; sayıyı tam sayı
olarak yolla (`yaw=1234`), panoda `olcek=10` ile 123,4'e çevrilir.

---

## Eski testler

`enkoder_saglik_stm32/`, `as5600_test/`, `gaz_test/`, `acs712_test/`,
`direksiyon_role/`, `step_test/` — tarihsel, kendi panolarıyla.
`enkoder_saglik_stm32/enkoder_pano.py` bu ortak motorun atasıdır;
yenileri onun genelleştirilmiş hâlini kullanıyor.

⚠ `direksiyon_role/` ve `step_test/` **röle dönemine ait** — direksiyon
24 Ağustos'ta step motora geri döndü, `step_mega/` onun yerini aldı.
