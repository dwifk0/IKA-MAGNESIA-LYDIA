# MCP4725 tezgah ölçümü — sonuç (30 Ağustos 2026)

I2C1 · `SCL = PB8` · `SDA = PB9` · adres **`0x60`** · `VOUT` kontrolcüye bağlı değil.

## Çalışıyor — tam gaza ulaşıyor

| Nokta | ham | Beklenen | **Ölçülen** |
|---|---|---|---|
| EEPROM açılış değeri | 655 | 719 mV | **0,71 V** |
| Rölanti 0,80 V | 728 | 800 mV | **0,79 V** |
| %45 güç 2,33 V | 2120 | 2329 mV | **2,32 V** |
| **Tam gaz 4,20 V** | 3821 | 4198 mV | **4,20 V** ✅ |

Dördü de metre çözünürlüğü içinde. **Aralığın tamamı doğrusal, %100 gaz erişilebilir.**
Dahili DAC'ın tavanı %72'ydi ([[gaz_dac_f767/SONUC.md]]).

## 🔴 EEPROM tuzağı gerçekti — sayıyla

İlk okumada **EEPROM = 2048 → 2,50 V** çıktı (fabrika varsayılanı, yarı ölçek).
`DAC` yazmacı da 2048 okundu, yani çip gücü görür görmez onu çıkışa basmıştı.

Gaz karşılığı **%50**, ölçülen hız doğrusuyla **≈1,6 m/s (5,8 km/h)**.
`VOUT` kontrolcüye bağlı olsaydı **anahtar çevrildiği anda araç kendi kendine
hareket ederdi**, firmware ilk satırını çalıştırmadan.

✅ EEPROM'a **655** yazıldı, geri okumayla doğrulandı. Çip artık her açılışta
rölantiden başlıyor.

**655 seçimi tesadüfen dayanıklı:** VDD 4,5–5,1 V arasında ne olursa olsun çıkış
0,72–0,81 V arasında kalıyor, hareket eşiği 0,96 V'un hep altında. VDD kesinleşse
bile değiştirmeye gerek yok.

## 🔴 Açık: VDD = 4,50 V, kaynağı belirlenmedi

Beklenen 5,00 V'tu, ölçülen **4,50 V**. Çip doğru çalışıyor — hata varsayımdaydı.

**Bu bir kalibrasyon detayı değil, mimari bir zayıflık:** MCP4725'in referansı
kendi beslemesidir. Besleme oynarsa **aynı `ham` değeri farklı gaz voltajı
üretir** → gaz, 5 V rayındaki her dalgalanmayla sürüklenir.
Dahili DAC'ta bu risk yoktu: `VDDA` kartın kendi regüle rayı, bugün 3,28'de sabit durdu.

Tavan payı da dar: VDD 4,50, tam gaz 4,20 → **0,30 V.** VDD 4,20'nin altına
düşerse tam gaz erişilemez olur.

**Yapılacak:** 4,50 V nereden geliyor (kart 5 V pini / modül diyodu / UBEC),
ve **yük altında** ne kadar oynuyor. Araçta bu uç UBEC'e gidecek; regüleli 5 V
oradan gelmeli.

## Karar tablosu

| | Dahili DAC (`PA4`) | MCP4725 |
|---|---|---|
| Tavan | 3,24 V = **%72** | 4,20 V = **%100** |
| Referans | `VDDA` 3,28 V — kart regülatörü, kararlı | **kendi beslemesi** — 4,50 V, kaynağı belirsiz |
| Haberleşme | yok, doğrudan pin | I²C (100 ms zaman aşımı, Mega'daki gibi kilitlemiyor) |
| Açılış anı | 0 V | EEPROM değeri (**şimdi rölanti**) |
| En kötü arıza | ne olursa olsun **%72 tavan fiziksel** | besleme sürüklenmesi / EEPROM |
| Ek parça | yok | modül + 2 pull-up (5 V'a) |

## ⚠ Hâlâ cevaplanmadı: bu kazanç gerçek mi

**3,24 V'un üstünde araç gerçekten hızlanıyor mu bilinmiyor.** Ölçülen hız doğrusu
2,33 V'ta bitiyor. Üstünü kontrolcünün akım sınırı, BMS'in 60 A kesmesi ya da
motorun 48 V'taki boş devri sınırlıyor olabilir.

Bugün kanıtlanan şey **DAC'ın 4,20 V verebildiği** — aracın o voltajda daha hızlı
gittiği değil. Karar, 25–30 m'lik geçişlerle 2,33 · 2,60 · 2,90 · 3,24 V ölçülüp
doğrunun yatıp yatmadığı görülmeden verilmemeli.

Şema: (şema depoda yayımlanmadı)
