#!/bin/bash
# SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
# Copyright (C) 2026 Ahmet Efe Nezli
# Kamera gozcusu — dusen kamera dugumunu PORT YOLUNDAN geri baglar.
#
# 🔴 NEDEN GEREKLI: iki web kamerasi (32e6:9211) kendiliginden USB'den dusup
# geri geliyor. Cekirdek gunlugu, 9 Eyl 2026 Mardin:
#     usb 1-2.3: USB disconnect, device number 8
#     usb 1-2.3: new high-speed USB device number 15 using tegra-xusb
# Her donusunde /dev/videoN DEGISIYOR (video4 -> video8). usb_cam dugumu
# acilista sabit bir /dev/videoN'e baglandigi icin cihaz altindan cekilince
# oluyor ve GERI GELMIYOR: kamera USB'de duruyor ama yayin bitmis oluyor.
# Kalici cozum kabloda; bu gozcu o cozume kadar ayakta tutar.
#
# Port yolu SABIT kaliyor (1-2.2 / 1-2.3), o yuzden esas alinan o.
#
#   nohup ./kamera_gozcu.sh > /tmp/kamera_gozcu.log 2>&1 &

source /opt/ros/humble/setup.bash
source /home/lydia/lydia_ws/install/setup.bash
export ROS_DOMAIN_ID=42 ROS_LOCALHOST_ONLY=0 RMW_IMPLEMENTATION=rmw_fastrtps_cpp
export FASTRTPS_DEFAULT_PROFILES_FILE=/home/lydia/lydia_ortam/udp_only.xml
KAM='-p image_width:=640 -p image_height:=480 -p pixel_format:=mjpeg2rgb -p framerate:=30.0 -p qos_history_policy:=keep_last -p qos_history_depth:=1'
LOG=/home/lydia/lydia_log; mkdir -p "$LOG"

: "${ON_PORT:=1-2.3}"
: "${ARKA_PORT:=1-2.2}"
: "${TARET_PORT:=1-2.1}"

# Port yolundan YAKALAMA aygitini bul. Bir kameranin birden cok /dev/videoN'i
# olur; yakalama olani index==0. Otekine baglanmak "veri gelmiyor" diye saatler
# yakiyor.
aygit() {
  local port="$1" v n
  for v in /dev/video*; do
    n=$(basename "$v")
    [ "$(cat "/sys/class/video4linux/$n/index" 2>/dev/null)" = "0" ] || continue
    case "$(readlink -f "/sys/class/video4linux/$n/device")" in
      *"/$port/"*|*"/$port:"*) echo "$v"; return 0;;
    esac
  done
  return 1
}

kur() {   # $1=aygit  $2=konu oneki  $3=dugum adi
  setsid nohup ros2 run usb_cam usb_cam_node_exe --ros-args \
    -r __node:="$3" -p camera_name:="$3" -p video_device:="$1" $KAM \
    -r /image_raw:="$2"/image_raw -r /camera_info:="$2"/camera_info \
    > "$LOG/$3.log" 2>&1 < /dev/null &
  echo "$(date '+%H:%M:%S') $3 <- $1 ($2)"
}

# 🔴 TARET KAMERASI TERS MONTAJLI (PTZ gibi). Cevirme KAYNAKTA yapiliyor:
#   usb_cam -> /camera/taret_ham/image_raw -> [cevir180] -> /camera/taret/image_raw
# Boylece ROS tarafindaki HERKES (YOLO, panolar, kamera_akis) duz kare goruyor
# ve kimsenin ayrica cevirmesi gerekmiyor. Iki yerde cevirmek goruntuyu iki kez
# dondurup basa getirir — kamera_akis.py'deki taret cevirmesi bu yuzden
# kaldirildi.
# ⚠ 180 donus BORESIGHT'IN ISARETINI DE TERS CEVIRIR: donmemis karede
#   (+27,-31) olan nokta donmus karede (-27,+31) olur. Ofset donmus karede
#   YENIDEN olculmeli (boresight.json "dogrulandi": false).
#
# ⚠ Onek "/camera/taret_ham" — "/camera/taret/ham" DEGIL. kur_dugum onegin
#   sonuna "/image_raw" ekliyor; ikincisi ".../taret/ham/image_raw" gibi bir
#   ad uretirdi ve cevirici baska bir konuyu dinlerdi.
TARET_HAM_ONEK=/camera/taret_ham
TARET_HAM_KONU="$TARET_HAM_ONEK/image_raw"

cevirici_bak() {
  pgrep -f "[c]evir180.py" > /dev/null && return 0
  [ -f /home/lydia/cevir180.py ] || return 0     # dosya yoksa sessiz gec
  setsid nohup python3 -u /home/lydia/cevir180.py \
      --girdi "$TARET_HAM_KONU" --cikti /camera/taret/image_raw \
      > "$LOG/cevir180.log" 2>&1 < /dev/null &
  echo "$(date '+%H:%M:%S') cevir180 <- $TARET_HAM_KONU -> /camera/taret/image_raw"
}

bak() {   # $1=dugum adi  $2=port  $3=konu oneki
  # 🔴 Ayakta olma kontrolu `__node:=` ile DEGIL `camera_name:=` ile yapiliyor.
  # Aciliş betiginin kaldirdigi taret dugumunde `__node:=` remap'i YOK, yalniz
  # `-p camera_name:=nisan_kamera` var; `__node:=` arayan bir gozcu onu goremez
  # ve cihazi zaten tutulan kamera icin IKINCI bir dugum acar. Iki dugum ayni
  # /dev/videoN'i acamaz, ikisi de bozulur.
  pgrep -f "camera_name:=$1" > /dev/null && return 0     # ayakta, dokunma
  local d; d=$(aygit "$2") || { echo "$(date '+%H:%M:%S') $1: port $2 bos, bekleniyor"; return 0; }
  # Cihaz gercekten acilabiliyor mu — USB'ye yeni donmus cihaz bir sure
  # mesgul kalabiliyor, erken baglanan dugum aninda oluyor.
  [ -r "$d" ] || return 0
  kur "$d" "$3" "$1"
}

echo "kamera gozcusu basladi — on=$ON_PORT arka=$ARKA_PORT taret=$TARET_PORT"
while true; do
  bak kam_on   "$ON_PORT"   /camera
  bak kam_arka "$ARKA_PORT" /camera/arka
  bak nisan_kamera "$TARET_PORT" "$TARET_HAM_ONEK"
  cevirici_bak
  sleep 5
done
