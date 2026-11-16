# Mosquitto user and ACL for the bridge (example)

The firmware image contains its broker credentials, so give the device its
own broker user and grant it only what it publishes. Then a leaked image is
worth no more than this device's own topics.

```sh
mosquitto_passwd -b /etc/mosquitto/passwd ams "$(openssl rand -base64 24)"
```

`/etc/mosquitto/aclfile` (with `acl_file` set in `mosquitto.conf`):

```
user ams
topic write AMSpub
topic write ams/<MAC>/up/#
topic read  ams/<MAC>/up/#
```

- `<MAC>` is the board's WiFi MAC in upper case with colons, as printed on the
  serial console at boot (`firmware 0x.., running from app0, ... mac ...`).
  Use `ams/+/up/#` until you know it.
- `up/#` must be writable: `up/online` is the device's last will, which the
  broker publishes on the client's behalf, and a will the client may not write
  is silently never sent.
- The read grant is only so the device user can be used to check its own
  output; the firmware subscribes to nothing.
- Whatever consumes the readings (Home Assistant, a display, a logger) needs
  `topic read AMSpub` on its own user.

Reload with `systemctl reload mosquitto`.

## Verify by reading back, not by reading the config

mosquitto drops a denied publish **silently**, and `mosquitto_pub` reports
success either way. Subscribe as a consumer, publish as `ams`, and see what
arrives:

```sh
mosquitto_sub -h <broker> -p 8883 --cafile ca.crt -u <consumer> -P '<pass>' -t AMSpub -R -v -W 3 &
mosquitto_pub -h <broker> -p 8883 --cafile ca.crt -u ams        -P '<pass>' -t AMSpub -m test
```

Use `-R` on the subscriber: otherwise a retained message already on the topic
arrives first and looks like a delivered publish.
