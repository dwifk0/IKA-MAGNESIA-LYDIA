# Gaz DAC → `PA4` · tezgah testi

**Karta hiçbir şey bağlanmıyor.** Tek gereken multimetre.
⚠ **Kontrolcü bağlı OLMAMALI** — bağlıyken bu sketch gerçekten gaz verir.

| Ölçüm | Nokta |
|---|---|
| DAC çıkışı | **`PA4` = CN11-32** ↔ GND (CN8-11) |
| Referans (VDDA) | **3V3 = CN8-7** ↔ GND |

## Neden yeniden ölçülüyor

MCP4725 düşürüldü, gaz artık dahili DAC'ta. Eski `THROTTLE_DAC_VREF 5.0` ve
MCP4725 kalibrasyonu **geçersiz**. Yeni formül:

```
ham = (volt / VREF) * 4095        VREF = VDDA ≈ 3,3 V
```

Kontrolcünün aralığı **0,80 V = %0 · 4,20 V = %100**:

| | Volt | ham |
|---|---|---|
| Rölanti | 0,80 | 993 |
| %30 güç | 1,82 | 2259 |
| %45 güç (Mega döneminde kullanılan) | 2,33 | 2892 |
| **DAC tavanı** | **~3,1** | 4095 → **tam gazın ~%68'i** |

Çıkış `VREF+`'i aşamaz; `VREF+` lehim köprüsüyle `VDDA`'ya (3,3 V) bağlı ve
MCU'nun mutlak sınırı 3,6 V. **5 V almanın yolu yok.** Tam aralık için ×1,5
op-amp gerekir — bilinçli olarak konmadı (*kaçak gaz* arıza kipi).

## Ölçülecek üç şey — hiçbiri bugüne kadar ölçülmedi

1. **VDDA gerçekte kaç volt?** 3V3–GND. Formülün tabanı bu; 3,30 değilse
   sketch'teki `VREF` düzeltilecek ve bütün ham değerler kayar.
2. **DAC'ın gerçek tavanı.** "TAVAN" düğmesi (ham 4095) → ölç.
   3,30 çıkmayacak; tamponlu çıkış ~3,1 V'ta doyar. **%68 rakamı buradan doğrulanır.**
3. **DAC'ın gerçek tabanı.** "TABAN" düğmesi (ham 0) → ölç.
   Tamponlu çıkış 0 V'a inmez, ~0,2 V bekleniyor.
   ⚠ Kritik değil: rölanti zaten 0,80 V, tabanın altında kalmıyoruz.
   Ama **kontrolcüye 0 V vermek "gaz kablosu koptu" arızası tetikleyebilir**,
   o yüzden firmware kesme durumunda 0 değil **0,80 V** yazacak.

## Sıra

```bash
cd ~/ika/testler/gaz_dac_f767 && ~/.local/bin/pio run
#   -> firmware.bin  →  NOD_F767ZI diskine surukle
python.exe gaz_pano.py --port COM6      # http://localhost:8777
```

Sonra sırayla: **TABAN → rölanti → %30 → %45 → TAVAN**, her adımda multimetreyi oku.

## Beklenen sapma

Ölçülen ile beklenen arasında **±%2**'ye kadar fark normal (VDDA toleransı +
DAC doğrusalsızlığı). Daha büyük bir sapma varsa önce VDDA'yı doğrula;
o doğruysa DAC'ın uç bölgelerinde doyum var demektir, çalışma bandı
(0,80–2,33 V) zaten uçlardan uzak.

⚠ Ölçüm **yüksüz** yapılıyor. Kontrolcü bağlanınca girişin yükü çıkışı biraz
aşağı çekebilir; 29 Ağustos'ta gaz ucunun boşta **0 V** ölçülmesi içeride
pull-down olduğunu gösterdi (pull-up yok), yani düşürme etkisi küçük olmalı —
ama **kontrolcü bağlıyken bir kez daha ölçülmeli.**
