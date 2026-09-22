#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
# Copyright (C) 2026 Ahmet Efe Nezli
"""
DALY Smart BMS — çerçeve kurma ve çözümleme. Saf protokol, hiç G/Ç yok.

Neden ayrı dosya: bu katman donanımsız test edilebiliyor. `--test` ile
elle kurulmuş çerçeveler üstünde doğrulanır; sahada bir sayı tuhaf
göründüğünde önce burayı, sonra kabloyu şüphelenirsin.

Çerçeve (hem UART hem BLE'de aynı, 13 bayt sabit):

    A5 <adres> <komut> 08 <8 bayt veri> <toplam>

    adres   : 0x40 = bilgisayar → BMS,  0x01 = BMS → bilgisayar
    toplam  : ilk 12 baytın toplamı & 0xFF

Sağlama toplamı olduğu için yanlış hizalanmış ya da bozuk bir çerçeve
sessizce geçmez — `cozumle()` None döner.

DOĞRULANMA DURUMU: protokolün kendisi DALY'nin yaygın belgelerinden ve
açık kaynak uygulamalarından; sahada bu BMS ile HENÜZ denenmedi.
Sağlama toplamı tutuyorsa çerçeve yapısı doğru demektir. Asıl teyit
edilecek şey ölçek katsayıları (özellikle akım işareti) — bkz. OKUBENI.md.
"""

import struct
import sys

BASLIK = 0xA5
ADRES_HOST = 0x40
ADRES_BMS = 0x01
CERCEVE_BOY = 13

# Kullandığımız komutlar. Sağdaki isim çıktıda görünür.
KOMUT = {
    0x90: "ozet",        # toplam gerilim, akım, SOC
    0x91: "hucre_uc",    # en yüksek / en düşük hücre
    0x92: "sicaklik_uc", # en yüksek / en düşük sıcaklık
    0x93: "mos",         # şarj/deşarj MOSFET durumu, kalan kapasite
    0x94: "yapi",        # hücre sayısı, sensör sayısı, yük/şarj durumu
    0x95: "hucreler",    # hücre gerilimleri (çok çerçeveli)
    0x96: "sicakliklar", # sensör sıcaklıkları (çok çerçeveli)
    0x97: "balans",      # hangi hücreler balanslanıyor
    0x98: "arizalar",    # hata bayrakları
}

# 0x98'in 7 veri baytındaki bit adları. Sırası DALY belgesindeki sıra.
ARIZA_BITLERI = [
    ("hucre_gerilim_yuksek_1", "hucre_gerilim_yuksek_2",
     "hucre_gerilim_dusuk_1", "hucre_gerilim_dusuk_2",
     "toplam_gerilim_yuksek_1", "toplam_gerilim_yuksek_2",
     "toplam_gerilim_dusuk_1", "toplam_gerilim_dusuk_2"),
    ("sarj_sicaklik_yuksek_1", "sarj_sicaklik_yuksek_2",
     "sarj_sicaklik_dusuk_1", "sarj_sicaklik_dusuk_2",
     "desarj_sicaklik_yuksek_1", "desarj_sicaklik_yuksek_2",
     "desarj_sicaklik_dusuk_1", "desarj_sicaklik_dusuk_2"),
    ("sarj_akim_yuksek_1", "sarj_akim_yuksek_2",
     "desarj_akim_yuksek_1", "desarj_akim_yuksek_2",
     "soc_yuksek_1", "soc_yuksek_2", "soc_dusuk_1", "soc_dusuk_2"),
    ("fark_gerilim_1", "fark_gerilim_2", "fark_sicaklik_1", "fark_sicaklik_2",
     "-", "-", "-", "-"),
    ("sarj_mos_sicaklik", "desarj_mos_sicaklik",
     "sarj_mos_sensor_ariza", "desarj_mos_sensor_ariza",
     "sarj_mos_surucu_ariza", "desarj_mos_surucu_ariza",
     "-", "-"),
    ("gerilim_toplama_ariza", "sicaklik_sensor_ariza", "eeprom_ariza",
     "rtc_ariza", "onsarj_ariza", "haberlesme_ariza",
     "ic_haberlesme_ariza", "akim_modulu_ariza"),
    ("toplam_gerilim_algilama_ariza", "kisa_devre_koruma", "dusuk_gerilim_kilit",
     "-", "-", "-", "-", "-"),
]


def toplam(govde: bytes) -> int:
    """İlk 12 baytın sağlama toplamı."""
    return sum(govde) & 0xFF


def istek(komut: int, adres: int = ADRES_HOST) -> bytes:
    """BMS'e gönderilecek 13 baytlık istek çerçevesi."""
    if komut not in KOMUT:
        raise ValueError(f"bilinmeyen komut 0x{komut:02X}")
    govde = bytes([BASLIK, adres, komut, 0x08]) + bytes(8)
    return govde + bytes([toplam(govde)])


def cerceveleri_ayikla(tampon: bytearray):
    """
    Akıştan tam çerçeveleri söker. Tamponu YERİNDE kısaltır.

    BLE'de çerçeveler bölünerek ya da birleşerek gelebiliyor; bu yüzden
    bayt akışı gibi davranıp 0xA5 arıyoruz. Sağlama toplamı tutmayan
    aday atlanıyor — böylece veri içinde rastlanan 0xA5 bizi kaydırmıyor.
    """
    cikti = []
    i = 0
    while i + CERCEVE_BOY <= len(tampon):
        if tampon[i] != BASLIK:
            i += 1
            continue
        aday = bytes(tampon[i:i + CERCEVE_BOY])
        if toplam(aday[:12]) == aday[12]:
            cikti.append(aday)
            i += CERCEVE_BOY
        else:
            i += 1
    del tampon[:i]
    return cikti


def cozumle(c: bytes):
    """
    Tek çerçeveyi sözlüğe çevirir. Geçersizse None.

    Dönen sözlükte her zaman 'komut' ve 'ad' var; gerisi komuta göre.
    Çok çerçeveli komutlarda ('hucreler', 'sicakliklar') 'cerceve_no'
    bulunur — birleştirme çağıranın işi, bkz. HucreToplayici.
    """
    if len(c) != CERCEVE_BOY or c[0] != BASLIK:
        return None
    if toplam(c[:12]) != c[12]:
        return None

    komut = c[2]
    d = c[4:12]
    s = {"komut": komut, "ad": KOMUT.get(komut, f"0x{komut:02X}"),
         "adres": c[1]}

    if komut == 0x90:
        ham_akim = struct.unpack(">H", d[4:6])[0]
        s.update(
            gerilim_V=struct.unpack(">H", d[0:2])[0] / 10.0,
            # 30000 ofset: altı deşarj (negatif), üstü şarj.
            akim_A=(ham_akim - 30000) / 10.0,
            soc_yuzde=struct.unpack(">H", d[6:8])[0] / 10.0,
        )
    elif komut == 0x91:
        s.update(
            hucre_max_mV=struct.unpack(">H", d[0:2])[0], hucre_max_no=d[2],
            hucre_min_mV=struct.unpack(">H", d[3:5])[0], hucre_min_no=d[5],
        )
        s["fark_mV"] = s["hucre_max_mV"] - s["hucre_min_mV"]
    elif komut == 0x92:
        s.update(sicaklik_max_C=d[0] - 40, sicaklik_max_no=d[1],
                 sicaklik_min_C=d[2] - 40, sicaklik_min_no=d[3])
    elif komut == 0x93:
        s.update(
            durum={0: "bosta", 1: "sarj", 2: "desarj"}.get(d[0], f"? {d[0]}"),
            sarj_mos=bool(d[1]), desarj_mos=bool(d[2]),
            bms_omur=d[3],
            kalan_mAh=struct.unpack(">I", d[4:8])[0],
        )
    elif komut == 0x94:
        s.update(hucre_sayisi=d[0], sensor_sayisi=d[1],
                 sarj_cihazi=bool(d[2]), yuk=bool(d[3]), dio=d[4])
    elif komut == 0x95:
        s["cerceve_no"] = d[0]
        s["hucreler_mV"] = [struct.unpack(">H", d[1 + 2 * k:3 + 2 * k])[0]
                            for k in range(3)]
    elif komut == 0x96:
        s["cerceve_no"] = d[0]
        s["sicakliklar_C"] = [b - 40 for b in d[1:8]]
    elif komut == 0x97:
        bitler = int.from_bytes(d[0:6], "big")
        s["balans"] = [k + 1 for k in range(48)
                       if bitler & (1 << (47 - k))]
    elif komut == 0x98:
        aktif = []
        for bayt_no in range(7):
            for bit_no in range(8):
                if d[bayt_no] & (1 << bit_no):
                    ad = ARIZA_BITLERI[bayt_no][bit_no]
                    if ad != "-":
                        aktif.append(ad)
        s["arizalar"] = aktif
        s["ariza_sayaci"] = d[7]
    else:
        s["ham"] = d.hex(" ")
    return s


class HucreToplayici:
    """
    0x95'in çok çerçeveli cevabını toplar.

    BMS her çerçevede 3 hücre yolluyor, çerçeve numarası 1'den başlıyor.
    Hücre sayısı 0x94'ten öğrenilir; bilinmiyorsa numara atlayınca ya da
    tekrar 1 gelince tur bitmiş sayılır.
    """

    def __init__(self, hucre_sayisi=None):
        self.hucre_sayisi = hucre_sayisi
        self.parca = {}

    def ekle(self, s):
        no = s["cerceve_no"]
        if no in self.parca and no == 1:
            self.parca.clear()
        self.parca[no] = s["hucreler_mV"]
        return self.tamam()

    def tamam(self):
        if not self.parca:
            return None
        n = self.hucre_sayisi
        if n is None:
            return None
        gereken = (n + 2) // 3
        if len(self.parca) < gereken:
            return None
        duz = []
        for no in sorted(self.parca):
            duz.extend(self.parca[no])
        return duz[:n]


# ---------------------------------------------------------------- öz test

def _test():
    basarisiz = 0

    def kontrol(ad, kosul, ek=""):
        nonlocal basarisiz
        print(f"  {'OK  ' if kosul else 'HATA'} {ad}{' — ' + ek if ek else ''}")
        if not kosul:
            basarisiz += 1

    def sahte(komut, veri):
        govde = bytes([BASLIK, ADRES_BMS, komut, 0x08]) + bytes(veri)
        return govde + bytes([toplam(govde)])

    print("istek çerçevesi")
    i = istek(0x90)
    kontrol("boy 13", len(i) == 13)
    kontrol("başlık ve komut", i[0] == 0xA5 and i[1] == 0x40 and i[2] == 0x90)
    kontrol("sağlama", toplam(i[:12]) == i[12], i.hex(" "))

    print("0x90 özet — 14,8 V paket, 12,3 A deşarj, %78,5")
    # 148 = 14,8 V ; 30000-123 = 29877 ; 785 = %78,5
    s = cozumle(sahte(0x90, struct.pack(">HHHH", 148, 0, 29877, 785)))
    kontrol("gerilim 14,8", s and abs(s["gerilim_V"] - 14.8) < 1e-6)
    kontrol("akım -12,3 (deşarj negatif)", s and abs(s["akim_A"] + 12.3) < 1e-6)
    kontrol("SOC 78,5", s and abs(s["soc_yuzde"] - 78.5) < 1e-6)

    print("0x91 hücre uçları")
    s = cozumle(sahte(0x91, bytes([0x0F, 0xA0, 2, 0x0F, 0x50, 4, 0, 0])))
    kontrol("max 4000 mV / hücre 2", s and s["hucre_max_mV"] == 4000
            and s["hucre_max_no"] == 2)
    kontrol("fark 80 mV", s and s["fark_mV"] == 80)

    print("0x92 sıcaklık (40 ofset)")
    s = cozumle(sahte(0x92, bytes([40 + 31, 1, 40 + 27, 2, 0, 0, 0, 0])))
    kontrol("max 31 °C", s and s["sicaklik_max_C"] == 31)
    kontrol("min 27 °C", s and s["sicaklik_min_C"] == 27)

    print("0x93 MOS ve kalan kapasite")
    s = cozumle(sahte(0x93, bytes([2, 1, 1, 42]) + struct.pack(">I", 6280)))
    kontrol("durum deşarj", s and s["durum"] == "desarj")
    kontrol("iki MOS açık", s and s["sarj_mos"] and s["desarj_mos"])
    kontrol("kalan 6280 mAh", s and s["kalan_mAh"] == 6280)

    print("0x95 hücreler — 4S, iki çerçeve")
    t = HucreToplayici(hucre_sayisi=4)
    c1 = cozumle(sahte(0x95, bytes([1]) + struct.pack(">HHH", 3701, 3699, 3702) + b"\x00"))
    kontrol("1. çerçeve tamamlanmadı", t.ekle(c1) is None)
    c2 = cozumle(sahte(0x95, bytes([2]) + struct.pack(">HHH", 3700, 0, 0) + b"\x00"))
    kontrol("2. çerçeveyle tamam", t.ekle(c2) == [3701, 3699, 3702, 3700])

    print("0x97 balans")
    s = cozumle(sahte(0x97, bytes([0b10100000, 0, 0, 0, 0, 0, 0, 0])))
    kontrol("hücre 1 ve 3 balansta", s and s["balans"] == [1, 3])

    print("0x98 arızalar")
    s = cozumle(sahte(0x98, bytes([0b00000001, 0, 0, 0, 0, 0, 0, 3])))
    kontrol("hücre gerilim yüksek 1", s and "hucre_gerilim_yuksek_1" in s["arizalar"])
    kontrol("arıza sayacı 3", s and s["ariza_sayaci"] == 3)

    print("bozuk çerçeve reddi")
    kotu = bytearray(sahte(0x90, bytes(8)))
    kotu[12] ^= 0xFF
    kontrol("sağlama tutmayan None", cozumle(bytes(kotu)) is None)
    kontrol("kısa çerçeve None", cozumle(b"\xa5\x01\x90") is None)

    print("akıştan ayıklama — araya çöp karışmış")
    iyi = sahte(0x90, struct.pack(">HHHH", 148, 0, 30000, 500))
    tampon = bytearray(b"\xa5\xa5\x00" + iyi + b"\x77" + iyi + b"\xa5\x01")
    bulunan = cerceveleri_ayikla(tampon)
    kontrol("iki çerçeve bulundu", len(bulunan) == 2, f"{len(bulunan)} bulundu")
    kontrol("artık tampon korundu", len(tampon) == 2, tampon.hex(" "))

    print()
    if basarisiz:
        print(f"{basarisiz} test BAŞARISIZ")
        return 1
    print("tüm testler geçti")
    return 0


if __name__ == "__main__":
    sys.exit(_test() if "--test" in sys.argv else
             print(__doc__) or 0)
