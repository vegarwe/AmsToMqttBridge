## Arduino Code

Firmware and libraries for reading the HAN port of Norwegian AMS power meters
(Kaifa, Aidon, Kamstrup) and publishing the data as JSON over MQTT.

```
ArduinoCode/
├── AmsToMqttBridge/          The firmware (PlatformIO src_dir)
│   ├── AmsToMqttBridge.ino   setup()/loop(), HAN reading, JSON, rollback, boot announce
│   ├── wifi_mqtt.cpp/.h      WiFi connect/reconnect, TLS MQTT, last will
│   ├── SelfServiceWebServer  Web UI on port 80: power graph + OTA firmware upload
│   ├── config.h_             Template for local settings (copy to config.h)
│   └── config.h              Your settings - gitignored, never commit
└── Libraries/                (PlatformIO lib_dir)
    ├── HanReader/            Reads the serial stream, HDLC framing, CRC, DLMS/COSEM decoding
    ├── HanToJson/            Maps decoded lists to JSON per meter type
    └── HanConfigAp/          Access-point config portal + EEPROM storage (optional)
../deploy/
├── push-ota.sh               Build + flash over WiFi
├── ha-discovery.sh           Home Assistant entities via MQTT discovery
└── mosquitto-acl.md          Example broker user + ACL for the device
```

---

## ESP32 Feather build (current setup)

The original project used a custom ESP8266 + TSS721 PCB. This setup uses
off-the-shelf parts instead, no soldering of SMD parts needed:

| Part | Notes |
| --- | --- |
| [Adafruit HUZZAH32 – ESP32 Feather](https://www.adafruit.com/product/3405) | PlatformIO board `featheresp32`, CP2104 USB-serial |
| M-BUS to TTL board, v1.06.2 | TSS721-based M-Bus slave → 3.3 V UART. Cheap, from AliExpress |

### Wiring

```
 Meter HAN port (RJ45)          M-BUS to TTL board              ESP32 Feather
 ─────────────────────          ──────────────────              ─────────────
 pin 1  ───────────────────────  MBUS  (polarity
 pin 2  ───────────────────────  MBUS   does not matter)
                                 VCC  ─────────────────────────  3V
                                 GND  ─────────────────────────  GND
                                 TXD  ─────────────────────────  RX   (GPIO16)
                                 RXD  ─────────────────────────  TX   (GPIO17, unused, may be left open)
```

HAN RJ45 pin-out: see [NVE_Info_kunder_HANgrensesnitt.pdf](../Debugging/Documentation/NVE_Info_kunder_HANgrensesnitt.pdf).
The HAN port is 2400 baud, 8E1, and the meter pushes data on its own (Kaifa:
every 2 s for power, every 10 s for the full list, once an hour for cumulative energy).
With the M-Bus side unconnected the firmware runs normally and simply never
publishes a reading; device state (`up/online`, `up/version`) still appears.

**Why GPIO16/17 explicitly:** on the ESP32 Arduino core, `Serial1` without pin
arguments defaults to RX=GPIO26 on current cores (GPIO9 = flash on older ones),
which collides with the temperature sensor pin. The firmware uses `Serial2` with
explicit pins 16/17, which are the pins silk-screened RX/TX on the Feather.

**Temperature sensor: none for now (`TEMP_SENSOR_NONE`).** The original
DS18B20 (1-Wire) support is still there. An I²C sensor is planned instead —
an SHT41 for temperature + humidity. A DHT22 was considered and dropped: its
library keeps interrupts off for ~5 ms per read, which is known to upset the
ESP32 WiFi stack while associated.

### Toolchain (Linux)

PlatformIO Core is used for building. It downloads the ESP32 toolchain, the
Arduino core and the libraries in `platformio.ini` on first build.

```bash
# PlatformIO in its own virtualenv (same layout as the official installer)
python3 -m venv ~/.platformio/penv
~/.platformio/penv/bin/pip install -U platformio
ln -sf ~/.platformio/penv/bin/pio ~/.local/bin/pio

# Serial port access (log out and in again afterwards)
sudo usermod -aG dialout $USER
```

Pinned: `platformio/espressif32@8.0.0` (Arduino-ESP32 3.3.12), ArduinoJson 6.21,
256dpi/MQTT 2.5. Verified with PlatformIO Core 6.2.0.

Partition table is `min_spiffs.csv`: two 1.9 MB app slots for OTA. The image
is ~1.05 MB, which would be 80% of the default 1.25 MB slot. **The partition
table can only change over USB**, so it was set before the first flash.

### Configure

```bash
cp ArduinoCode/AmsToMqttBridge/config.h_ ArduinoCode/AmsToMqttBridge/config.h
$EDITOR ArduinoCode/AmsToMqttBridge/config.h
```

| Setting | Meaning |
| --- | --- |
| `USE_STATIC_CONFIG` | `1` = use the values in config.h. `0` = original behaviour: boot as access point `AMS2MQTT` and configure from a browser, stored in EEPROM |
| `WIFI_SSID` / `WIFI_PASS` | WiFi network |
| `METER_TYPE` | `1` Kaifa, `2` Aidon, `3` Kamstrup |
| `MQTT_HOST` / `MQTT_PORT` | Your broker |
| `MQTT_TLS` | `1` = TLS, verified against `root_ca_pem` (port 8883). `0` = plain MQTT (port 1883) |
| `MQTT_USER` / `MQTT_PASS` | Preferably a dedicated broker user, see [deploy/mosquitto-acl.md](../deploy/mosquitto-acl.md) |
| `MQTT_ROOT` | `ams`: device state goes to `ams/<MAC>/up/...` |
| `MQTT_PUBLISH_TOPIC` | `AMSpub`: the readings, the topic this project has always used |
| `OTA_USER` / `OTA_PASS` | HTTP Basic auth for the firmware upload |
| `root_ca_pem` | The CA that signed the broker certificate (TLS only) |

Compile-time hardware options are at the top of `AmsToMqttBridge.ino`:
`TEMP_SENSOR`, `TEMP_SENSOR_PIN`, `HAN_RX_PIN`/`HAN_TX_PIN`,
`IS_CUSTOM_AMS_BOARD` (for the original ESP8266 PCB), and `FIRMWARE_VERSION`
— **bump it on every image you flash**.

### Build, flash, monitor

From the repository root:

```bash
pio run                       # build
pio run -t upload             # build + flash over USB
pio device monitor            # 115200 baud, Ctrl-C to exit
```

**More than one ESP32 plugged in?** PlatformIO auto-detects the serial port,
and with several boards using the same USB chip it can pick the wrong one.
Pin the board in a `platformio_local.ini` next to `platformio.ini` (gitignored;
the format is at the end of `platformio.ini`), using its
`/dev/serial/by-id/...` path: that contains the USB chip's serial number and,
unlike `/dev/ttyUSB0`, does not move between boots.

The Feather auto-resets into the bootloader, no buttons needed.

### OTA (firmware over WiFi)

The firmware has always had an HTTP upload; it is now authenticated and has
rollback.

```bash
deploy/push-ota.sh <device-ip>
```

- `POST /update` (multipart, field `update`), HTTP Basic auth with
  `OTA_USER`/`OTA_PASS`. Credentials are checked when the upload *starts*, so
  an unauthenticated body never reaches flash. `GET /serverIndex` is the same
  as a browser form, behind the same auth.
- **Rollback**: a new image boots `PENDING_VERIFY` and only confirms itself
  once it has reached the broker. Any reset before that — a crash loop, a
  watchdog, pulling the plug — boots the previous image. So an image with a
  wrong WiFi or broker password undoes itself on the next reset.
- **How to tell what is running**: the retained `ams/<MAC>/up/version`
  carries `fw`, `partition` (`app0`/`app1`, alternates on every OTA) and
  `reset`. Reset the board after an OTA: back on the new version means it
  confirmed; back on the old means it never reached the broker.
- Plain HTTP on the LAN: Basic auth keeps out the casual, not someone sniffing
  the VLAN. The image carries the WiFi and broker passwords either way.

**Test it on the bench before the board goes to the basement:** USB-flash
`0x01`, bump `FIRMWARE_VERSION`, `push-ota.sh`, check `up/version` shows the
new version on the other partition, reset, check again.

### Check it works

```bash
mosquitto_sub -h <broker> -p 8883 --cafile ca.crt -u <user> -P '<pass>' \
    -t 'ams/+/up/#' -t AMSpub -v
```

---

## Runtime behaviour

1. Boot, print config to USB serial (115200), connect WiFi (retries forever),
   connect MQTT with a last will on `ams/<MAC>/up/online`.
2. Start the web server on port 80.
3. Once the broker is up: publish retained `up/online` = `online` and
   `up/version`, and confirm the running image (cancels rollback).
4. Read the HAN UART. On every complete, CRC-valid frame: decode, build JSON,
   publish to `AMSpub`.
5. HAN data is only read while MQTT is connected; frames arriving while
   disconnected are dropped.

The red LED (GPIO13) blinks once per received frame.

### MQTT topics

| Topic | Retained | Payload |
| --- | --- | --- |
| `AMSpub` | no | One reading per HAN frame, below |
| `ams/<MAC>/up/online` | yes | `online`; `offline` as the last will |
| `ams/<MAC>/up/version` | yes | `{"fw","partition","reset","mac","ip"}` |

### Reading payload (`AMSpub`)

```jsonc
{
  "id": "24:0A:C4:00:00:00",    // WiFi MAC
  "IP": "192.168.1.50",
  "up": 123456,                 // millis() since boot
  "t": 1763322000,              // meter timestamp, unix time
  "data": {
    "temp": 21.4,               // °C, only with a temperature sensor enabled
    "lv": "Kamstrup_V0001",     // list version
    "id": "5706567000000000",   // meter ID
    "type": "6861111",          // meter type
    "P": 1534,                  // active power import, W
    "Q": 0,                     // reactive power, VAr
    "I1": 412, "I2": 523, "I3": 301,   // current per phase
    "U1": 2310, "U2": 2298, "U3": 2305, // voltage per phase
    "tPI": 12345678,            // cumulative active import, Wh   (hourly)
    "tPO": 0,                   // cumulative active export, Wh   (hourly)
    "tQI": 1234, "tQO": 5678    // cumulative reactive import/export (hourly)
  }
}
```

Which fields are present depends on the list the meter sent (Kaifa's 2-second
list only has `P`). Values are the meter's raw integers; the scaling of current
and voltage differs between meter vendors (e.g. Kaifa sends mA and 0.1 V), see
the OBIS documents in [Debugging/Documentation](../Debugging/Documentation).

### Home Assistant

[`deploy/ha-discovery.sh`](../deploy/ha-discovery.sh) publishes retained MQTT
discovery configs, so the entities show up under one *Power meter* device as
soon as HA's MQTT integration is connected. Nothing to edit in HA, and no
firmware change: the entities read `AMSpub` and `ams/<MAC>/up/` as they are.

| Entity | From | Notes |
| --- | --- | --- |
| Power | `P` | W, every 2 s |
| Current L1–L3, Voltage L1–L3 | `I*`, `U*` | A and V, every 10 s |
| Energy import | `tPI` | kWh, `total_increasing`: use it in the Energy dashboard. Hourly |
| Energy export, Reactive power | `tPO`, `Q` | disabled by default |
| Firmware | `up/version` | diagnostic; partition, reset reason and IP as attributes |

All of them go *unavailable* through the device's last will when the bridge
drops off, and through `expire_after` when the bridge is up but the meter has
stopped sending. `AMSpub` is not retained, so after an HA restart the energy
sensor is unknown until the next hourly frame.

```bash
MQTT_HOST=... MQTT_CAFILE=ca.crt MQTT_USER=<admin> DEVICE_MAC=<MAC> \
    deploy/ha-discovery.sh publish          # or: remove, show
```

Settings can also live in `deploy/ha-discovery.local.env` (gitignored); the
script lists them. On an IT grid a Kaifa meter reports `U2 = 0` (it measures
L1–L2 and L3–L2), so set `VOLTAGES="1 3"` there. The scaling in the script is
Kaifa's; check it before using it with Aidon or Kamstrup.

HA's broker user needs `read` on `AMSpub` and `ams/<MAC>/up/#`, see
[mosquitto-acl.md](../deploy/mosquitto-acl.md).

### Web server (port 80)

| Path | Auth | |
| --- | --- | --- |
| `/` | open | Graph of active power (down-sampled history, last ~500 points) |
| `/powerData` | open | The same history as JSON |
| `/serverIndex` | Basic | OTA firmware upload form |
| `/update` | Basic | OTA upload endpoint (POST) |

---

## Libraries

### HanReader

Reads bytes from a `HardwareSerial`, finds HDLC frames (`0x7E ... 0x7E`),
checks header and frame CRC (`Crc16`), and exposes the DLMS list as indexed
values (`getInt`, `getString`, `getTime`). `Kaifa.h`, `Aidon.h` and
`Kamstrup.h` define the list layouts (which index is which OBIS value) for
each list size. `compensateFor09HeaderBug` works around a malformed date
header from some Kaifa meters.

### HanToJson

`hanToJson(data, meterType, hanReader)` picks the right layout from meter type
and list size and fills a `JsonObject` with the short keys shown above.

### HanConfigAp

Used when `USE_STATIC_CONFIG` is `0`: with no config in EEPROM the device starts
an open access point `AMS2MQTT` with a captive portal at `http://192.168.4.1/`
where WiFi, meter type and MQTT settings are entered. On the custom PCB, holding
the AP button (GPIO0) at boot re-enters this mode.

---

## Other code

* `../tmp/Esp32FeatherRestBridge` – an earlier experiment (untracked) posting
  to a REST endpoint instead of MQTT.
* `../Debugging/` – PC and Arduino tools for capturing and analysing raw HAN
  data, plus sample captures from all three meter vendors.
* `../ams_mqtt_to_csv.py`, `../AMS.py` – helper scripts (untracked) that log `t`
  and `P` from MQTT to CSV and plot it. They still point at the old broker.
