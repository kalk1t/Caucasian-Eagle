# Saves a picture of the game's own window only (PrintWindow with its full rendered content), never the rest of the
# screen: for test runs that must show the HUD, which the game's own frame captures do not hold (release audit 2026-11-20).
#   tools\release\Save-GameWindow.ps1 -ProcessId <id> -Out picture.png
param([Parameter(Mandatory)] [int]$ProcessId, [Parameter(Mandatory)] [string]$Out)
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System; using System.Runtime.InteropServices;
public static class GameWindow {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
}
'@
$p = Get-Process -Id $ProcessId -ErrorAction Stop
$h = $p.MainWindowHandle
if ($h -eq [IntPtr]::Zero) { throw 'The game has no window yet.' }
$r = New-Object GameWindow+RECT; [void][GameWindow]::GetClientRect($h, [ref]$r)
$w = [Math]::Max(1, $r.R - $r.L); $ht = [Math]::Max(1, $r.B - $r.T)
$bmp = New-Object System.Drawing.Bitmap $w, $ht
$g = [System.Drawing.Graphics]::FromImage($bmp); $dc = $g.GetHdc()
[void][GameWindow]::PrintWindow($h, $dc, 3)   # PW_CLIENTONLY | PW_RENDERFULLCONTENT: the DirectX picture too
$g.ReleaseHdc($dc); $g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
"saved $Out ($w x $ht)"
