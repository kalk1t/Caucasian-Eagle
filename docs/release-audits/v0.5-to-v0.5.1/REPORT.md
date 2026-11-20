# Windows trust audit: v0.5.0 → v0.5.1

[← Changelog](../../changelog.md)

2026-11-20. The owner: "examine why windows shows our app is not safe". Audited on one PC: Windows 11 Home 10.0.26300 x64, the owner's own user (not an administrator session). Microsoft Defender: antimalware 4.18.26100.4, engine 1.1.26100.4, signatures 1.463.386.0, real-time protection on. Smart App Control: off on this PC. File inventory with hashes: [`artifacts.json`](artifacts.json). Evidence (small text files, paths shown as `%USERPROFILE%`): [`evidence/`](evidence/). Tools: [`scripts/README.md`](scripts/README.md).

## Answer

Windows warns because **nothing of ours is signed**: the installer, `CaucasianEagle.exe` and the uninstaller have no Authenticode signature. So Windows cannot name a publisher ("Unknown publisher"), and SmartScreen has no reputation for the file: a new unsigned file is warned about until enough people have downloaded it, and every new version starts again from nothing. Where Smart App Control is on, it may block an unsigned program with no reputation outright. **No malware finding** is behind it: Defender finds nothing in the 0.5.0 or 0.5.1 files, the public 0.5.0 files are byte-identical to what the `v0.5.0` commit builds, the installer asks for no administrator rights, and it adds nothing that starts with Windows. Signing needs a code-signing identity only the owner can get; **the owner has decided to stay unsigned until version 1.0.0** and get a certificate then. Everything else for signing is ready.

## What the public v0.5.0 download is

The four release files were downloaded at 2026-11-20 21:38–21:40Z from their public links (one redirect each to GitHub's file host). Setup `5bb5677b…c4f` and zip `3e9f56a3…0fd` equal `dist/` from commit `ec0542b` (tag `v0.5.0`) and the published `.sha256` files. The untouched reference copies are kept outside Git (`build/release-audit/v0.5/original`, git-ignored); scans and the warning test used separate copies, deleted afterwards. The CDN's temporary signed parameters are removed from the kept download log.

At 21:40:09Z the zip's `.sha256` link answered 404. The repository had become private at about 21:39:33Z (not by this audit): every link of every release answered 404 to visitors. The owner made it public again; at 22:02Z all four links answered 200 and both checksums matched. v0.5.0's files were not changed.

## Findings

| ID | Severity | Warning or issue in v0.5 | Cause and evidence | Fix in v0.5.1 | Verification | Status |
|---|---|---|---|---|---|---|
| F1 | High | "Windows protected your PC" / unknown publisher on the installer (reported by the owner) | Setup.exe `NotSigned` (`Get-AuthenticodeSignature`); no signing certificate on the PC or in the project | Signing built in, not used: `tools/package.ps1 -Sign`, `tools/release/Sign-Files.ps1` | 0.5.1 Setup still `NotSigned` | BLOCKED — OWNER ACTION (deferred to 1.0.0 by the owner) |
| F2 | High | Smart App Control may block the game where it is on | `CaucasianEagle.exe` `NotSigned`; Microsoft: SAC blocks unsigned files without reputation | As F1 (the game is signed before the zip and installer take it) | Not testable here: SAC is off on this PC and cannot be turned on without a Windows reset | BLOCKED — OWNER ACTION |
| F3 | Medium | Uninstaller unsigned | `unins000.exe` `NotSigned` after install | Inno `SignedUninstaller=yes` under `-Sign` | Installed 0.5.1 uninstaller `NotSigned` | BLOCKED — OWNER ACTION |
| F4 | Medium | No signing step and no release gate: an unsigned or broken release could go out unnoticed | Packaging had no signing or checks | `tools/release/Test-Release.ps1`: versions, checksums, 2 GiB limit, shipped programs, signatures, Defender scan, public links | Fails the unsigned 0.5.1 (exit 1, 3 failures); passes with `-AllowUnsigned`; signer exits 2 when unconfigured | FIXED AND VERIFIED IN v0.5.1 |
| F5 | Medium | Signing itself has never run | No identity to sign with | `Sign-Files.ps1`: SHA-256, RFC 3161 timestamp, `signtool verify /pa`, certificate or Artifact Signing, no secrets on the command line or in Git (`signing.local.json`, `*.pfx`, `*.p12` git-ignored) | Not run with a real identity | IMPLEMENTED — VERIFICATION PENDING |
| F6 | Low | The installer's file properties had no copyright | `LegalCopyright` empty | `VersionInfo*`, `AppCopyright` in the installer script | 0.5.1 Setup: company, product, `© 2026 Giorgi Samkharadze`, 0.5.1 | FIXED AND VERIFIED IN v0.5.1 |
| F7 | — | SmartScreen warning on opening a downloaded installer | A copy marked as from the internet (Zone.Identifier ZoneId=3, the real URL) opened through the shell | — | 0.5.0 and 0.5.1: the wizard opened with no SmartScreen window on this PC (`evidence/*download-warning.json`); a browser download was not tested | NOT REPRODUCED |
| F8 | — | Antivirus detection | Defender custom scans of copies | — | 0.5.0 Setup and zip, 0.5.1 Setup and zip: no threats | NOT REPRODUCED |
| F9 | — | Administrator prompt (UAC) or anything left running with Windows | Per-user install (`PrivilegesRequired=lowest`), `asInvoker` manifest | — | Install, upgrade, uninstall: "Administrative install mode: No"; no startup entry, task, service or firewall rule; player data kept; after uninstall the system equals the start (`evidence/*diff*.txt`) | NOT REPRODUCED |
| F10 | Medium | Download links answered 404 | Repository private for about 20 minutes (owner's setting) | Owner made it public | 22:02Z: all four v0.5.0 links 200, checksums match | NOT REPRODUCED (no defect in the release) |
| F11 | Medium | Esc in the Ascent showed the testing panel, not the menu with settings | Lab-era pause text | The game's menu and settings window | Nature test opens both, `NATURE_PASS`; pictured (`evidence/v0.5.1-esc-menu.png`) | FIXED AND VERIFIED IN v0.5.1 |
| F12 | Low | Users had no way to check a download, and a checksum is easily taken for proof of origin | — | Manual: "Why Windows warns", how to compare `Get-FileHash` with the `.sha256`, and that a checksum proves integrity, not who made it | Text in `docs/manual.md` | FIXED AND VERIFIED IN v0.5.1 |
| F13 | Medium | The zip is close to GitHub's 2 GiB limit | 2,032 of 2,048 MiB; old development assets ship (debt D1) | — | Gate checks the limit | STILL PRESENT |
| F14 | Low | `Play-TheRoad.cmd` is an unsigned batch file | Batch files cannot carry Authenticode | — | — | STILL PRESENT |
| F15 | — | Browser download warnings (Edge, Chrome) | Not exercised: no browser download was made | — | — | NOT TESTED |
| F16 | Medium | The audit's own tool stopped unrelated processes | The first `Test-DownloadWarning.ps1` took every process started during its wait for the installer's and stopped them (on this PC: `dllhost` in the 0.5.0 run; `codex-command-runner`, `powershell`, `python`, `conhost`, `dllhost` in the first 0.5.1 run) | It now follows only the opened file's own process tree and SmartScreen | Rerun: only the Setup and its wizard seen and closed, nothing left running | FIXED AND VERIFIED IN v0.5.1 |

There is no updater in the game, so there is no update channel to secure.

## Owner actions

| Finding ID | Required action | Why Claude could not complete it | What is already prepared | Cost or prerequisite, if verified | How completion will be checked |
|---|---|---|---|---|---|
| F1, F2, F3, F5 | Get a code-signing identity (at 1.0.0, by the owner's decision) | It needs the owner's identity validation and payment | `tools/package.ps1 -Sign`, `tools/release/Sign-Files.ps1`, `signing.template.json`, the Inno signing hookup, the gate that fails unsigned files | Microsoft Artifact Signing: from $9.99 a month (Microsoft's SmartScreen page); individuals only in the USA and Canada, organisations in listed countries; validation 1–30 business days. Otherwise a certificate from a commercial CA (not priced here). Reputation still builds over time after signing | `tools\package.ps1 -Sign`, then `tools\release\Test-Release.ps1 -Version <v> -Scan` passes without `-AllowUnsigned` |

## Tests

Passed: Defender scans (4 files); download-warning test on 0.5.0 and 0.5.1 (opened); install of public 0.5.0, upgrade to 0.5.1, uninstall (system diffs, player data kept); first launch of the installed 0.5.0 and 0.5.1 (`ASCENT_PASS`); package smoke `ASCENT_PASS`, jars `WAYS_PASS`, `--road` window; nature with menu `NATURE_PASS`; release gate with `-AllowUnsigned`; gate failing unsigned files; signer refusing when unconfigured. Failed: none of the audit's. Known and unchanged: the creatures' tour `CREATURES_FAIL bad=4` (two groups off their ground, see techplan 08). Not run: signing with a real identity, Smart App Control on, a browser download, a second PC.

## Publication

The owner approved publishing 0.5.1 and keeping it unsigned until 1.0.0. v0.5.0 is untouched. A version once published is never changed; a fix is a new version.
