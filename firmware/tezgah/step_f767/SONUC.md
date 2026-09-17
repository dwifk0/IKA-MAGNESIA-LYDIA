# Step motor tezgah testi — sonuç (31 Ağustos 2026)

23IP65-20 + DM860H + FLE57 1:10 · NUCLEO-F767ZI · **ortak anot, açık drenaj**

## ✅ Motor çalıştı, 20000 Hz'e kadar adım kaçırmadı

| | Değer |
|---|---|
| Mikroadım | **1600** (DIP SW5–SW8 `on·off·on·on`) |
| Adım / çıkış turu | 16.000 → 44,4 darbe/derece |
| İvme | **30.000 Hz/s** |
| **Temiz çıkılan en yüksek hız** | **20.000 Hz** |
| Karşılığı | **450 °/sn** çıkış milinde · motor **750 RPM** |

🔴 **Gerçek kaçırma eşiği BULUNAMADI** — sketch'in tavanı 20000 ve orada hâlâ
temizdi. Yani eşik ≥ 20000. Daha yükseğini aramaya gerek görülmedi.

## Faz akımı belirleyiciydi

İlk denemede DIP `SW1-3 = off·on·on` (**2,57 A REF**) ile başlanmıştı — kasten
düşük. O ayarda 12000 Hz "anca anca" dönüyordu.

Tasarım değerine (`off·on·off` = **4,86 A REF / 5,83 A tepe**) alınınca
20000 Hz sorunsuz geçti. Motorun anması 5,0 A; yani önceki ayar yaklaşık
**yarım akımdı** ve 450+ RPM'de step motorun momenti zaten düşerken bir de
akım kısılıydı.

## Mikroadım hız kazandırmıyor — yanlış çıkarım düzeltildi

Ara aşamada "mikroadımı 800'e indirirsek aynı Hz'te iki kat hız, motor devri
değişmez" denmişti. **Yanlış.** Belirli bir çıkış hızı için motor devri her
mikroadımda aynıdır:

| Mikroadım | 270 °/sn için | Motor |
|---|---|---|
| 1600 | 12.000 Hz | 450 RPM |
| 800 | 6.000 Hz | **450 RPM** |
| 400 | 3.000 Hz | **450 RPM** |

Mikroadım yalnız **gereken darbe hızını** değiştirir. Darbe hızı bizde zaten
sıkıntı değildi (sürücü 40 kHz'e kadar, F767 rahat), dolayısıyla düşürmenin
faydası yok — sadece hareketi kabalaştırırdı. **1600'de kalındı.**

## 🔴 Bu sayı OLDUĞU GİBİ firmware'e yazılmaz

**Test yüksüz.** Direksiyon miline bağlanınca gerçek yük gelecek ve 750 RPM'de
moment zaten düşükken **adım kaçırma çok daha erken başlayacak.**

✅ **Karar (kullanıcı, 31 Ağu): ayar direksiyon sistemine takıldıktan sonra
tekrar yapılacak.** Firmware'e şimdilik korunaklı bir ara değer yazıldı.

⚠ Yüklü testte tekrar bakılacaklar:
- Kaçırma eşiği (işaret testiyle, yine bantla)
- İvme — yük ataleti eklenince tavandan önce ivme sınırlar
- Motor ısısı: `SW4 = on` olduğu için **duruşta da 4,86 A** çekiyor ve
  direksiyon çoğu zaman duruyor olacak

## Kalanlar

- [ ] **Referans (homing) mikro switch'i hâlâ yok ve listede de yok.** Step açık
      çevrim, E6B2 traksiyon milinde → direksiyonda **hiç geri besleme yok**,
      açılışta konum bilinmiyor. Bu test onu çözmez.
- [ ] `DIR` seviyesinin hangi yönü verdiği — araçta tekerlek yönüne bakılarak
      kesinleşecek.
- [ ] Yazılımda sanal açı limiti (referanstan ±N adım), mekanik uca dayanıp
      adım kaçırmasın.
- [ ] ⚠ **Akımdan stall algılama denenmeyecek.** Step motor dururken de nominal
      akımını çeker; DC motordaki temiz akım sıçraması olmaz.
