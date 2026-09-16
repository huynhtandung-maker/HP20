# HP20 v0.9.16 — GitHub TLS Trust Fix & Explicit OTA Apply

## Purpose

This patch fixes the repeatable `GITHUB SUMS HTTP -1` failure seen when HP20 v0.9.14 tried to reach GitHub Release assets using a stale single-root trust anchor.

## Changes

- GitHub OTA trust changes from one DigiCert root to a strict multi-root PEM set:
  - ISRG Root X1
  - USERTrust ECC Certification Authority
  - USERTrust RSA Certification Authority
  - DigiCert Global Root G2 fallback
- No insecure TLS mode.
- Public HTTPS redirect hops still use a fresh TLS client per host.
- Up to three bounded retries for transient connection failures.
- Serial diagnostics identify context, host, hop and attempt.
- Background checks no longer auto-install firmware; they stop at `UPDATE_AVAILABLE`.
- Actual installation requires explicit `updateFirmware` RPC from the dashboard.
- Includes all v0.9.15 Wi-Fi provisioning UX improvements.

## Bridge validation path

Because v0.9.14 itself contains the broken GitHub trust set, v0.9.16 must be installed once through the known-good ThingsBoard-hosted binary path (or USB). After v0.9.16 is running, validate External URL OTA with the next release.

- Normalize `SHA256SUMS.txt` entries to release-asset basenames; CI now emits basename-only checksums and the device parser tolerates legacy path-prefixed entries.
