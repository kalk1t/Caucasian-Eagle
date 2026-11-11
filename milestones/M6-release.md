# M6 · Release (v1.0)

**Who:** You + Claude · **Needs:** M5 done; decisions P2 (price and store) and P6 (publisher name, icon) in [techplan/12-coverage-open-questions.md](../techplan/12-coverage-open-questions.md)

## Goal
Caucasian Eagle v1.0 is a product: signed, licensed, documented for players and published.

## Steps

### 1. Legal and credits (You + Claude)
- [ ] Confirm the Meshy plan and licence cover a released (and, if chosen, paid) game (risk R5).
- [ ] Licences of every shipped third-party item: DirectX Tool Kit, fonts, UI art, music and sound.
- [ ] Credits screen and a `LICENSES.txt` in the package.
- [ ] Note on the legend: Amirani is traditional Georgian folklore; credit the tradition and any specific retelling used.

### 2. Signing (You)
- [ ] Obtain a code-signing certificate (risk R3). Keep it outside the repository.
- [ ] Sign `CaucasianEagle.exe` and the Setup file in `tools\package.ps1`.

### 3. Store page (You + Claude)
- [ ] Choose the store (question P2). Prepare description, screenshots, trailer capture, minimum requirements.
- [ ] Privacy statement (the game collects nothing; say so).

### 4. Final checks (You + Claude)
- [ ] Full playthrough on at least two different PCs.
- [ ] The manual checklist, the clean-PC install test and the smoke test all pass on the release build.
- [ ] Player manual complete in English (and Georgian if P3 says so).
- [ ] Engine developer and validation modes are closed to players (question Q2).

### 5. Ship
- [ ] Tag `v1.0.0` in both repositories' terms: game tag, engine submodule commit recorded.
- [ ] Publish the release; keep the exact Setup file, zip and checksums archived.

## ✅ Done when
- [ ] A stranger can buy or download, install, finish and uninstall the game without help.
- [ ] Windows shows the publisher name instead of an unknown-publisher warning.
- [ ] Everything shipped has a recorded licence.
