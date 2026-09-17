#!/bin/bash
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (C) 2026 Ahmet Efe Nezli
# Kameraları BUL ve düğümleri kur. Aygıt numarası değil PORT YOLU esas alınır.
#
# Neden: ön ve arka kamera aynı model, aynı VID:PID (32e6:9211) ve hatta aynı
# seri numarasına sahip (2024080515520033). udev bunları model ya da seriyle
# ayıramaz; ayırt eden TEK şey USB port yolu. /dev/videoN numaraları da her
# açılışta kayıyor (bir gün video11, ertesi gün video6 oldu).
#
# Kullanım:
#   ./kamera_baslat.sh              → yalnız haritayı yazar, hiçbir şey başlatmaz
#   ON=<port> ARKA=<port> ./kamera_baslat.sh --kur
#
# Örnek:  ON=2.3.2 ARKA=2.3.1 ./kamera_baslat.sh --kur

set -u
ORT='source /opt/ros/humble/setup.bash; source /home/lydia/lydia_ws/install/setup.bash; export ROS_DOMAIN_ID=42 ROS_LOCALHOST_ONLY=0 RMW_IMPLEMENTATION=rmw_fastrtps_cpp FASTRTPS_DEFAULT_PROFILES_FILE=/home/lydia/lydia_ortam/udp_only.xml'
KAM='-p image_width:=640 -p image_height:=480 -p pixel_format:=mjpeg2rgb -p framerate:=30.0 -p qos_history_policy:=keep_last -p qos_history_depth:=1'

# --- harita: port yolu -> yakalama aygıtı -------------------------------------
# Bir kameranın birden çok /dev/videoN'i olur (yakalama + metadata). Yakalama
# olanı ATTR{index}==0 ile ayrılıyor; ötekine bağlanmak "veri gelmiyor" diye
# saatler yakar.
harita() {
  for v in /dev/video*; do
    idx=$(udevadm info -q property -n "$v" 2>/dev/null | sed -n 's/^ID_V4L_PRODUCT=.*//p;s/^.*//p' >/dev/null; \
          cat "/sys/class/video4linux/$(basename "$v")/index" 2>/dev/null)
    [ "${idx:-1}" = "0" ] || continue
    model=$(udevadm info -q property -n "$v" 2>/dev/null | sed -n 's/^ID_MODEL=//p')
    yol=$(udevadm info -q path -n "$v" 2>/dev/null | grep -oE '[0-9]+-[0-9](\.[0-9]+)*' | tail -1)
    printf "%-14s port=%-10s model=%s\n" "$v" "${yol:-?}" "${model:-?}"
  done
}

aygit_bul() {   # $1 = port yolu (örn. 2.3.2 ya da 1-2.3.2)
  for v in /dev/video*; do
    [ "$(cat "/sys/class/video4linux/$(basename "$v")/index" 2>/dev/null)" = "0" ] || continue
    yol=$(udevadm info -q path -n "$v" 2>/dev/null | grep -oE '[0-9]+-[0-9](\.[0-9]+)*' | tail -1)
    case "$yol" in *"$1") echo "$v"; return 0;; esac
  done
  return 1
}

kur_dugum() {   # $1=aygıt $2=konu öneki $3=düğüm adı $4=log
  setsid nohup bash -c "$ORT; ros2 run usb_cam usb_cam_node_exe --ros-args \
    -r __node:=$3 -p camera_name:=$3 -p video_device:=$1 $KAM \
    -r /image_raw:=$2/image_raw -r /camera_info:=$2/camera_info" \
    > "/home/lydia/lydia_log/$4.log" 2>&1 < /dev/null &
}

echo "=== kamera haritası ==="
harita

[ "${1:-}" = "--kur" ] || { echo; echo "Kurmak için:  ON=<port> ARKA=<port> $0 --kur"; exit 0; }

ON_D=$(aygit_bul "${ON:?ON portu verilmedi}")   || { echo "HATA: ON portu ($ON) bulunamadı"; exit 1; }
ARKA_D=$(aygit_bul "${ARKA:?ARKA portu verilmedi}") || { echo "HATA: ARKA portu ($ARKA) bulunamadı"; exit 1; }
echo; echo "ön  : $ON_D   (port $ON)"; echo "arka: $ARKA_D  (port $ARKA)"

# 🔴 Yalnız kendi kurduğumuz iki düğüm kapatılıyor. Eskiden buradaki
# `pkill -f usb_cam_node_exe` TARET kamerasının düğümünü de öldürüyordu ve
# betik onu geri kaldırmıyordu — arka kamerayı kurmak taretin görüntüsünü
# götürüyordu.
pkill -f "[_]_node:=kam_on"; pkill -f "[_]_node:=kam_arka"; sleep 3
kur_dugum "$ON_D"   /camera       kam_on   kam_on
sleep 3
# 🔴 ARKA KAMERA /camera/taret'E DEĞİL /camera/arka'YA yayınlar. Eskiden
# taretin konusuna yazıyordu: panoda "arka kamera" diye taret görüntüsü
# çıkıyor, gerçek taret yayını da eziliyordu. (Aynı hata kamera_akis.py'de
# de vardı, 9 Eylül'de ikisi birden düzeltildi.)
kur_dugum "$ARKA_D" /camera/arka  kam_arka kam_arka
sleep 10

# Taret kamerası düşmüşse geri kaldır — bu betik onu artık öldürmüyor ama
# başka bir sebeple durmuş olabilir ve pano üç kamerayı da bekliyor.
pgrep -f "[c]amera_name:=nisan_kamera" >/dev/null || {
  setsid nohup bash -c "$ORT; ros2 run usb_cam usb_cam_node_exe --ros-args \
    -p camera_name:=nisan_kamera -p video_device:=/dev/kamera_nisan $KAM \
    -r /image_raw:=/camera/taret/image_raw \
    -r /camera_info:=/camera/taret/camera_info" \
    > /home/lydia/lydia_log/kam_taret.log 2>&1 < /dev/null &
  sleep 6; echo "taret kamerası geri kaldırıldı"
}
echo; echo "çalışan düğüm sayısı: $(pgrep -c -f '[u]sb_cam_node_exe')"

# Akış sunucusu ayakta değilse onu da kaldır.
pgrep -f "[k]amera_akis" >/dev/null || {
  setsid nohup bash -c "$ORT; python3 -u /home/lydia/kamera_akis.py --port 8095" \
    > /tmp/kamera_akis.log 2>&1 < /dev/null &
  sleep 6; echo "akış sunucusu başlatıldı (:8095)"
}
