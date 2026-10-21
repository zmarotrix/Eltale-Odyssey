param([int]$Map = 2, [int]$Submap = 0, [switch]$NoRestart)
# Restarts the game with the installed mods (quick start), warps to a map the way the Return spell does,
# then walks in a square until a random battle starts.
$s = $PSScriptRoot
$h = @{ "X-Q64-Dev" = "1" }
function m($a, $n) { ((Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:43064/api/mem?addr=0x$a&len=$n" -Headers $h -TimeoutSec 2).Content | ConvertFrom-Json).hex }
function wr([string]$addrHex, [string]$hex) {
    $a = [Convert]::ToUInt32($addrHex, 16)
    Invoke-WebRequest -UseBasicParsing -Method Post -Uri "http://127.0.0.1:43064/api/mem" -Headers $h -ContentType "application/json" -Body "{`"addr`":$a,`"hex`":`"$hex`"}" -TimeoutSec 3 | Out-Null
}
if (-not $NoRestart) {
    Stop-Process -Name Quest64Recompiled -Confirm:$false -ErrorAction SilentlyContinue
    Wait-Process -Name Quest64Recompiled -Timeout 10 -ErrorAction SilentlyContinue
    # Install the latest build of the mod (the closing game can hold the file for a moment).
    $nrm = "G:\Projects\Quest 64 PC\Eltale-Odyssey\mod\build\eltale_odyssey.nrm"
    for ($i = 0; $i -lt 20; $i++) {
        try { Copy-Item $nrm "$env:LOCALAPPDATA\Quest64Recompiled\mods\" -Force -ErrorAction Stop; break } catch { Start-Sleep -Milliseconds 500 }
    }
    Start-Sleep 1
    $r = "G:\Projects\Quest 64 PC\Quest64-Recomp"
    Start-Process -FilePath "$r\build-Release\Quest64Recompiled.exe" -ArgumentList "--dev-panel", "--quick-start" -WorkingDirectory $r | Out-Null
    for ($i = 0; $i -lt 40; $i++) { Start-Sleep -Milliseconds 500; try { if ((m "8007B2E0" 2) -eq "0001") { break } } catch {} }
    Start-Sleep 2
}
# Warp: pending map/submap/entrance, transition type 0x108, fade out.
wr '80085368' (('{0:x8}{1:x8}00000000') -f $Map, $Submap)
wr '8007BA4C' '00000108'
wr '8007B2EC' '0000001e'
$gs = [Convert]::ToUInt32((m "8007B2E4" 4), 16) -bor 0x80
wr '8007B2E4' ($gs.ToString("x8"))
Start-Sleep 5
$dirs = @("stick:0,-1@1500", "stick:1,0@1500", "stick:0,1@1500", "stick:-1,0@1500")
for ($k = 0; $k -lt 24; $k++) {
    & "$s\inp.ps1" $dirs[$k % 4] | Out-Null
    if (([Convert]::ToUInt16((m "8008C592" 2), 16) -band 1) -ne 0) { "battle started"; Start-Sleep 3; return }
}
"no battle"
