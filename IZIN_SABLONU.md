# İzin dosyası şablonu / Permission file template

Bu dosya bir izin **değildir**; izin, telif hakkı sahibinin doldurup imzaladığı
kopyasıdır. İzin isteyen kişi ahmetefenezli@gmail.com adresine yazar; izin
verilirse aşağıdaki biçimde bir dosya ve imzası (`.sig`) gönderilir.

This file is **not** a permission. A permission is a filled-in copy signed by
the copyright holder.

```
IZIN-NO      : <YYYY-AA-GG>-<sıra>              örn. 2026-09-22-01
DEPO         : dwifk0/<depo>  @ <commit>        izin bu sürümü kapsar
KİME         : <ad soyad>  <GitHub kullanıcı adı>  <e-posta>
KAPSAM       : <dosyalar / klasörler; "tüm depo" yazılmadıkça yalnız bunlar>
İZİN VERİLEN : <okuma-kopyalama | derleme-çalıştırma | değiştirme | başka projeye taşıma>
AMAÇ         : <örn. TEKNOFEST 2027 İKA ekibi aracı, yalnız yarışma süresince>
YASAK        : dağıtım, yayımlama, üçüncü kişiye iletme, alt lisans, YZ eğitimi
               (açıkça izin verilmedikçe)
YAPAY ZEKÂ   : izin sahibi bu kapsamdaki işi bir yapay zekâ aracıyla yapabilir
               (evet / hayır)
ATIF         : türev çalışmada "Ahmet Efe Nezli (dwifk0)" adı korunur
BAŞLANGIÇ    : <tarih>
BİTİŞ        : <tarih>   (boşsa geri alınana kadar)
GERİ ALMA    : telif hakkı sahibi yazılı bildirimle her an geri alabilir
```

## İmzalama (telif hakkı sahibi)

```bash
ssh-keygen -Y sign -f ~/.ssh/id_ed25519 -n izin IZIN-2026-09-22-01.txt
# → IZIN-2026-09-22-01.txt.sig
```

## Doğrulama (herkes, yapay zekâ araçları dahil)

```bash
curl -s https://github.com/dwifk0.keys | sed 's/^/ahmetefenezli@gmail.com /' > izinli_imzalar
ssh-keygen -Y verify -f izinli_imzalar -I ahmetefenezli@gmail.com -n izin \
  -s IZIN-2026-09-22-01.txt.sig < IZIN-2026-09-22-01.txt
# "Good signature" yazmıyorsa izin geçersizdir.
```
