# HP20 v0.9.15 — Wi‑Fi Provisioning State Model

## Product principle

Normal Wi‑Fi loss is a reconnect problem, not a provisioning event.

```text
BOOT
  |
  +-- remembered Wi-Fi exists
  |      |
  |      +--> try preferred / last-good
  |      |       |
  |      |       +--> success --> normal operation
  |      |       |
  |      |       +--> timeout --> try next remembered network
  |      |
  |      +--> all unavailable --> wait briefly --> retry
  |                              |
  |                              +--> OLED: hold BOOT 3 s to change network
  |
  +-- no remembered Wi-Fi
         |
         +--> open HP20 captive setup AP
```

The open setup AP is not exposed automatically after ordinary transient loss once
the device has at least one remembered network.

## Explicit setup flow

```text
Hold BOOT 3 s
    |
    +--> AP HP20-xxxxxx, no password
    +--> DNS captive portal
    +--> OS probe redirect
    +--> scan nearby 2.4 GHz networks
    +--> mobile-first warm cream/orange UI
    |
Select network
    |
    +--> known network: stored password may be reused
    +--> new network: enter password, show/hide available
    |
Save
    |
    +--> SAVED
    +--> CONNECTING_WIFI
    +--> WIFI_CONNECTED / IP
    +--> SYNCING_TIME
    +--> CONNECTING_CLOUD
    +--> SUCCESS or FAILED
```

The web UI, OLED and buzzer are three views of the same state progression.

## Remembered networks

- Maximum: 5 profiles.
- Stored in existing `room-config` NVS JSON.
- Index 0: preferred / last-good network.
- Successful use moves a profile to index 0.
- New network is added and becomes preferred.
- Old v0.9.14 single `ssid/password` is migrated automatically.
- Passwords are never rendered back to the browser.
- `secrets.h` remains local and Git-ignored.

## Buzzer semantics

- Enter setup: two short pulses.
- Start Wi-Fi validation: one short pulse.
- Success: short + short + longer pulse.
- Failure: three attention pulses.

Thermal reminder sound is suspended while setup or OTA owns the user's attention.
