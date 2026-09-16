# HP20 v0.9.16 — GitHub TLS trust fix

## Root cause

HP20 v0.9.14 trusted GitHub public HTTPS with a single root certificate: `DigiCert Global Root G2`.
That assumption is no longer valid for the GitHub domains used by Release OTA. GitHub and its release CDN can terminate on different CA hierarchies.

Observed failure:

`GITHUB SUMS HTTP -1`

The failure occurs before the firmware binary is downloaded. ESP32 cannot establish the verified HTTPS connection needed to fetch `SHA256SUMS.txt`, so the safety gate correctly blocks the OTA.

## Fix

`github_ca.h` now supplies a strict concatenated PEM trust set to `WiFiClientSecure::setCACert()`:

- ISRG Root X1
- USERTrust ECC Certification Authority
- USERTrust RSA Certification Authority
- DigiCert Global Root G2 fallback

TLS verification remains enabled. `setInsecure()` is not used.

`hp20_ota.cpp` also adds bounded fresh-client retries and host/attempt diagnostics for negative HTTP connection errors.

## Behavior fix

Periodic/background OTA checks now only announce `UPDATE_AVAILABLE`. They no longer silently install a firmware release. The actual flash starts only after the explicit `updateFirmware` RPC from the dashboard.

## Migration constraint

The fix cannot repair the downloader already running inside v0.9.14. Therefore v0.9.16 must first reach the device through the known-good ThingsBoard-hosted binary path or USB. Once v0.9.16 is running, the next release can be used to validate GitHub External URL OTA end-to-end.
