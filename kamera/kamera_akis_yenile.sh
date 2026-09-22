#!/bin/bash
# SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
# Copyright (C) 2026 Ahmet Efe Nezli
# kamera_akis.py'yi yeniden baslatir.
#
# 🔴 NEDEN BETIK DOSYASI: bu isi `ssh ... 'pkill -f kamera_akis.py; ...'`
# diye tek satirda yapmak CALISMIYOR. pkill -f komut satirlarinda arar ve
# uzaktaki bash'in kendi komut satirinda da o metin gecer — pkill yeni sureci
# baslatmadan once KENDI KABUGUNU olduruyor. Betigin icerigi komut satirinda
# gorunmedigi icin burada boyle bir catisma yok.
source /opt/ros/humble/setup.bash
source /home/lydia/lydia_ws/install/setup.bash
export ROS_DOMAIN_ID=42
cd /home/lydia

pkill -f "kamera_akis.py"
sleep 2
setsid nohup python3 -u /home/lydia/kamera_akis.py \
    --port 8095 --genislik 480 --kalite 50 \
    > /tmp/kamera_akis.log 2>&1 < /dev/null &
sleep 7
if ss -ltn | grep -q ":8095 "; then echo ":8095 ACIK"; else echo ":8095 KAPALI"; fi
head -8 /tmp/kamera_akis.log
