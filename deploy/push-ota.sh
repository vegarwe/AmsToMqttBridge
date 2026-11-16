#!/bin/bash
# Build and flash the firmware over WiFi, through the device's own HTTP upload
# (http://<device>/update, Basic auth from OTA_USER/OTA_PASS in config.h).
#
#   deploy/push-ota.sh <device-ip-or-hostname>
#
# The new image boots PENDING_VERIFY and only confirms itself once it reaches
# the broker; any reset before that rolls back to the previous image. So the
# real check is the retained version afterwards, not this script's "OK":
#
#   mosquitto_sub -h <broker> -p 8883 --cafile <ca.crt> -u <user> -P <pass> \
#       -t 'ams/+/up/version' -v -W 3

set -euo pipefail

host="${1:?usage: $0 <device-ip-or-hostname>}"
cd "$(dirname "$0")/.."

cfg=ArduinoCode/AmsToMqttBridge/config.h
image=.pio/build/featheresp32/firmware.bin

pio run

user=$(sed -n 's/^#define OTA_USER *"\(.*\)"/\1/p' "$cfg")
pass=$(sed -n 's/^#define OTA_PASS *"\(.*\)"/\1/p' "$cfg")
[ -n "$user" ] && [ -n "$pass" ] || { echo "OTA_USER/OTA_PASS not found in $cfg" >&2; exit 1; }

echo "Uploading $(stat -c %s "$image") bytes to http://$host/update"
# Credentials via a curl config on stdin, so they never show up in ps.
printf 'user = "%s:%s"\n' "$user" "$pass" |
    curl -K - --fail-with-body -sS --max-time 120 \
         -F "update=@$image;type=application/octet-stream" \
         "http://$host/update"
echo
echo "Device is restarting. Confirm with the retained ams/<MAC>/up/version."
