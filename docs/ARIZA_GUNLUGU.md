# Arıza Günlüğü

Sahada gerçekten yaşanmış arızalar ve nasıl bulundukları. Hepsinin ortak yanı
şu: hiçbiri hata mesajı vermedi. Sistem çalışıyor gibi göründü, sadece yanlış
çalıştı — teşhisi zor olan tek arıza sınıfı budur.

---

## 1. Fren "bozuldu", aslında kilitlendi

**Belirti.** Fren bir süre düzgün çalışıyor, sonra hiç cevap vermiyor. Güç
kesip açınca düzeliyor.

**Sebep.** Fren motorunda, aynı yönde eşik süreyi aşan komutu kilitleyen bir
stall koruması var. Koruma çalışıyordu; asıl sorun kilidin **nasıl açıldığıydı**.

**Bulgu.** Kilidi yön *değişimi* değil, **yön alanının değişmesi** açıyor. `0`
da bir yöndür — komutu bir tik `0`'a çekmek yeterli, ters yöne komut vermek
gerekmiyor. Üst katman sürekli aynı yönde komut bastığı için kilit hiç
açılmıyordu.

**Ders.** Bir kilidin *kurulma* şartını belgeleyip *açılma* şartını
belgelememek, kilidi görünmez bir arızaya çeviriyor.

## 2. Gaz komutu gidiyor, araç gitmiyor

**Belirti.** Telemetride gaz değeri doğru görünüyor, araç kımıldamıyor.

**Sebep.** Gaz DAC'i cevap vermiyordu. Kart komutu üretiyor, yazıyor, telemetride
raporluyordu — ama çevre birimi hattı almıyordu.

**Bulgu.** Bir çıkışın "yazıldı" olması "uygulandı" demek değil. Buna karşı
`0x80` hata bayrağı eklendi: DAC cevap vermediğinde bayrak kalkıyor ve arıza
sessiz olmaktan çıkıyor.

**Ders.** Her aktüatör için "komut gitti mi" ile "komut uygulandı mı" ayrı
sorulardır. İkincisini soran bir denetim yoksa, ilki yanıltıcıdır.

## 3. Röle 3,3 V'la çekmiyor

**Belirti.** Röle modülü tezgâhta çalışıyor, kartla çalışmıyor.

**Sebep.** Modül 5 V mantık için tasarlanmış; mikrodenetleyicinin 3,3 V HIGH
seviyesi tetikleme eşiğini geçmiyor.

**Bulgu.** Çözüm gerilim yükseltmek değil, **açık drenaj** sürmek oldu: pin
yalnız LOW çeker, HIGH'ı modülün kendi pull-up'ı üretir.

**Ders.** Farklı mantık seviyeli iki kartı birleştirirken "aynı GND" yeterli
değil; eşik gerilimleri ayrıca kontrol edilmeli.

## 4. Seri port bir gün sonra başka cihaza bağlandı

**Belirti.** Dün çalışan başlatma betiği bugün yanlış cihaza bağlanıyor.

**Sebep.** `/dev/ttyUSB0` numaraları takılma sırasına göre dağıtılıyor. Araçta
aynı USB-seri çipinden birden fazla var ve bazılarının seri numarası aynı —
yani `by-id` bile onları ayırmıyor.

**Bulgu.** Seri numarası benzersiz olanlar `by-id` ile, olmayanlar **fiziksel
port konumuna** göre (`by-path`) sabitlendi. Cihaz adı artık hangi sırayla
takıldıklarına bağlı değil.

**Ders.** Kalıcı cihaz adı vermek bir konfor değil, sahada geçirilen saatlerin
doğrudan karşılığı.

## 5. Pano düzenli aralıklarla donuyor

**Belirti.** Telemetri panosu birkaç dakikada bir, birkaç saniye donuyor.

**Sebep.** Araç ile bilgisayar arasındaki USB ağ bağlantısının adres kirası
kısaydı; her yenilemede bağlantı kısa süre kesiliyordu.

**Ders.** Periyodik ve düzenli aralıklı bir kesinti neredeyse her zaman bir
zamanlayıcıdır. Önce "hangi şey bu periyotta bir şey yapıyor" diye sorulur.

## 6. Cihaz listede var ama bağlanılamıyor

**Belirti.** `lsusb` cihazı gösteriyor, uygulama bulamıyor.

**Bulgu.** Bunlar iki ayrı katman: `lsusb` çipin USB veri yolunda **numaralanmış**
olduğunu söyler, uygulamanın aradığı ise `/dev` altındaki **sembolik bağ**.
Sürücü yüklenmemişse veya udev kuralı eşleşmemişse ilki var, ikincisi yok.

**Ders.** "Görünüyor" ile "kullanılabilir" arasındaki farkı bilmek, yanlış yerde
saatlerce aramayı önlüyor.

## 7. Ölçülmemiş sayı, ölçülmüş gibi kullanıldı

**Belirti.** Simülasyonda bir başarım ölçütü tutarlı biçimde raporlanıyordu —
ama o ölçüt hiç hesaplanmamıştı; kod varsayılan değeri basıyordu.

**Bulgu.** Aynı hata dört kez tekrarlandı. Sayı ekranda göründüğü için doğru
sanıldı.

**Ders.** Bir sayının **nereden geldiği** izlenebilir değilse, o sayı yoktur.
Bu depodaki her ölçüm için yanında nasıl elde edildiği yazılı —
[`KALIBRASYON.md`](KALIBRASYON.md) tam olarak bu yüzden var.
