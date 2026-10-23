# Settle test (run in a battle, on Brian's turn). Each step waits for Brian's turn, then either casts the Earth
# spell C-left (Rock) or ends the turn with A, and logs the foes' HP and the Settle state. Expected: cast 1 full
# damage, cast 2 at 0.4x (Settled), a passed turn clears it, cast 3 full again.
$s = $PSScriptRoot
$h = @{ "X-Q64-Dev" = "1" }
function m($a, $n) { ((Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:43064/api/mem?addr=0x$a&len=$n" -Headers $h).Content | ConvertFrom-Json).hex }
function wr($a, $hex) { $n = [Convert]::ToUInt32($a, 16); Invoke-WebRequest -UseBasicParsing -Method Post -Uri "http://127.0.0.1:43064/api/mem" -Headers $h -ContentType "application/json" -Body "{`"addr`":$n,`"hex`":`"$hex`"}" | Out-Null }
function sym($name) { $map = Get-Content "G:\Projects\Quest 64 PC\Eltale-Odyssey\mod\build\mod.map"; (($map | Select-String " $name$").Line -split '\s+')[0] }
$settled = sym "settled"; $pending = sym "settle_pending"
function hps { (0..3 | ForEach-Object { [Convert]::ToInt32((m ((0x8007C9A2 + $_ * 0x128).ToString("X8")) 2), 16) }) }
function brian_turn { for ($i = 0; $i -lt 120; $i++) { if ((m "8008C592" 2) -eq "0001") { Start-Sleep -Milliseconds 800; return }; Start-Sleep -Milliseconds 150 } }
foreach ($i in 0..3) { $a = (0x8007C9A2 + $i * 0x128); if ((hps)[$i] -gt 0) { wr $a.ToString("X8") "03e8" } }   # 1000 HP each
wr "8007BA88" (m "8007BA8A" 2)   # Brian's MP refilled to max (gPlayerMainData +8 current, +0xA max)
foreach ($step in "cast", "cast", "pass", "cast") {
    brian_turn
    $before = hps
    "{0}: settled {1} pending {2} hp {3}" -f $step, (m $settled 1), (m $pending 1), ($before -join ",")
    if ($step -eq "cast") { & "$s\inp.ps1" "CLEFT@120" "wait@400" "A@120" | Out-Null } else { & "$s\inp.ps1" "A@120" | Out-Null }
    Start-Sleep 4
    $after = hps
    $dmg = 0; foreach ($i in 0..3) { $dmg += $before[$i] - $after[$i] }
    "   damage dealt {0}; settled now {1} pending {2}" -f $dmg, (m $settled 1), (m $pending 1)
}
