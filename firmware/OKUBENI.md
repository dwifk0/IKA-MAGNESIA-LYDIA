# Firmware

İki parça: kartta koşan **tezgâh testleri** ve ana firmware'den ayrılmış,
platformdan bağımsız **çekirdek modüller**.

```
cekirdek/   Saf mantık — Arduino, HAL, kesme ya da zamanlayıcı yok
test/       Masaüstünde koşan birim testler (kart gerekmez)
tezgah/     Her parçayı tek başına doğrulayan sketch'ler + panolar
```

---

## `cekirdek/` — neden ayrıldı

Ana firmware tek bir dosyada, 2300 satır. Bu kartta çalışırken sorun değil ama
iki şeyi imkânsız kılıyor: **tek bir parçayı ayrı test etmek** ve **başka bir
projede yeniden kullanmak.**

Burada, sürüş mantığının donanıma dokunmayan kısımları ayrı modüllere çıkarıldı.
Hiçbiri `digitalRead`, `millis` ya da `Serial` çağırmıyor — pin okuma ve zaman
dışarıdan parametre olarak geliyor. Sonuç: aynı kod hem kartta koşuyor hem
masaüstünde `g++` ile derlenip test ediliyor.

| Modül | İçerik |
|---|---|
| [`cerceve.h`](cekirdek/cerceve.h) | 8 baytlık çerçeve: kur, çöz, bayt başına toplayıcı, teşhis sayaçları |
| [`ibus.h`](cekirdek/ibus.h) | FlySky iBUS çözücü, yeniden senkron, Hz penceresi |
| [`emniyet.h`](cekirdek/emniyet.h) | Acil stop izleme, arıza kilidi, fren stall mandalı |
| [`kip_hakemi.h`](cekirdek/kip_hakemi.h) | **Sürüş yetkisi kimde:** güvenlik kümeleri, kip geçişi |
| [`gaz_profili.h`](cekirdek/gaz_profili.h) | Manuel gaz tavanı (pot), otonom m/s → volt, kalkış darbesi |
| [`direksiyon_jog.h`](cekirdek/direksiyon_jog.h) | Jog profili, merkeze dönüş, yumuşak sınır, güvenli hız |
| [`taret_jog.h`](cekirdek/taret_jog.h) | Pan/tilt jog, üstel eğri, sınırların birikim üzerinde uygulanması |

Araca özel ölçülmüş sabit yok; hepsi çağıran tarafın verdiği parametreler.
Değerler nasıl ölçülür: [`../docs/KALIBRASYON.md`](../docs/KALIBRASYON.md).

## `test/` — kartsız çalışan 138 test

```bash
cd firmware/test && make
```

```
cerceve                     23 gecti, 0 kaldi
emniyet                     23 gecti, 0 kaldi
ibus                        16 gecti, 0 kaldi
direksiyon                  13 gecti, 0 kaldi
kip hakemi                  36 gecti, 0 kaldi
gaz + taret                 27 gecti, 0 kaldi
```

Bağımlılık yok: `g++`, `make` ve 40 satırlık bir assert başlığı. Derleme
`-Wall -Wextra -Wpedantic -Werror` ile yapılıyor.

Testler yalnız "çalışıyor mu" demiyor, **neden böyle yazıldığını** gösteriyor:

- Fren mandalının **ters yönle değil, yön alanının değişmesiyle** çözüldüğü —
  `0`'ın da bir yön olduğu
- Bozuk bir iBUS çerçevesinin kanal değerlerini **ezmediği**
- Kümülatif sayacın bir kopmayı gizlediği, Hz penceresinin gizlemediği
- iBUS mod kanalının açılışta neden merkezde **bırakılamayacağı** (1500 orta
  kademedir; sistem ilk çerçeve gelmeden kendini yarı otonom sanar)
- Çırpınan bir sağlık bayrağının neden kilide çevrildiği
- Jog hedefinin gerçek konumun çok önüne geçmesinin "bırakınca durmuyor"
  hissini nasıl ürettiği
- **Güvenlik > kumanda > üst bilgisayar** sırasının otonom kipte de
  bozulmadığı — beş güvenlik sebebinden her birinin tek başına yettiği
- "Gazı kes" ile "freni bas"ın neden ayrı kümeler olduğu
- Kip geçişinde komutun neden sıfırlandığı (bayat bir hız komutu aracı
  kendiliğinden kaldırır)
- Gaz tavanı potunun kanalı bozulduğunda neden **tam güce** dönüldüğü,
  sıfıra değil
- Taret açısının neden `float` tutulduğu — tamsayıda 0,6°/tik her tikte
  sıfıra yuvarlanır ve taret hiç kımıldamaz

> Bir test yazarken bir hatam ortaya çıktı: senkron kaybının **her zaman
> kalıcı olmadığını** buldum — veri baytlarından biri `0xAA` değilse toplayıcı
> kendiliğinden toparlanıyor. Test buna göre düzeltildi ve ikisi de artık ayrı
> ayrı doğrulanıyor. Zaman aşımının neden şansa bırakılamayacağı tam olarak bu.

## `tezgah/` — parçalar araca takılmadan önce

Her parça **masada, tek başına** doğrulanır. Her testin üç bileşeni var:
kartta koşan sketch, tarayıcıda açılan pano, ve `BAGLANTI.md` kablo belgesi.

**Neden tek tek:** araca beş şey birden takıp "çalışmıyor" demek, beş arızayı
birbirine karıştırmak demektir. Bu projede iki kez yaşandı — sponsor
enkoderinin ölü B kanalı ve gaz DAC arızası, ikisi de ancak tezgâhta tek başına
denenince görüldü.

Ayrıntı ve sonuç tabloları: [`tezgah/OKUBENI.md`](tezgah/OKUBENI.md).
Testi geçen parçaların klasörlerinde `SONUC.md` var — ölçülen değerler ve
çıkan tuzaklarla.

---

## Burada olmayan

Firmware'in donanıma bağlı katmanı: pin tanımları, zamanlayıcı ve kesme
kurulumu, DAC/PWM sürücüleri, I²C sensör okuyucuları, telemetri döngüsünün
zamanlaması ve derlenmiş ikililer. Bunlar karta özgü ve tek başlarına taşınmaz.

Yayımlanan şey **karar katmanı**: sürüş yetkisinin kime ait olduğu, hangi
durumun hangi aktüatörü nasıl etkilediği, bir komutun nasıl gerilime
dönüştüğü. Araca özel ölçülmüş hiçbir sabit yok — hepsi çağıran tarafın
verdiği parametreler.
