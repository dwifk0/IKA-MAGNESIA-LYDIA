#!/bin/bash
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (C) 2026 Ahmet Efe Nezli
# Kayit sunucusunu (BU PC'de) baslatir/durdurur.
#
# 🔴 BETIK DOSYASINDAN calismasi SART. `pkill -f kayit_sunucu.py` tek satirlik
# bir kabuk komutunun icinde verilince, kabugun KENDI komut satirinda da o
# metin gectigi icin pkill sureci baslatmadan once kabugu olduruyor. Bugun uc
# kez yasandi (exit 144). Betigin icerigi komut satirinda gorunmedigi icin
# burada boyle bir catisma yok.
#
#   bash kayit_ac.sh          → baslat
#   bash kayit_ac.sh kapat    → durdur
#
# Ortam degiskenleriyle: ARAC=<ARAC_IP> FPS=10 bash kayit_ac.sh
cd "$(dirname "$0")/../.."

pkill -f "kayit_sunucu.py"
sleep 1

if [ "${1:-}" = "kapat" ]; then
  echo "kayit sunucusu durduruldu"
  exit 0
fi

setsid nohup python3 -u genel/kamera/kayit_sunucu.py \
    --arac "${ARAC:-<ARAC_IP>}" --fps "${FPS:-8}" \
    > /tmp/kayit_sunucu.log 2>&1 < /dev/null &
sleep 4

if ss -ltn 2>/dev/null | grep -q ":8790 "; then
  head -2 /tmp/kayit_sunucu.log
  echo "✅ kayit sunucusu ACIK → http://localhost:8790"
else
  echo "🔴 acilmadi:"
  tail -10 /tmp/kayit_sunucu.log
fi
