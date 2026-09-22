#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
# Copyright (C) 2026 Ahmet Efe Nezli
"""
sahte_jetson.py — F767 komut yolunu ROS olmadan denemek icin.

Neden var: komut yolu 2 Eylul 2026'da yazildi ama `seri_kopru.py` ile denemek
Jetson'i, ROS yiginini ve calisan bir otonomiyi ayni anda gerektiriyor. Uctan
uca ilk deneme oyle yapilirsa, bir sey calismadiginda kusurun kartta mi
koprude mi otonomide mi oldugu ayirt EDILEMEZ. Bu betik yalnizca protokolu
konusur: kartin komutu aliyor mu, dogru mu anliyor, guvenlik kilitleri
tutuyor mu -- bunlari tek basina cevaplar.

BAGLANTI (tezgah)
  USB-TTL           F767ZI
  ---------------   -------------------------------
  TX          -->   PG9  = D0  = CN10-16  (USART6_RX)
  RX          <--   PG14 = D1  = CN10-14  (USART6_TX)
  GND         <->   GND        = CN12-9
  ⚠ USB-TTL'in 5 V ucunu KARTA BAGLAMA. Kart kendi beslemesinden calisir.
  ⚠ ST-LINK sanal COM portu (COM6) BU DEGIL. O port teshis ciktisi ve pano
    icin; bu betik ayri bir USB-TTL ister.

KULLANIM
    python.exe sahte_jetson.py --port COM7
    python3 sahte_jetson.py --port /dev/ttyUSB0
    python3 sahte_jetson.py --dinle          # yalniz telemetriyi coz, komut yollama

Komutlar (calisirken yaz + Enter):
    h 0.5     hiz komutu [m/s], - ile geri
    a 10      direksiyon [derece]. 🔴 ROS isareti: + SOLA
    f 300     fren [binde 0..1000]
    d         DUR (PKT_J_DUR)
    e / E     Jetson E-STOP kaldir / ilan et
    s         sus  -- heartbeat dahil hicbir sey gonderme (zaman asimi denemesi)
    c         konus -- yeniden gondermeye basla
    ?         son telemetri ozeti
    q         cik (cikarken DUR gonderir)

⚠ ILK DENEMEDE ARAC TEKERLEKLERI YERDEN KESIK OLSUN. Bu betik gercek gaz
  verir; kartin tek savunmasi SwA kesme ve E-STOP.
"""

import argparse
import struct
import sys
import threading
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial yok:  pip install pyserial --break-system-packages")

BASLA, BITIS, BOYUT = 0xAA, 0x55, 8

# Jetson -> kart
PKT_SURUCU, PKT_DUR, PKT_HB, PKT_ESTOP, PKT_FREN = 0x01, 0x02, 0x04, 0x07, 0x08
PKT_LAZER, PKT_PAN, PKT_TILT = 0x03, 0x05, 0x06   # taret (TARET_VAR true iken)

# kart -> Jetson (0x30 blogu)
AD = {
    0x30: "enkoder sayimi", 0x31: "hiz", 0x32: "yaw/roll", 0x33: "pitch/kalib",
    0x34: "E-STOP", 0x35: "saglik", 0x36: "RC", 0x37: "surus",
    0x38: "JETSON ECHO", 0x39: "MOD",
    0x3A: "RC HAM", 0x3B: "SURUM", 0x3C: "HAT SAYACLARI",
}
MOD_AD = {0: "MANUEL", 1: "BOS/DUR (e-stop)", 2: "TAM OTONOM"}
JDR = [(0x01, "link"), (0x02, "jestop"), (0x04, "dur"), (0x08, "elle")]
HATA = [(0x01, "BNO yok"), (0x02, "BNO kalib"), (0x04, "enk sessiz"),
        (0x08, "gost sessiz"), (0x10, "estop uyusmaz"), (0x20, "RC yok"),
        (0x40, "fren stall"), (0x80, "GAZ YOK")]


def paket(komut, v0=0, v1=0):
    v0 = max(-32768, min(32767, int(v0)))
    v1 = max(-32768, min(32767, int(v1)))
    veri = struct.pack(">hh", v0, v1)
    crc = komut
    for b in veri:
        crc ^= b
    return struct.pack("BB4sBB", BASLA, komut, veri, crc, BITIS)


def bitler(deger, tablo):
    ad = [m for bit, m in tablo if deger & bit]
    return ",".join(ad) if ad else "-"


class SahteJetson:
    def __init__(self, ser, dinle_yalniz=False):
        self.ser = ser
        self.dinle_yalniz = dinle_yalniz
        self.kilit = threading.Lock()
        self.hiz = 0.0
        self.aci = 0.0
        self.fren = 0
        self.estop = False
        self.dur_gonder = True      # taze komut gelene kadar DUR
        self.sus = dinle_yalniz
        self.calisiyor = True
        self.telemetri = {}
        self.son_mod = None
        self.son_alinan = None   # 0x3C ham sayaclar (alt 16 bit)
        self.son_bozuk  = None
        self.d_alinan   = None   # saniyedeki artis — 0x3C geldikce hesaplanir
        self.d_bozuk    = None
        self.paket_sayisi = 0

    # ── gonderim: gercek koprunun ritmi (20 Hz surus + 10 Hz heartbeat) ─────
    def gonderici(self):
        n = 0
        while self.calisiyor:
            time.sleep(0.05)
            if self.sus:
                continue
            with self.kilit:
                try:
                    if self.estop:
                        self.ser.write(paket(PKT_ESTOP, 1, 0))
                        self.ser.write(paket(PKT_DUR))
                    elif self.dur_gonder:
                        self.ser.write(paket(PKT_DUR))
                    else:
                        self.ser.write(paket(PKT_SURUCU,
                                             int(self.hiz * 1000),
                                             int(self.aci * 100)))
                        self.ser.write(paket(PKT_FREN, self.fren, 0))
                    n += 1
                    if n % 2 == 0:
                        self.ser.write(paket(PKT_HB))
                except serial.SerialException as e:
                    print(f"\n[HATA] yazilamadi: {e}")
                    self.calisiyor = False

    # ── alim: kartin 0x30 blogunu coz ──────────────────────────────────────
    def alici(self):
        ara = bytearray()
        while self.calisiyor:
            try:
                gelen = self.ser.read(64)
            except serial.SerialException:
                break
            if not gelen:
                continue
            ara.extend(gelen)
            while len(ara) >= BOYUT:
                if ara[0] != BASLA:
                    ara.pop(0)
                    continue
                p = bytes(ara[:BOYUT])
                del ara[:BOYUT]
                if p[7] != BITIS:
                    continue
                crc = p[1] ^ p[2] ^ p[3] ^ p[4] ^ p[5]
                if crc != p[6]:
                    continue
                self.coz(p)

    def coz(self, p):
        komut = p[1]
        v0, v1 = struct.unpack(">hh", p[2:6])
        self.paket_sayisi += 1
        self.telemetri[komut] = (v0, v1)

        # Kip degisimi ve link kaybi SESSIZ GECILMEZ: bu iki satir, "komut
        # gonderiyorum ama arac dinlemiyor" durumunun tek gorunur yeri.
        if komut == 0x39:
            if self.son_mod != v0:
                print(f"\n>>> KART KIPI: {MOD_AD.get(v0, v0)}  [{bitler(v1, JDR)}]")
                self.son_mod = v0
            if not (v1 & 0x01) and not self.sus:
                print("\n!!! Kart bizi CANLI GORMUYOR — heartbeat ulasmiyor.")
        elif komut == 0x34 and v0:
            print("\n!!! KART E-STOP BASILI")
        elif komut == 0x3C:
            # Delta PAKET GELDIGINDE hesaplanir, ozet() cagrildiginda DEGIL:
            # ozet() kullanici Enter'a bastikca calisiyor, yani araligi
            # duzensiz. Iki kez arka arkaya basilsaydi delta 0 cikar ve
            # "hic paket gelmiyor" yanlis alarmi verirdi. 0x3C 1 Hz geldigi
            # icin paket basina delta zaten saniyedeki hizdir.
            al, bz = (v0 & 0xFFFF), (v1 & 0xFFFF)
            if self.son_alinan is not None:
                self.d_alinan = (al - self.son_alinan) & 0xFFFF
                self.d_bozuk  = (bz - self.son_bozuk)  & 0xFFFF
            self.son_alinan, self.son_bozuk = al, bz

    def ozet(self):
        t = self.telemetri
        s = []
        if 0x39 in t:
            m, j = t[0x39]
            s.append(f"kip={MOD_AD.get(m, m)} [{bitler(j, JDR)}]")
        if 0x38 in t:
            h, a = t[0x38]
            s.append(f"kartin ANLADIGI: hiz={h/1000.0:+.3f} m/s  aci={a/100.0:+.2f}°")
            with self.kilit:
                g_h, g_a = self.hiz, self.aci
            if self.dur_gonder or self.estop:
                g_h = 0.0
            # 🔴 Gonderilen ile anlasilan arasindaki fark tek basina teshis:
            # olcek hatasi, isaret hatasi ve kayip paket burada gorunur.
            if abs(g_h - h / 1000.0) > 0.02 or abs(g_a - a / 100.0) > 0.05:
                s.append(f"  ⚠ FARK VAR — gonderilen: hiz={g_h:+.3f} aci={g_a:+.2f}")
        # 🎯 Komut yolunun ILK gercek testi tam olarak bu satir. Sayaclar
        # 4 Eylul'de eklendi (0x3C); once yalniz kartin ASCII teshis
        # satirindaydilar ve portu bu betik tuttugunda gorunmuyorlardi.
        # Ikisi ancak BIRLIKTE ayirt ediyor:
        #   alinan sabit + bozuk sabit  -> hic gelmiyor (kablo/surec/port)
        #   alinan sabit + bozuk artiyor -> geliyor ama cozulmuyor (baud)
        # ⚠ Alanlar sayaclarin ALT 16 BITI, 65535'te sarar — mutlak degere
        # degil ARTISA bakilacak.
        if 0x3C in t and self.d_alinan is not None:
            al, bz = (t[0x3C][0] & 0xFFFF), (t[0x3C][1] & 0xFFFF)
            uyari = ""
            if self.d_alinan == 0 and self.d_bozuk > 0:
                uyari = "  ⚠ BAUD UYUSMAZLIGI — geliyor ama cozulmuyor"
            elif self.d_alinan == 0:
                uyari = "  ⚠ KART HIC PAKET ALMIYOR"
            s.append(f"kart alinan={al} (+{self.d_alinan}/sn)  "
                     f"bozuk={bz} (+{self.d_bozuk}/sn){uyari}")
        if 0x3B in t:
            s.append(f"kart surumu: protokol={t[0x3B][0]}  yapi={t[0x3B][1]}")
        if 0x37 in t:
            s.append(f"gaz={t[0x37][0]} mV  fren={t[0x37][1]}‰")
        if 0x31 in t:
            s.append(f"enkoder hizi={t[0x31][0]} mm/s")
        if 0x32 in t:
            s.append(f"yaw={t[0x32][0]/10.0:.1f}°")
        if 0x35 in t:
            s.append(f"hata=0x{t[0x35][0] & 0xFF:02X} [{bitler(t[0x35][0], HATA)}]"
                     f"  calisma={t[0x35][1]} sn")
        s.append(f"toplam paket={self.paket_sayisi}")
        return "\n  ".join(s) if s else "karttan HIC paket gelmedi"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--dinle", action="store_true",
                    help="yalniz telemetriyi coz, komut gonderme")
    a = ap.parse_args()

    try:
        ser = serial.Serial(a.port, a.baud, timeout=0.1)
    except serial.SerialException as e:
        sys.exit(f"port acilamadi: {e}")

    sj = SahteJetson(ser, a.dinle)
    threading.Thread(target=sj.alici, daemon=True).start()
    if not a.dinle:
        threading.Thread(target=sj.gonderici, daemon=True).start()

    print(__doc__.split("KULLANIM")[0])
    print(f"port: {a.port} @ {a.baud}"
          + ("   [YALNIZ DINLEME]" if a.dinle else ""))
    print("komut icin: h/a/f/d/e/E/s/c/?/q  — 'q' cikis\n")

    try:
        while sj.calisiyor:
            try:
                satir = input("> ").strip()
            except EOFError:
                break
            if not satir:
                print("  " + sj.ozet())
                continue
            k, _, arg = satir.partition(" ")
            with sj.kilit:
                if k == "q":
                    break
                elif k == "h":
                    sj.hiz = float(arg or 0)
                    sj.dur_gonder = False
                    print(f"  hiz = {sj.hiz:+.3f} m/s")
                elif k == "a":
                    sj.aci = float(arg or 0)
                    sj.dur_gonder = False
                    print(f"  aci = {sj.aci:+.2f}°  (ROS isareti: + SOLA)")
                elif k == "f":
                    sj.fren = max(0, min(1000, int(arg or 0)))
                    print(f"  fren = {sj.fren}‰")
                elif k == "d":
                    sj.hiz, sj.dur_gonder = 0.0, True
                    print("  DUR gonderiliyor")
                elif k == "E":
                    sj.estop = True
                    print("  Jetson E-STOP ILAN EDILDI")
                elif k == "e":
                    sj.estop, sj.dur_gonder = False, True
                    print("  E-STOP kaldirildi — hareket icin TAZE komut ver (h ...)")
                elif k == "s":
                    sj.sus = True
                    print(f"  SUSTUM. Kart {700} ms sonra guvenli tarafa dusmeli:"
                          " gaz rolanti + tam fren.")
                elif k == "c":
                    sj.sus = False
                    print("  yeniden gonderiliyor")
                elif k == "?":
                    print("  " + sj.ozet())
                else:
                    print("  bilinmeyen komut")
    except KeyboardInterrupt:
        pass
    finally:
        sj.calisiyor = False
        time.sleep(0.15)
        try:
            ser.write(paket(PKT_DUR))
            ser.close()
        except Exception:
            pass
        print("\ncikildi (DUR gonderildi).")


if __name__ == "__main__":
    main()
