# Scripts of the audit

They live in [`tools/release/`](../../../../tools/release/) so that every release uses them; what was run:

| Script | Used for |
|---|---|
| `Test-Release.ps1 -Version 0.5.1 -AllowUnsigned -Scan` | The release gate on the candidate (`evidence/v0.5.1-release-check.txt`); without `-AllowUnsigned` it fails unsigned files |
| `Test-Release.ps1 -Version 0.5.1 -Published` | After publishing: every public link answers 200 with its listed size and its checksum |
| `Test-DownloadWarning.ps1 -File <setup> -Url <public link> -Out <folder>` | Opens a copy marked as downloaded from the internet, as a double-click does; never removes the mark, never answers a warning; `-Picture` only on a test PC (it photographs the whole screen) |
| `Get-SystemSnapshot.ps1 -Out <file>` | Before and after install, upgrade and uninstall: startup entries, tasks, services, firewall rules, uninstall entry, shortcuts, install folder, player-data hashes |
| `Sign-Files.ps1 -Files <files>` | Signing (none configured yet: exits 2) |
| `Save-GameWindow.ps1 -ProcessId <id> -Out <png>` | A picture of the game's own window only |

Defender: `MpCmdRun.exe -Scan -ScanType 3 -File <copy> -DisableRemediation` on copies. Download: `curl.exe -L` of the public links, then `Get-FileHash`.
