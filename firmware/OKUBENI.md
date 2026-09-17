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
| [`direksiyon_jog.h`](cekirdek/direksiyon_jog.h) | Jog profili, merkeze dönüş, yumuşak sınır, güvenli hız |

Araca özel ölçülmüş sabit yok; hepsi çağıran tarafın verdiği parametreler.
Değerler nasıl ölçülür: [`../docs/KALIBRASYON.md`](../docs/KALIBRASYON.md).

## `test/` — kartsız çalışan 75 test

```bash
cd firmware/test && make
```

```
cerceve                     23 gecti, 0 kaldi
emniyet                     23 gecti, 0 kaldi
ibus                        16 gecti, 0 kaldi
direksiyon                  13 gecti, 0 kaldi
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

Kartın tam firmware'i (kip hakemi, sürüş kararı, telemetri döngüsü, taret
sürüşü) ve derlenmiş ikilileri yayımlanmamıştır. Yayımlanan şey, o firmware'in
**yeniden kullanılabilir ve test edilebilir** parçalarıdır.
