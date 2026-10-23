param([string]$Combo = "F", [int]$Charge = -1, [int]$Boost = 0, [int]$Levels = 0, [int]$Hp = 1000, [string]$Shot = "", [int]$Watch = 7000, [string]$After = "", [int]$AfterMs = 700, [int]$Dist = 30)
# Casts one spell in a battle and logs what it did (RAM only; never saves). Run on Brian's turn.
#   cast.ps1 -Combo FFE        presses: F Fire (C-up), E Earth (C-left), W Water (C-down), A Wind (C-right)
#   -Levels 70  sets every element level (so every tier is open)   -Charge 3  sets Spirit Charge
#   -Boost 2    D-up presses before casting   -Hp 1000  every live foe's HP   -Shot file.png  a screenshot of the menu
$s = $PSScriptRoot
$h = @{ "X-Q64-Dev" = "1" }
function m($a, $n) { ((Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:43064/api/mem?addr=0x$a&len=$n" -Headers $h).Content | ConvertFrom-Json).hex }
function wr($a, $hex) { $n = [Convert]::ToUInt32($a, 16); Invoke-WebRequest -UseBasicParsing -Method Post -Uri "http://127.0.0.1:43064/api/mem" -Headers $h -ContentType "application/json" -Body "{`"addr`":$n,`"hex`":`"$hex`"}" | Out-Null }
function sym($name) { $map = Get-Content "G:\Projects\Quest 64 PC\Eltale-Odyssey\mod\build\mod.map"; (($map | Select-String " $name$" | Select-Object -First 1).Line -split '\s+')[0] }
$burns = sym "burns"; $frost = sym "frost_turns"; $chargeAddr = sym "charge"
$buffs = "guard_turns", "wall_turns", "fog_turns", "haze_turns", "renew_turns", "storm_turns", "juggernaut", "confused" | ForEach-Object { @{ n = $_; a = (sym $_) } }
for ($i = 0; $i -lt 100; $i++) { if ((m "8008C592" 2) -eq "0001") { break }; Start-Sleep -Milliseconds 200 }
Start-Sleep -Milliseconds 600
if ($Levels -gt 0) { $l = $Levels.ToString("x2"); wr "8007BAA4" "$l$l$l$l" }
wr "8007BA88" "00c8"; wr "8007BA8A" "00c8"   # MP 200/200 (RAM only)
wr "8007BBBE" "03e7"   # in-battle agility 999: tests hit instead of rolling against the foe's dodge
if ($Charge -ge 0) { wr $chargeAddr $Charge.ToString("x2") }
function hps { (0..5 | ForEach-Object { [Convert]::ToInt32((m ((0x8007C9A2 + $_ * 0x128).ToString("X8")) 2), 16) }) }
$before = hps
foreach ($i in 0..5) { if ($before[$i] -gt 0) { wr (0x8007C9A2 + $i * 0x128).ToString("X8") $Hp.ToString("x4") } }
function state {
    $hp = (hps) -join ","
    $st = (0..2 | ForEach-Object { $p = [Convert]::ToUInt32((m ((0x8007CA24 + $_ * 0x128).ToString("X8")) 4), 16); if ($p -ne 0) { m ($p.ToString("X8")) 2 } else { "----" } }) -join ","
    "hp $hp | status $st | burns " + (m $burns 42) + " | frost " + (m $frost 6) + " | charge " + (m $chargeAddr 1) + " | mp " + (m "8007BA88" 2) + " | brianhp " + (m "8007BA84" 2) + " | " + (($buffs | ForEach-Object { $_.n.Substring(0, 4) + "=" + (m $_.a 1) }) -join " ")
}
# Face the first live foe (yaw at battle data +0x10; 0 faces +z), so the spell flies at it.
function f($hex) { [BitConverter]::ToSingle([BitConverter]::GetBytes([Convert]::ToUInt32($hex, 16)), 0) }
function fh([single]$v) { ([BitConverter]::ToUInt32([BitConverter]::GetBytes($v), 0)).ToString("x8") }
$t = (0..5 | Where-Object { (hps)[$_] -gt 0 } | Select-Object -First 1)
$bp = m "8007BACC" 12; $ep = m ((0x8007C9BC + $t * 0x128).ToString("X8")) 12
# Bring the pack in: the target 30 units from Brian, the others keeping their places around it.
$bx = f $bp.Substring(0, 8); $bz = f $bp.Substring(16, 8); $ex = f $ep.Substring(0, 8); $ez = f $ep.Substring(16, 8)
$d = [math]::Sqrt(($ex - $bx) * ($ex - $bx) + ($ez - $bz) * ($ez - $bz))
if ($d -gt $Dist + 2) {
    $k = ($d - $Dist) / $d; $mx = ($bx - $ex) * $k; $mz = ($bz - $ez) * $k
    foreach ($i in 0..5) {
        if ((hps)[$i] -gt 0) {
            $a = 0x8007C9BC + $i * 0x128; $q = m $a.ToString("X8") 12
            wr $a.ToString("X8") (fh ((f $q.Substring(0, 8)) + $mx)); wr ($a + 8).ToString("X8") (fh ((f $q.Substring(16, 8)) + $mz))
        }
    }
    $ep = m ((0x8007C9BC + $t * 0x128).ToString("X8")) 12
}
$yaw = [math]::Atan2((f $ep.Substring(0, 8)) - (f $bp.Substring(0, 8)), (f $ep.Substring(16, 8)) - (f $bp.Substring(16, 8)))
wr "8007BADC" (fh $yaw)
"before: " + (state)
$keys = @{ "F" = "CUP"; "E" = "CLEFT"; "W" = "CDOWN"; "A" = "CRIGHT" }
foreach ($c in $Combo.ToCharArray()) { & "$s\inp.ps1" "$($keys[[string]$c])@100" "wait@150" | Out-Null }
for ($b = 0; $b -lt $Boost; $b++) { & "$s\inp.ps1" "DUP@100" "wait@150" | Out-Null }
Start-Sleep -Milliseconds 300
if ($Shot) { & "$s\shot.ps1" $Shot | Out-Null }
& "$s\inp.ps1" "A@120" | Out-Null
$last = ""
$sw = [Diagnostics.Stopwatch]::StartNew()
if ($After) { Start-Sleep -Milliseconds $AfterMs; & "$s\shot.ps1" $After | Out-Null }
while ($sw.ElapsedMilliseconds -lt $Watch) {
    $line = state
    if ($line -ne $last) { "{0,5} {1}" -f $sw.ElapsedMilliseconds, $line; $last = $line }
    Start-Sleep -Milliseconds 150
}
