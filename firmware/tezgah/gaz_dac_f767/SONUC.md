# Gaz DAC ölçümü — sonuç (30 Ağustos 2026)

`PA4` = DAC1_OUT1 (CN11-32). Kontrolcü **bağlı değilken**, yüksüz ölçüm.
Referans ölçüldü: **`AVDD` (CN12-7) = 3,28 V** → sketch'teki `VREF` bu değere alındı.

## Ölçülen

| Nokta | ham | Beklenen | **Ölçülen** | Fark |
|---|---|---|---|---|
| Taban | 0 | 0 mV | **0,04 V** | — |
| Rölanti 0,80 V | 998 | 799 mV | **0,79 V** | ≤10 mV (metre çözünürlüğü) |
| %30 güç 1,82 V | 2272 | 1819 mV | **1,82 V** | ✅ |
| %45 güç 2,33 V | 2908 | 2329 mV | **2,33 V** | ✅ |
| Tavan | 4095 | 3280 mV | **3,24 V** | doyum |

**Kalibrasyon doğrulandı.** `ham = (volt / 3.28) * 4095` formülü çalışma bandının
tamamında metre çözünürlüğü içinde tutuyor. Eski `THROTTLE_DAC_VREF 5.0` ve
MCP4725 sayıları kesin olarak geçersiz.

## Tampon payı tahminden çok daha iyi

Datasheet'in kötü durumu ~200 mV; ölçülen **iki uçta da 40 mV**:
taban 0,04 V · tavan 3,24 V (VDDA 3,28). İki ucun simetrik olması okumanın
tutarlı olduğunu da gösteriyor.

🔴 **Erişilen gaz aralığı %67 değil, %72:**
`(3,24 − 0,80) / (4,20 − 0,80) = %72`
Önceki `~3,1 V → %68` tahmini ölçümle güncellendi.

**%45 güçte tavana 0,91 V pay kalıyor** — çalışma noktası doyumdan uzak.

## Op-amp kararı sağlamlaştı

Kazanç zaten %28'lik kullanılmayan bant içindi, ölçümle %28'e indi.
Bedeli gaz hattında *kaçak gaz* arıza kipi olurdu. Dahili DAC'ın en kötü arızası
"çıkış sıfırda kalır" = araç durur. **Op-amp konmuyor**, karar değişmedi.

## ⚠ Açık kalan: yüklü ölçüm

Bu ölçüm **yüksüz**. Kontrolcü bağlanınca girişin yükü çıkışı aşağı çeker;
29 Ağustos'ta gaz ucunun boşta **0 V** okunması içeride pull-down olduğunu
gösterdi (pull-up yok), yani düşüm küçük olmalı — **ama ölçülmedi.**
Kontrolcü bağlandığında **2,33 V noktası tekrar ölçülecek**; çalışma noktasında
sapma varsa kalibrasyon oradan düzeltilir, tavandan değil.

## Firmware notu

Kesme durumunda DAC'a **0 değil `THROTTLE_V_MIN` = 0,80 V (ham 998)** yazılacak.
Gerekçe DAC'ın tabanı değil kontrolcü: 0 V "gaz kablosu koptu" arızası
tetikleyebilir. DAC 0,04 V'a inebiliyor, ama inmesini istemiyoruz.
