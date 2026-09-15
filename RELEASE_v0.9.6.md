# HP20 v0.9.6 — Release Candidate

## Scope

This release candidate closes the HP20 project baseline after provisioning, built-in ThingsBoard TLS trust, OTA foundation, and the final telemetry cadence decision.

## User-visible changes

- Firmware version: `0.9.6`.
- Standard ThingsBoard telemetry cadence: **5 minutes**.
- Setup portal exposes `5 phút (chuẩn)` and allows 5-minute persistence in NVS.
- Existing secure provisioning remains unchanged: Wi-Fi/password/token stay in ESP32 NVS or local ignored `secrets.h`.
- ThingsBoard OTA remains opt-in and uses HTTPS, title/version checks, SHA-256, size guard, and the inactive OTA partition.

## Release gate

Do not tag or publish v0.9.6 as final until all gates pass:

1. GitHub Actions passes host tests and both OLED firmware builds.
2. Current USB-flashed v0.9.5 device is online and OTA is enabled.
3. v0.9.6 binary is uploaded to ThingsBoard as Title `HP20`, Version `0.9.6`, Type `Firmware`.
4. Package is assigned to the single HP20 test device.
5. Device reports OTA states through `UPDATED` and reboots into `HP20 v0.9.6`.
6. Wi-Fi/token/NVS survive reboot.
7. Telemetry continues and the configured reporting interval can be set to 5 minutes.
8. Only then create Git tag `v0.9.6` and GitHub Release `HP20 v0.9.6`.

## Git release commands after OTA acceptance

```powershell
git checkout main
git pull origin main
git tag -a v0.9.6 -m "HP20 v0.9.6 - 5-minute telemetry and verified ThingsBoard OTA"
git push origin v0.9.6
```

Then create a GitHub Release from tag `v0.9.6` and attach the verified firmware binary plus `SHA256SUMS.txt` produced by GitHub Actions.
