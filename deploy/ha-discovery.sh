#!/bin/bash
# Publish Home Assistant MQTT discovery for the bridge's readings.
#
# Every message is retained, so HA picks the entities up whenever it next
# connects, including the first time, and nothing has to be configured in HA
# beyond its MQTT integration. Re-runnable: edit the entities here and publish
# again.
#
#   deploy/ha-discovery.sh publish     # create/update every entity
#   deploy/ha-discovery.sh remove      # delete them (empty retained payloads)
#   deploy/ha-discovery.sh show        # what is currently retained
#
# Needs mosquitto_pub/mosquitto_sub, and a broker user allowed to write under
# the discovery prefix (normally an admin user, not the device's or HA's).
#
# Settings come from the environment, or from deploy/ha-discovery.local.env
# (gitignored) next to this script:
#
#   MQTT_HOST=mqtt.example.lan         broker
#   MQTT_PORT=8883
#   MQTT_CAFILE=/path/to/ca.crt        empty for plain MQTT
#   MQTT_USER=admin
#   MQTT_PASS=...  or  MQTT_PASS_FILE=/path/to/file   (else prompted)
#   DEVICE_MAC=AA:BB:CC:DD:EE:FF       as in ams/<MAC>/up/version
#   PUBLISH_TOPIC=AMSpub               MQTT_PUBLISH_TOPIC from config.h
#   ROOT=ams                           MQTT_ROOT from config.h
#   PHASES="1 2 3"                     phases to make current/voltage sensors for
#   VOLTAGES="1 2 3"                   on an IT grid Kaifa reports U2 = 0: use "1 3"
#
# HA's own broker user needs read on PUBLISH_TOPIC and ROOT/<MAC>/up/#, and on
# the discovery prefix. Without the reads HA creates every entity and then
# receives no value for any of them, and the broker logs nothing about it.
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
[ -r "$here/ha-discovery.local.env" ] && . "$here/ha-discovery.local.env"

MQTT_HOST=${MQTT_HOST:?set MQTT_HOST}
MQTT_PORT=${MQTT_PORT:-8883}
MQTT_CAFILE=${MQTT_CAFILE:-}
MQTT_USER=${MQTT_USER:?set MQTT_USER}
DEVICE_MAC=${DEVICE_MAC:?set DEVICE_MAC}
PUBLISH_TOPIC=${PUBLISH_TOPIC:-AMSpub}
ROOT=${ROOT:-ams}
PHASES=${PHASES:-1 2 3}
VOLTAGES=${VOLTAGES:-$PHASES}
DISC=${DISC:-homeassistant}

MQTT_PASS=${MQTT_PASS:-}
if [ -z "$MQTT_PASS" ] && [ -n "${MQTT_PASS_FILE:-}" ]; then
    MQTT_PASS=$(tr -d '\r\n' < "$MQTT_PASS_FILE")
fi
if [ -z "$MQTT_PASS" ] && [ -t 0 ]; then
    read -rsp "password for $MQTT_USER: " MQTT_PASS; echo
fi
# Without this, a run with no TTY sends an empty password, and `show` hides
# the rejection behind -W.
[ -n "$MQTT_PASS" ] || { echo "No password: set MQTT_PASS or MQTT_PASS_FILE." >&2; exit 1; }

conn=(-h "$MQTT_HOST" -p "$MQTT_PORT" -u "$MQTT_USER" -P "$MQTT_PASS")
[ -n "$MQTT_CAFILE" ] && conn+=(--cafile "$MQTT_CAFILE")

macid=$(echo "$DEVICE_MAC" | tr -d ':' | tr 'A-F' 'a-f')
NODE="ams_$macid"
T="$ROOT/$DEVICE_MAC"

pub() { mosquitto_pub "${conn[@]}" -t "$1" -m "$2" -r; }

DEV="\"device\":{\"identifiers\":[\"$NODE\"],\"name\":\"Power meter\",\"model\":\"AmsToMqttBridge\",\"connections\":[[\"mac\",\"$DEVICE_MAC\"]]}"

# The device's last will. HA greys the entities out when the bridge goes away
# instead of showing its last reading as current.
AVL="\"availability\":[{\"topic\":\"$T/up/online\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\"}]"

# One entity per field of the meter frame.
#   sensor <key> <name> <field> <expr> <unit> <device_class> <state_class> <precision> <expire_after> [extra json]
#
# The frames differ in what they carry -- Kaifa sends P alone every 2 s, adds
# currents and voltages every 10 s, and the energy registers once an hour --
# so every template checks that its field is there, and renders empty when it
# is not. HA ignores an empty state for a numeric sensor and keeps the last
# value; a template that failed on the missing key would blank it instead.
#
# The id check is because PUBLISH_TOPIC has no device segment: with a second
# bridge on the same topic, each set of entities still only takes its own.
#
# expire_after covers the case the will cannot: the bridge is up but the meter
# has stopped talking (unplugged, HAN port turned off). About three frames.
sensor() {
    local key=$1 name=$2 field=$3 expr=$4 unit=$5 dclass=$6 sclass=$7 prec=$8 expire=$9 extra=${10:-}
    pub "$DISC/sensor/$NODE/$key/config" "{
      \"name\":\"$name\",\"unique_id\":\"${NODE}_$key\",
      \"state_topic\":\"$PUBLISH_TOPIC\",
      \"value_template\":\"{% if value_json.id == '$DEVICE_MAC' and value_json.data.$field is defined %}{{ $expr }}{% endif %}\",
      \"unit_of_measurement\":\"$unit\",\"device_class\":\"$dclass\",\"state_class\":\"$sclass\",
      \"suggested_display_precision\":$prec,\"expire_after\":$expire,
      ${extra:+$extra,}
      $DEV,$AVL}"
}

keys() {
    echo power reactive_power energy_import energy_export firmware
    for p in $PHASES;   do echo current_l$p; done
    for p in $VOLTAGES; do echo voltage_l$p; done
}

case "${1:-publish}" in
publish)
  sensor power          "Power"           P   "value_json.data.P"                        W   power          measurement      0 10
  sensor reactive_power "Reactive power"  Q   "value_json.data.Q"                        var reactive_power measurement      0 60 \
         '"enabled_by_default":false'

  # Energy dashboard. Wh on the wire, kWh here because that is what the
  # dashboard and tariffs are in. Hourly, hence the long expiry.
  sensor energy_import  "Energy import"   tPI "(value_json.data.tPI / 1000) | round(3)" kWh energy         total_increasing 2 7500
  # Only meaningful with production (solar); off until wanted.
  sensor energy_export  "Energy export"   tPO "(value_json.data.tPO / 1000) | round(3)" kWh energy         total_increasing 2 7500 \
         '"enabled_by_default":false'

  # Kaifa: mA and 0.1 V. (Aidon and Kamstrup scale differently; see
  # ArduinoCode/Libraries/HanToJson before reusing this for those.)
  for p in $PHASES; do
    sensor current_l$p "Current L$p" I$p "(value_json.data.I$p / 1000) | round(2)"  A current measurement 1 60
  done
  for p in $VOLTAGES; do
    sensor voltage_l$p "Voltage L$p" U$p "(value_json.data.U$p / 10) | round(1)"    V voltage measurement 1 60
  done

  # The bridge itself, from its retained boot announcement.
  pub "$DISC/sensor/$NODE/firmware/config" "{
    \"name\":\"Firmware\",\"unique_id\":\"${NODE}_firmware\",
    \"state_topic\":\"$T/up/version\",\"value_template\":\"{{ value_json.fw }}\",
    \"json_attributes_topic\":\"$T/up/version\",
    \"entity_category\":\"diagnostic\",\"icon\":\"mdi:chip\",
    $DEV,$AVL}"

  echo "published $(keys | wc -w) entities under $DISC/sensor/$NODE/"
  ;;

remove)
  # An empty retained payload is how discovery is deleted; HA removes the
  # entity when it sees it.
  for k in $(keys); do pub "$DISC/sensor/$NODE/$k/config" ""; done
  echo "removed."
  ;;

show)
  mosquitto_sub "${conn[@]}" -t "$DISC/sensor/$NODE/+/config" -v -W 3 || true
  ;;

*)
  echo "usage: $0 {publish|remove|show}" >&2
  exit 1
  ;;
esac
