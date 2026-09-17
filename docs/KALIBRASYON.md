# Kalibrasyon — sabitler ve nasıl ölçüldükleri

Sürüş kartındaki hiçbir kalibrasyon sabiti koda gömülü değildir. Hepsi çalışma
anında `0x09` paketiyle yazılır, kalıcı bellekte tutulur ve `0x3E` ile geri
okunur. Bu ayrım bilinçlidir: **sabit değişince firmware yeniden derlenmez**,
dolayısıyla sahada ölçüp sahada yazabilirsiniz.

> **Bu depoda ölçülmüş değerler yoktur.** Aşağıdaki yordamlar, aynı sabitleri
> kendi aracınızda üretmeniz için yeterlidir. Yer tutucu dosya:
> [`kalibrasyon.ornek.h`](../kalibrasyon.ornek.h)

---

## 1. Tekerlek çevresi ve dişli oranı

Enkoder tekerin kendisine değil, aktarma organının bir yerine bağlı. Dolayısıyla
"tur başına sayım" tek başına yetmez; iki sabit gerekir.

**Yordam.** Aracı düz bir zeminde, yerden ölçülmüş bilinen bir mesafede
(en az 10 m — kısa mesafede tekerlek kayması oranı bozar) elle ittirin. Başlangıç
ve bitişte `0x30` sayımını okuyun.

```
metre başına sayım = (bitiş − başlangıç) / mesafe[m]
```

Çevre ve dişli oranını ayrı ayrı istiyorsanız, tekerleği yerden kesip **tam bir
tur** çevirin: o turdaki sayım dişli oranını, tekerin yere bastığı iz uzunluğu
da yuvarlanma çevresini verir. Yuvarlanma çevresi geometrik çevreden küçüktür —
lastik basınç altında yassılır, hesapla bulmayın, **ölçün**.

## 2. Direksiyon oranı ve sınırı

Direksiyon kolonu ile tekerlek açısı arasında bir redüksiyon var. Kolon
derecesini teker derecesine çeviren oran ölçülür:

**Yordam.** Kolonu bilinen bir adım sayısı kadar döndürün, ön tekerleğin açısını
açıölçerle ölçün. Oran = kolon derecesi / teker derecesi.

⚠ **Homing yok.** Kart açılışta direksiyonun nerede olduğunu bilmez; bulunduğu
yeri sıfır kabul eder. Bu yüzden her açılışta direksiyon **fiziksel olarak
merkezde** bırakılmalıdır. Sınır (`ayar 11`) bu sıfıra göre uygulanır, mutlak
bir referansa göre değil.

**İşaret (`ayar 5`) ayrı bir sabittir.** Oranı doğru ölçüp işareti ters
bırakmak, aracın komuta ters yöne dönmesiyle sonuçlanır. İlk testte tekerleği
yerden kesin.

## 3. Gaz

Gaz kumandası bir DAC üzerinden analog gerilim olarak sürülür. Hız ile gerilim
arasındaki ilişki **açık döngüdür** — kart hız geri beslemesiyle düzeltme
yapmaz, sadece istenen hızı gerilime çevirir.

Bu iki sabit gerekir:

- **Kalkış gerilimi:** aracın hareket etmeye başladığı gerilim. Altındaki her
  değerde motor döner ama araç durur; sürücü bunu "gaz çalışmıyor" sanır.
- **Hız–gerilim eğimi:** kalkış üstünde, hız komutunu gerilime çeviren eğim.

**Yordam.** Aracı kaldırın (teker yerden kesik), geriliği kademe kademe artırıp
enkoder hızını `0x31`'den okuyun. Yere indirdiğinizde eğim değişir — yüklü
ölçüm ayrıca yapılmalıdır.

⚠ **Hızın açık döngü olduğunu bilmeyen bir üst katman, yokuşta hız düştüğünde
kartı suçlar.** Kapalı döngü isteniyorsa üst bilgisayarda kurulmalıdır; kart
ona `0x31` ile gerçek hızı zaten veriyor.

## 4. Fren

Fren bir motorla kuvvet uygulanarak çekilir ve **mandallı** bir stall koruması
vardır: aynı yönde eşik süreyi aşan komut, motoru korumak için kilitlenir.

⚠ **Kilidi yön *değişimi* değil, yön alanının *değişmesi* çözer.** `0` da bir
yöndür — komutu bir tik `0`'a çekmek kilidi açar. Ters yöne komut vermek
gerekmez. Bu, saatlerce "fren bozuldu" diye aranan bir davranıştı.

Ölü bölge ayrıca ölçülmelidir: belirli bir binde değerinin altındaki komutlar
frende hiçbir hareket üretmez.

## 5. IMU

IMU, `0x33`'te kendi `CALIB_STAT` baytını aynen yayınlar. Dört alan da 3
olmadan yönelim verisine güvenilmez.

**Yordam.** Manyetometre için aracı yatay düzlemde yavaşça 360° çevirin;
jiroskop için birkaç saniye tamamen hareketsiz bırakın; ivmeölçer için altı
yüzün her birine yaklaşık 2 saniye getirin.

⚠ Kalibrasyon **her açılışta sıfırlanır** ve araç metal bir yapının üstünde
durduğu için manyetometre nadiren 3'e çıkar. Yönelime bağlı kritik davranış
kurulacaksa, önce `0x33`'ün ikinci baytı izlenmelidir.

---

## Doğrulama refleksi

Her sabiti yazdıktan sonra `0x3E` ile **geri okuyun**. Yazma paketinin
gitmediği, ölçeklendiğinde taştığı ya da yanlış kimliğe düştüğü durumlar
sessizdir — tek uyarı, geri okunan değerin yazdığınızdan farklı olmasıdır.
