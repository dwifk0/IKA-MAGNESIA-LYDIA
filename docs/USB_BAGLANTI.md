# USB / bağlantı — dış oturum bulgularının değerlendirmesi

**Tarih:** 27 Ağustos 2026
**Kaynak dosya:** `genel/USB_VE_BAGLANTI_BULGULARI_disoturum.md`
(dış Claude oturumundan geldi, `~/Downloads`'tan kopyalandı)

---

## 0. ASIL PROBLEM — dosya bunu ıskalamış

**Kullanıcının şikâyeti:** Jetson açıldığında **bazen `mega`, bazen `turret`
UNO'su, bazen nişan kamerası — bazen ikisi birden — hiç görünmüyor.** Aralıklı.

Dış dosya her şeyi "otonom modda fps düşüyor"a ve USB bant genişliğine bağlamış.
Asıl şikâyet bu değil, ve **kullanıcı belirtinin manuel modda da görüldüğünü
doğruladı** — yani dosyanın "manuelde sorun görünmüyor" varsayımı yanlış.

Ayrıca izokron rezervasyon akış açılırken bir kez yapılır, mod değişince
değişmez. Yığın systemd ile açılışta tümüyle kalkıyor; mod yalnız bir çalışma
zamanı durumu. **Bant genişliği bu belirtiyi açıklayamaz.**

### Teşhis — cihaz kaybolduğu anda çalıştırılacak

```bash
lsusb
ls -l /dev/mega /dev/turret /dev/kamera_*
```

| Sonuç | Anlamı |
|---|---|
| `lsusb`'de **yok** | Donanım/güç. Powered hub akımı boot'ta hepsine yetmiyor olabilir — `99-ika.rules.yedek` zaten *"Nisan kamerasi ... powered hub uzerinden baglanmali!"* diyor. "Bazen ikisi de olmuyor" bunu işaret ediyor |
| `lsusb`'de **var**, symlink yok | udev. `mega`/`turret` kuralları `KERNELS=="1-2.4.1"` / `1-2.4.3` fiziksel yoluna bağlı |

⚠ `lydia_startup.sh` `[ -e /dev/kamera_on ]` ile bakıp yoksa yalnız uyarı basıp
geçiyor — udev geç oturursa cihaz fiilen oradayken "yok" sayılıyor.

**Ölçüm yapılmadan kablo veya kural değiştirme.**

---

## 1. Doğrulanan kısımlar

- **Sahte odometri gerçek** — `lydia_startup.sh`, `static_transform_publisher
  0 0 0 0 0 0 odom base_footprint`.
- **Başlatma sırası gerçek** — `seri_kopru` sensörlerden **~47 sn** sonra
  (8+14+14+5+6). Komut hattı ve E-STOP bu kadar geç geliyor.
- **DDS UDP-only gerçek**, gerekçesi scriptin başında yazılı.
- **`quirks=128` doğru sayı** — `UVC_QUIRK_FIX_BANDWIDTH` = 0x80.
- **CAN reddi doğru**, gerekçeleri sağlam. Satın alma kaleminden çıkarılabilir.

## 2. Olgusal hatalar

1. **"3 webcam + derinlik aynı anda" yanlış.** Script yalnız iki webcam açıyor
   (`kamera_on`, `kamera_nisan`) + OS30A. `kamera_arka` udev'de tanımlı ama
   hiç başlatılmıyor. Dolayısıyla §8'in ölçüm protokolü de kusurlu.
2. **§5.1 ile §7.3 çelişiyor.** §5 "MCU'lar bulk, izokron havuzdan yemiyorlar"
   diyor; §7.3 "kameralar rezerve ediyor, komut hattı artıkla idare ediyor"
   diyor. İkisi aynı anda doğru olamaz. Sırayı değiştirmek yine de doğru —
   gerekçesi 47 sn'lik gecikme.
3. **§2.3 hub varsayımı yanlış kurulmuş** — zaten harici powered hub var, udev
   kuralları da hub altındaki portları kullanıyor.

## 3. Dosyanın kaçırdıkları

- 🔴 **`lydia_otonom` hiç kamera kullanmıyor.** Topic dökümü: `/scan`, `/odom`,
  `/odometry/filtered`, `/imu/data`. Yarışmayı bu paket sürecekse kamera bant
  genişliği otonomi için alakasız.
- 🔴 **LiDAR udev kuralı kırılgan:** yalnız `10c4:ea60` (VID:PID). Elimizde
  CP2102 var (`genel/MEGA_YEDEK_KARTI.md`); Jetson'a takılırsa `/dev/lidar`
  yanlış cihaza gider. HSS'de aynı tuzağa düşüldü.
- **CH340'ta seri numarası yok** — "udev'i `ATTRS{serial}`'e taşı" önerisi
  yürümez. Port yolu kusur değil, doğru çözüm; asıl iş fiziksel etiketleme.

## 4. Katılmadığım öneriler

- **DDS shared memory'yi geri açmak "5 dk, kolay" değil.** Bilinen,
  tekrarlanabilir bir arıza için konmuş çözüm; geri alındığında arıza
  "düğümler birbirini sessizce bulamıyor" biçiminde çıkar. `rm -f
  /dev/shm/fastrtps*` yalnız boot'taki bayat kilidi temizler, koşu ortasında
  sert ölen düğümün bıraktığını değil. Aynı kazanç risksiz alınabilir: fps
  düşür, `compressed` transport, foxglove/dashboard ham görüntü aboneliğini
  kaldır.
- **CSI kamera:** doğru mimari, yanlış zaman. Device tree overlay + JetPack
  kamera sürücüsü işi, kargoya kalan sürede olmaz.

## 5. Sıralama

| # | İş | Neden |
|---|---|---|
| 0 | **Mardin'e hangi paket gidiyor?** | Cevaplanmadan aşağısı doğru önceliğe oturmaz. Dış dosya bunu §10'a gömmüş |
| 1 | **Cihaz kaybolunca `lsusb` + `ls -l /dev/...`** | Asıl problem bu; teşhissiz düzeltme yok |
| 2 | **Odometri köprüsü** (gösterge ucu → Mega kesme pini) | `MANISA_YAPILACAKLAR.md` §E. Hem tökezlemenin muhtemel sebebi hem Nav2'nin ön şartı |
| 3 | `seri_kopru` + LiDAR'ı en öne al | 47 sn'lik E-STOP gecikmesi |
| 4 | Otonom koşuda OS30A gerekli mi? | Gerekmiyorsa kapat — en büyük tüketici |
| 5 | `quirks=128` + fps 10–15 | Ucuz, geri alınabilir, ölçülebilir |
| 6 | udev: etiketle + LiDAR kuralına yol/seri ekle | Seri no peşinde koşma |
| 7 | DDS SHM | Önce `tegrastats` ile ölç; belki hiç dokunma |
