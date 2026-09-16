# HP20 v0.9.15 — Wi‑Fi Provisioning UX + External URL OTA Acceptance

## Release objective

v0.9.15 gộp hai mục tiêu:

1. Biến Wi‑Fi onboarding/reconnect thành trải nghiệm sản phẩm rõ ràng, mobile-first.
2. Dùng chính v0.9.14 đang chạy trên thiết bị để kiểm chứng OTA v0.9.15 qua GitHub Release External URL, không upload `.bin` lần hai vào ThingsBoard.

## Commit

**Title**

```text
feat(wifi): redesign provisioning UX and prepare v0.9.15 URL OTA
```

**Body**

```text
HP20 v0.9.15 Wi-Fi provisioning UX and remote OTA validation release.

- Remember up to 5 Wi-Fi networks in NVS
- Reconnect automatically using last-good / remembered networks
- Replace throw-away setup AP password with physical-setup intent
- Add captive portal probes for mobile/desktop OSes
- Add warm cream/orange mobile-first setup UI
- Add Wi-Fi scan cards, RSSI guidance and remembered-network management
- Add show/hide Wi-Fi password
- Add real connection-state loading flow: Wi-Fi -> IP -> NTP -> ThingsBoard
- Add coordinated OLED and buzzer feedback during provisioning
- Preserve v0.9.14 HTTPS redirect hardening for GitHub External URL OTA
- Bump firmware version to 0.9.15

Validation target:
v0.9.14 -> v0.9.15 via GitHub Release External URL without USB or
ThingsBoard-hosted duplicate binary.
```

## GitHub Release

**Tag**

```text
v0.9.15
```

**Title**

```text
HP20 v0.9.15 — Wi‑Fi Provisioning UX + External URL OTA
```

**Release notes**

```text
HP20 v0.9.15

Purpose
- Redesign Wi-Fi provisioning and reconnect UX.
- Validate remote OTA from v0.9.14 to v0.9.15 using GitHub Release External URL.

Wi-Fi UX
- Remember up to 5 Wi-Fi networks in device NVS.
- Automatically retry remembered networks after connection loss.
- Open setup only on first provisioning or local BOOT hold.
- Open setup AP without a throw-away password.
- Captive portal probes for Android, Apple platforms and Windows.
- Warm cream / amber / orange mobile-first setup interface.
- Wi-Fi cards with RSSI, signal quality, recommended network and remembered status.
- Show/hide Wi-Fi password.
- Forget one network or reset all remembered networks.
- Real setup progress: Save -> Wi-Fi -> IP -> time sync -> ThingsBoard -> complete.
- OLED + buzzer feedback throughout setup.

OTA
- Preserve HTTPS, version gate, SHA-256 and inactive OTA partition.
- Preserve manual cross-host GitHub redirect handling introduced in v0.9.14.
- Acceptance target: v0.9.14 -> v0.9.15 through External URL only.
```

Upload the CI artifacts:

```text
HP20-v0.9.15-SH1106.bin
HP20-v0.9.15-SSD1306.bin
SHA256SUMS.txt
```

## ThingsBoard package — External URL only

Use:

```text
Title:       HP20
Version:     0.9.15
Version tag: HP20 0.9.15
Type:        Firmware
Source:      Use external URL
```

For the current SH1106 device:

```text
https://github.com/huynhtandung-maker/HP20/releases/download/v0.9.15/HP20-v0.9.15-SH1106.bin
```

Do **not** upload the `.bin` a second time into ThingsBoard for this acceptance test.

## OTA acceptance gates

Before update:

```text
current_fw_version = 0.9.14
fw_version         = 0.9.15
fw_url             = .../v0.9.15/HP20-v0.9.15-SH1106.bin
```

During update expect:

```text
CHECKING
-> DOWNLOADING
-> VERIFIED
-> UPDATING
-> reboot
```

After reboot:

```text
current_fw_version = 0.9.15
fw_version         = 0.9.15
fw_state           = UPDATED
fw_progress        = 100
fw_error           = ""
```

Then validate Wi-Fi UX separately:

1. Reboot on the current saved Wi-Fi: must reconnect without asking for credentials.
2. Turn router off/on: HP20 must retry automatically and must not expose setup AP by itself.
3. Hold BOOT 3 seconds: HP20 setup AP appears without password.
4. Phone joins HP20 AP: captive setup should auto-open where the OS supports it.
5. Portal shows nearby networks, RSSI quality and recommended network.
6. Password can be shown/hidden.
7. Save a new network: web + OLED + buzzer must show the real progress stages.
8. Reboot: the new network is remembered.
9. Return to an older remembered network environment: HP20 should retry remembered profiles automatically.
10. Use “Quên mạng” / “Quên tất cả” to explicitly reset selection.
