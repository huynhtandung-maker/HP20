# GitHub Reconciliation · HP20 v0.9.6

Current release-candidate ZIP is **not automatically a GitHub release**.

Recommended sequence after local board validation:

1. `git checkout main && git pull origin main`
2. create a new branch from current `main`;
3. copy/sync this v0.9.6 source into the branch;
4. confirm `secrets.h` is absent/untracked;
5. commit + push branch;
6. open Pull Request;
7. wait for GitHub Actions (host tests + SH1106 + SSD1306) PASS;
8. merge only after physical ESP32 tests PASS;
9. then create tag `v0.9.6` and GitHub Release.

Do not force-push or rewrite repository history. If a real secret ever appeared in public history, rotate it first; deleting it only from HEAD is not sufficient.
