#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (C) 2026 Ahmet Efe Nezli
"""
İki BMS'i aynı anda okur: ana batarya (JK, BLE) + elektronik bataryası (DALY).

Neden tek betik: iki BMS ayrı süreçte okunursa BLE radyosunu ikisi birden
tarar, bağlantılar birbirini düşürür. Tek olay döngüsünde her cihaz kendi
gözcüsüyle çalışıyor; biri koparsa öteki etkilenmiyor.

Taşıyıcılar
    DALY  : UART (tercih edilen) ya da BLE
    JK    : yalnız BLE — o BMS'te dışarı çıkmış kablolu port yok

DALY'de UART'ı tercih et. Sebebi hız değil dayanıklılık: kablo kopmaz,
eşleşme düşmez, telefon uygulamasıyla çakışmaz ve BLE radyosunu ROS'a
bırakır. BLE yolu, kabloyu çekemediğin durum için duruyor.

Kullanım
    # önce adresleri bul (JK için bms_tara.py da kullanılabilir)
    python3 bms_oku.py --tara

    # boru hattını donanımsız dene
    python3 bms_oku.py --daly-sahte

    # sahada
    python3 bms_oku.py --daly-seri /dev/ttyUSB0 --jk-ble AA:BB:CC:DD:EE:FF

Çıktı
    ekrana tablo + her cihaz için CSV (--klasor ile yeri değişir)

UYARI: BLE'de her BMS aynı anda TEK istemci kabul eder. Bu betik bağlıyken
o cihaza telefondan girilemez. DALY'yi UART'tan okursan onun BLE'si serbest
kalır, yani sahada telefonla elektronik bataryasına bakabilirsin.
"""

import argparse
import asyncio
import csv
import datetime
import sys
from pathlib import Path

import daly_protokol as daly

# DALY BLE modüllerinde yaygın olan iki servis ailesi. Bulunamazsa GATT
# ağacındaki ilk bildirim/yazma karakteristiği kullanılır — model değişse
# de çalışsın diye keşif otomatik.
DALY_BLE_ADAYLAR = [
    ("0000fff1-0000-1000-8000-00805f9b34fb",
     "0000fff2-0000-1000-8000-00805f9b34fb"),
    ("0000ffe1-0000-1000-8000-00805f9b34fb",
     "0000ffe1-0000-1000-8000-00805f9b34fb"),
]

# JK BMS — hem bildirim hem komut AYNI karakteristikte (0xFFE1).
# 🔴 3 Eyl 2026: aday listesi verilmediğinde yazma ucu olarak GATT'taki ilk
# yazılabilir karakteristik (0xFFE3) seçiliyordu; istek yanlış uca gidiyor,
# cihaz bağlanıyor ama tek bayt göndermiyordu. Ölçülen GATT ağacı:
#   ffe0 → ffe3 (write) · ffe1 (read/write/notify) · ffe2 (write/notify)
JK_BLE_ADAYLAR = [
    ("0000ffe1-0000-1000-8000-00805f9b34fb",
     "0000ffe1-0000-1000-8000-00805f9b34fb"),
]


def jk_komut(kod: int) -> bytes:
    """JK 'yeni' protokol istek çerçevesi — 20 bayt.

        AA 55 90 EB <kod> 00 … 00 <ilk 19 baytın toplamı & 0xFF>

    0x96 = hücre bilgisi · 0x97 = cihaz bilgisi. **İkisi de salt okuma**,
    ayar yazmıyor — BMS'e körlemesine bayt göndermemek için bilerek yalnız
    bu ikisi kullanılıyor.
    """
    c = bytearray(b"\xaa\x55\x90\xeb")
    c.append(kod & 0xFF)
    c.extend(b"\x00" * 14)          # 4 + 1 + 14 = 19 bayt
    c.append(sum(c) & 0xFF)         # 20. bayt: sağlama
    return bytes(c)


# Her turda sırayla sorulan komutlar. 0x95 (hücreler) her turda gerekmez,
# ağırdır ve çok çerçeveli — beşte bir sıklıkta soruluyor.
TUR_KOMUTLARI = [0x90, 0x93, 0x91, 0x92, 0x98]
SEYREK_KOMUTLAR = [0x94, 0x95, 0x97]
SEYREK_HER = 5


def zaman():
    return datetime.datetime.now().strftime("%H:%M:%S")


class Kayit:
    """Cihaz başına CSV. Sütunlar ilk satırda sabitlenir."""

    def __init__(self, yol: Path, sutunlar):
        self.yol = yol
        self.sutunlar = sutunlar
        yeni = not yol.exists()
        self.f = yol.open("a", newline="", encoding="utf-8")
        self.w = csv.DictWriter(self.f, fieldnames=sutunlar, extrasaction="ignore")
        if yeni:
            self.w.writeheader()

    def yaz(self, satir):
        self.w.writerow(satir)
        self.f.flush()

    def kapat(self):
        self.f.close()


class DalyDurum:
    """DALY'den gelenleri tek bir güncel tabloda biriktirir."""

    SUTUNLAR = ["zaman", "gerilim_V", "akim_A", "soc_yuzde", "kalan_mAh",
                "durum", "sarj_mos", "desarj_mos", "hucre_max_mV",
                "hucre_min_mV", "fark_mV", "sicaklik_max_C", "sicaklik_min_C",
                "hucreler_mV", "arizalar"]

    def __init__(self):
        self.d = {}
        self.toplayici = daly.HucreToplayici()
        self.cerceve = 0
        self.son = None

    def isle(self, s):
        self.cerceve += 1
        self.son = datetime.datetime.now()
        komut = s["komut"]
        if komut == 0x95:
            hucreler = self.toplayici.ekle(s)
            if hucreler:
                self.d["hucreler_mV"] = " ".join(str(v) for v in hucreler)
            return
        if komut == 0x94:
            # Hücre sayısını öğrenince 0x95 birleştirmesi anlam kazanıyor.
            self.toplayici.hucre_sayisi = s.get("hucre_sayisi")
        if komut == 0x98:
            self.d["arizalar"] = ",".join(s["arizalar"]) if s["arizalar"] else ""
            return
        if komut == 0x97:
            return
        for k, v in s.items():
            if k not in ("komut", "ad", "adres", "cerceve_no"):
                self.d[k] = v

    def satir(self):
        r = {"zaman": datetime.datetime.now().isoformat(timespec="seconds")}
        r.update(self.d)
        return r

    def ozet(self):
        d = self.d
        if "gerilim_V" not in d:
            return "veri bekleniyor"
        parcalar = [f"{d['gerilim_V']:.1f} V", f"{d.get('akim_A', 0):+.1f} A",
                    f"%{d.get('soc_yuzde', 0):.0f}"]
        if "fark_mV" in d:
            parcalar.append(f"fark {d['fark_mV']} mV")
        if "sicaklik_max_C" in d:
            parcalar.append(f"{d['sicaklik_max_C']} °C")
        if d.get("arizalar"):
            parcalar.append(f"ARIZA: {d['arizalar']}")
        return " · ".join(parcalar)


class JkDurum:
    """
    JK için henüz çözümleyici YOK — bilerek.

    Sahada iki farklı JK çerçeve formatı dolaşıyor (eski 4E 57, yeni
    55 AA EB 90) ve hangisinin geldiği ancak canlı veriden anlaşılıyor.
    Tahminle yazılan ayrıştırıcı sessizce yanlış gerilim üretir; o yüzden
    burada ham çerçeve sayılıyor ve dökülüyor. Manisa'da ilk dökümü
    aldıktan sonra çözümleyici buraya eklenecek.
    """

    SUTUNLAR = ["zaman", "uzunluk", "imza", "ham"]
    IMZA = {b"\x4e\x57": "eski JK (4E 57)",
            b"\x55\xaa\xeb\x90": "yeni JK (55 AA EB 90)"}

    def __init__(self):
        self.cerceve = 0
        self.bayt = 0
        self.imza = None
        self.son = None
        self.sonuncu = b""

    def isle(self, veri: bytes):
        self.cerceve += 1
        self.bayt += len(veri)
        self.son = datetime.datetime.now()
        self.sonuncu = veri
        if self.imza is None:
            for on, ad in self.IMZA.items():
                if veri.startswith(on):
                    self.imza = ad
                    break

    def satir(self):
        return {"zaman": datetime.datetime.now().isoformat(timespec="seconds"),
                "uzunluk": len(self.sonuncu),
                "imza": self.imza or "",
                "ham": self.sonuncu.hex(" ")}

    def ozet(self):
        if not self.cerceve:
            return "veri bekleniyor"
        return (f"{self.cerceve} çerçeve · {self.bayt} bayt · "
                f"{self.imza or 'imza tanınmadı — dökümü incele'}")


# ------------------------------------------------------------- taşıyıcılar

class SahteDaly:
    """
    Donanımsız deneme. Gerçekçi ama uydurma veri üretir.

    Amacı boru hattını (çözümleme → durum → CSV → ekran) donanım gelmeden
    doğrulamak. Sahada bunu kullanma; sayılar uydurma.
    """

    def __init__(self):
        self.t = 0.0
        self.kuyruk = asyncio.Queue()

    async def baglan(self):
        await asyncio.sleep(0.2)

    async def kapat(self):
        pass

    async def gonder(self, cerceve: bytes):
        import math
        import struct
        komut = cerceve[2]
        self.t += 0.25
        akim = -8.0 - 4.0 * math.sin(self.t / 6)          # 4–12 A deşarj
        hucre = 3700 + int(30 * math.sin(self.t / 9))
        soc = 60 + 20 * math.sin(self.t / 20)
        if komut == 0x90:
            d = struct.pack(">HHHH", int(hucre * 4 / 100), 0,
                            int(30000 + akim * 10), int(soc * 10))
        elif komut == 0x91:
            d = struct.pack(">HBHBH", hucre + 12, 2, hucre - 9, 4, 0)
        elif komut == 0x92:
            d = bytes([40 + 33, 1, 40 + 29, 2, 0, 0, 0, 0])
        elif komut == 0x93:
            d = bytes([2, 1, 1, 40]) + struct.pack(">I", int(80 * soc))
        elif komut == 0x94:
            d = bytes([4, 2, 0, 1, 0, 0, 0, 0])
        elif komut == 0x95:
            # 4S → iki çerçeve
            for no, uc in ((1, (hucre, hucre - 9, hucre + 12)), (2, (hucre + 3, 0, 0))):
                d = bytes([no]) + struct.pack(">HHH", *uc) + b"\x00"
                await self._yolla(0x95, d)
            return
        elif komut == 0x97:
            d = bytes(8)
        elif komut == 0x98:
            d = bytes(8)
        else:
            return
        await self._yolla(komut, d)

    async def _yolla(self, komut, d):
        govde = bytes([daly.BASLIK, daly.ADRES_BMS, komut, 0x08]) + d
        await self.kuyruk.put(govde + bytes([daly.toplam(govde)]))

    async def al(self, zaman_asimi):
        try:
            return await asyncio.wait_for(self.kuyruk.get(), zaman_asimi)
        except asyncio.TimeoutError:
            return b""


class SeriTasiyici:
    """DALY UART. pyserial'ı iş parçacığında çalıştırır, döngüyü bloke etmez."""

    def __init__(self, port, hiz=9600):
        self.port, self.hiz, self.s = port, hiz, None

    async def baglan(self):
        try:
            import serial
        except ImportError:
            raise RuntimeError("pyserial yok. Kur:  pip3 install pyserial")
        loop = asyncio.get_running_loop()
        self.s = await loop.run_in_executor(
            None, lambda: serial.Serial(self.port, self.hiz, timeout=0.3))
        await asyncio.sleep(0.3)
        self.s.reset_input_buffer()

    async def kapat(self):
        if self.s:
            await asyncio.get_running_loop().run_in_executor(None, self.s.close)
            self.s = None

    async def gonder(self, cerceve: bytes):
        await asyncio.get_running_loop().run_in_executor(
            None, self.s.write, cerceve)

    async def al(self, zaman_asimi):
        loop = asyncio.get_running_loop()
        return await loop.run_in_executor(
            None, lambda: self.s.read(daly.CERCEVE_BOY * 4) or b"")


class BleTasiyici:
    """
    BLE üstünden bayt akışı. Hem DALY hem JK için kullanılıyor.

    Karakteristikler önce bilinen adaylardan aranıyor, bulunamazsa GATT
    ağacındaki ilk uygun olan seçiliyor — modelden modele değiştiği için.
    """

    def __init__(self, adres, adaylar=None, sessizlik=20.0):
        self.adres = adres
        self.adaylar = adaylar or []
        self.c = None
        self.oku_uuid = None
        self.yaz_uuid = None
        self.kuyruk = asyncio.Queue()
        self.kopuk = False
        # Bu kadar saniye hiç çerçeve gelmezse bağlantı ölü sayılır.
        # BLE'de kopma her zaman olay üretmiyor; sessiz kalan bir bağlantı
        # da kopuk sayılmazsa gözcü asla yeniden bağlanmaz.
        self.sessizlik = sessizlik

    async def baglan(self):
        try:
            from bleak import BleakClient
        except ImportError:
            raise RuntimeError("bleak yok. Kur:  pip3 install bleak")
        self.kopuk = False
        # Kopma bildirimi: bleak bunu çağırdığında al() hata fırlatsın diye
        # bayrağı kaldırıp kuyruğu dürtüyoruz (bekleyen al() hemen uyansın).
        self.c = BleakClient(self.adres, timeout=20.0,
                             disconnected_callback=self._koptu)
        await self.c.connect()

        bildirim, yazilabilir = [], []
        for s in self.c.services:
            for ch in s.characteristics:
                p = ch.properties
                if "notify" in p or "indicate" in p:
                    bildirim.append(ch.uuid.lower())
                if "write" in p or "write-without-response" in p:
                    yazilabilir.append(ch.uuid.lower())
        if not bildirim:
            raise RuntimeError("bildirim veren karakteristik yok — "
                               "cihaz beklenen BMS olmayabilir")

        for oku, yaz in self.adaylar:
            if oku in bildirim:
                self.oku_uuid = oku
                self.yaz_uuid = yaz if yaz in yazilabilir else (
                    yazilabilir[0] if yazilabilir else None)
                break
        if self.oku_uuid is None:
            self.oku_uuid = bildirim[0]
            self.yaz_uuid = yazilabilir[0] if yazilabilir else None

        await self.c.start_notify(
            self.oku_uuid, lambda _, v: self.kuyruk.put_nowait(bytes(v)))
        # Sessizlik sayacı bağlantı anından başlar; yoksa ilk boş okuma
        # anında "veri gelmiyor" sanılır.
        self._son_veri = asyncio.get_running_loop().time()

    def _koptu(self, _client):
        self.kopuk = True
        self.kuyruk.put_nowait(b"")

    async def kapat(self):
        if self.c:
            try:
                await self.c.disconnect()
            except Exception:                                    # noqa: BLE001
                pass
            self.c = None

    async def gonder(self, cerceve: bytes):
        if self.kopuk:
            raise ConnectionError("BLE koptu")
        if self.yaz_uuid is None:
            return
        await self.c.write_gatt_char(self.yaz_uuid, cerceve, response=False)

    async def al(self, zaman_asimi):
        if self.kopuk:
            raise ConnectionError("BLE koptu")
        try:
            veri = await asyncio.wait_for(self.kuyruk.get(), zaman_asimi)
        except asyncio.TimeoutError:
            veri = b""
        if self.kopuk:
            raise ConnectionError("BLE koptu")
        if veri:
            self._son_veri = asyncio.get_running_loop().time()
        elif (asyncio.get_running_loop().time()
              - getattr(self, "_son_veri", 0)) > self.sessizlik:
            # Bağlantı ayakta görünüyor ama veri yok: sessiz ölüm.
            raise ConnectionError(
                f"{self.sessizlik:.0f} sn'dir çerçeve gelmiyor")
        return veri


# ---------------------------------------------------------------- sürücüler

async def daly_dongu(tasiyici, durum, kayit, aralik):
    """Sırayla komut sorar, gelen çerçeveleri çözümler."""
    tampon = bytearray()
    tur = 0
    while True:
        komutlar = list(TUR_KOMUTLARI)
        if tur % SEYREK_HER == 0:
            komutlar += SEYREK_KOMUTLAR
        for komut in komutlar:
            await tasiyici.gonder(daly.istek(komut))
            # Cevap tek çerçevede gelmeyebilir; kısa pencere boyunca topla.
            son = asyncio.get_running_loop().time() + 0.35
            while asyncio.get_running_loop().time() < son:
                parca = await tasiyici.al(0.2)
                if not parca:
                    continue
                tampon.extend(parca)
                for c in daly.cerceveleri_ayikla(tampon):
                    s = daly.cozumle(c)
                    if s:
                        durum.isle(s)
            if len(tampon) > 512:            # hiç çözülemeyen çöp birikmesin
                del tampon[:-64]
        if durum.cerceve:
            kayit.yaz(durum.satir())
        tur += 1
        await asyncio.sleep(aralik)


async def jk_dongu(tasiyici, durum, kayit, aralik):
    """JK bağlandıktan sonra İSTEK bekler — kendiliğinden yayın YAPMIYOR.

    🔴 3 Eyl 2026 düzeltmesi. Eski varsayım "JK kendiliğinden yayın yapıyor,
    yalnız dinlenir" idi; gerçek cihazda (ana batarya, `50912AA35102300`)
    bağlantı kuruluyor ama **tek bayt gelmiyordu** — 20 sn sessizlikten sonra
    gözcü bağlantıyı ölü sayıp sonsuz döngüye giriyordu.

    Cihaz bilgisi bir kez, hücre bilgisi düzenli aralıkla istenir: bazı JK
    sürümleri tek yanıt verip susuyor, o yüzden istek tazeleniyor.
    """
    await tasiyici.gonder(jk_komut(0x97))          # cihaz bilgisi (bir kez)
    await asyncio.sleep(0.3)
    await tasiyici.gonder(jk_komut(0x96))          # hücre bilgisi
    son_kayit = 0.0
    son_istek = asyncio.get_running_loop().time()
    while True:
        veri = await tasiyici.al(2.0)
        simdi = asyncio.get_running_loop().time()
        if veri:
            durum.isle(veri)
            if simdi - son_kayit >= aralik:
                kayit.yaz(durum.satir())
                son_kayit = simdi
        if simdi - son_istek >= aralik:
            await tasiyici.gonder(jk_komut(0x96))
            son_istek = simdi


async def gozcu(ad, tasiyici_kur, dongu, durum, kayit, aralik):
    """
    Tek cihazın ömrünü yönetir: bağlan, çalıştır, koparsa geri gel.

    Bekleme süresi 2'den 30 saniyeye kadar ikişer katlanıyor. Sahada BLE
    kopması normal; tek seferlik bağlantı yetmez.
    """
    bekle = 2
    while True:
        t = tasiyici_kur()
        try:
            print(f"[{zaman()}] {ad}: bağlanılıyor…")
            await t.baglan()
            print(f"[{zaman()}] {ad}: bağlandı")
            bekle = 2
            await dongu(t, durum, kayit, aralik)
        except asyncio.CancelledError:
            await t.kapat()
            raise
        except Exception as e:                                   # noqa: BLE001
            print(f"[{zaman()}] {ad}: koptu — {type(e).__name__}: {e}")
            await t.kapat()
            print(f"[{zaman()}] {ad}: {bekle} sn sonra yeniden denenecek")
            await asyncio.sleep(bekle)
            bekle = min(bekle * 2, 30)


async def tablo(cihazlar, aralik=5.0):
    """Ekrana düzenli özet basar. Kayıt CSV'de, bu yalnız göz içindir."""
    while True:
        await asyncio.sleep(aralik)
        print(f"\n─── {zaman()} ───")
        for ad, durum in cihazlar:
            yas = ""
            if durum.son:
                gecen = (datetime.datetime.now() - durum.son).total_seconds()
                if gecen > 10:
                    yas = f"  ⚠ {gecen:.0f} sn'dir veri yok"
            print(f"  {ad:<22} {durum.ozet()}{yas}")


async def tara(sure):
    try:
        from bleak import BleakScanner
    except ImportError:
        sys.exit("bleak yok. Kur:  pip3 install bleak")
    print(f"BLE taranıyor ({sure} sn)…\n")
    # 🔴 3 Eyl 2026: yalnız İSİME bakmak yetmiyor. Ana bataryanın JK'sı
    # "50912AA35102300" diye yayın yapıyor — içinde ne "jk" ne "bms" var,
    # tarama onu kaçırdı ve cihaz yokmuş sanıldı. Asıl imza SERVİS kimliği:
    # JK 0xFFE0, DALY 0xFFF0/0xFFE0. İsim artık yalnız yardımcı ipucu.
    bulunan = await BleakScanner.discover(timeout=sure, return_adv=True)
    print(f"{'ADRES':<20} {'AD':<26} TAHMİN")
    for d, adv in sorted(bulunan.values(), key=lambda x: (x[0].name or "").lower()):
        ad = (d.name or "—")
        dusuk = ad.lower()
        servisler = " ".join(u.lower() for u in (adv.service_uuids or []))
        tahmin = ""
        if "0000ffe0" in servisler:
            tahmin = "← JK/BMS servisi (0xFFE0) GÖRÜLDÜ"
        elif "0000fff0" in servisler:
            tahmin = "← DALY servisi (0xFFF0) GÖRÜLDÜ"
        elif "jk" in dusuk:
            tahmin = "← JK olabilir (ana batarya)"
        elif "daly" in dusuk or dusuk.startswith("jhb") or "bms" in dusuk:
            tahmin = "← DALY olabilir (elektronik)"
        print(f"{d.address:<20} {ad:<26} {tahmin}")
    print("\nHiçbiri işaretlenmediyse: cihaz servis kimliğini yayında "
          "vermiyor olabilir — ble_incele.py <adres> ile GATT ağacına bak.")
    print("\nDALY'de kablo varsa BLE yerine UART kullan — bkz. OKUBENI.md")


async def calistir(n):
    klasor = Path(n.klasor)
    klasor.mkdir(parents=True, exist_ok=True)
    damga = datetime.datetime.now().strftime("%Y%m%d_%H%M")

    isler, cihazlar, kayitlar = [], [], []

    if n.daly_sahte or n.daly_seri or n.daly_ble:
        durum = DalyDurum()
        kayit = Kayit(klasor / f"daly_{damga}.csv", DalyDurum.SUTUNLAR)
        kayitlar.append(kayit)
        if n.daly_sahte:
            kur, ad = SahteDaly, "DALY (SAHTE VERİ)"
        elif n.daly_seri:
            kur = lambda: SeriTasiyici(n.daly_seri, n.daly_hiz)   # noqa: E731
            ad = f"DALY · {n.daly_seri}"
        else:
            kur = lambda: BleTasiyici(n.daly_ble, DALY_BLE_ADAYLAR)  # noqa: E731
            ad = f"DALY · BLE"
        cihazlar.append((ad, durum))
        isler.append(gozcu(ad, kur, daly_dongu, durum, kayit, n.aralik))

    if n.jk_ble:
        durum = JkDurum()
        kayit = Kayit(klasor / f"jk_{damga}.csv", JkDurum.SUTUNLAR)
        kayitlar.append(kayit)
        ad = "JK · BLE (ana)"
        cihazlar.append((ad, durum))
        isler.append(gozcu(ad, lambda: BleTasiyici(n.jk_ble, JK_BLE_ADAYLAR),
                           jk_dongu, durum, kayit, n.aralik))

    if not isler:
        sys.exit("Hiç cihaz verilmedi. --daly-sahte ile deneyebilirsin; "
                 "seçenekler için --help")

    print(f"{len(isler)} cihaz · CSV klasörü: {klasor}\n"
          f"Ctrl+C ile bitir.\n")
    isler.append(tablo(cihazlar))
    try:
        await asyncio.gather(*isler)
    finally:
        for k in kayitlar:
            k.kapat()


def main():
    a = argparse.ArgumentParser(
        description="İki BMS'i aynı anda oku (JK ana + DALY elektronik)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="Ayrıntı ve saha sırası: OKUBENI.md")
    a.add_argument("--tara", action="store_true", help="BLE tara ve çık")
    a.add_argument("--tara-sure", type=float, default=8)
    a.add_argument("--daly-seri", help="DALY UART portu, örn. /dev/ttyUSB0")
    a.add_argument("--daly-hiz", type=int, default=9600)
    a.add_argument("--daly-ble", help="DALY BLE adresi (UART yoksa)")
    a.add_argument("--daly-sahte", action="store_true",
                   help="donanımsız deneme; veriler UYDURMA")
    a.add_argument("--jk-ble", help="JK BLE adresi (ana batarya)")
    a.add_argument("--aralik", type=float, default=1.0,
                   help="kayıt aralığı, sn (varsayılan 1)")
    a.add_argument("--klasor", default=str(Path(__file__).with_name("kayit")))
    n = a.parse_args()
    try:
        asyncio.run(tara(n.tara_sure) if n.tara else calistir(n))
    except KeyboardInterrupt:
        print("\nkesildi.")


if __name__ == "__main__":
    main()
