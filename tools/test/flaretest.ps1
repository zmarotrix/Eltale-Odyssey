# Ember Staff detonation test setup: equips the Ember Staff (RAM only), burns enemy 0, moves it in front of Brian with
# enemy 1 just behind it, swings the staff, and logs HP and burns. Moving an enemy in memory doesn't always put it in
# the staff's reach; when the swing misses, the turn simply ends.
$s = $PSScriptRoot
$h = @{ "X-Q64-Dev" = "1" }
function m($a, $n) { ((Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:43064/api/mem?addr=0x$a&len=$n" -Headers $h).Content | ConvertFrom-Json).hex }
function wr($a, $hex) { $n = [Convert]::ToUInt32($a, 16); Invoke-WebRequest -UseBasicParsing -Method Post -Uri "http://127.0.0.1:43064/api/mem" -Headers $h -ContentType "application/json" -Body "{`"addr`":$n,`"hex`":`"$hex`"}" | Out-Null }
function f($hex) { [BitConverter]::ToSingle([BitConverter]::GetBytes([Convert]::ToUInt32($hex, 16)), 0) }
function fh([single]$v) { ([BitConverter]::ToUInt32([BitConverter]::GetBytes($v), 0)).ToString("x8") }
function sym($name) { $map = Get-Content "G:\Projects\Quest 64 PC\Eltale-Odyssey\mod\build\mod.map"; (($map | Select-String " $name$").Line -split '\s+')[0] }
$geo = sym "gEo"; $burns = sym "burns"
for ($i = 0; $i -lt 40; $i++) { if ((m "8008C592" 2) -eq "0001") { break }; Start-Sleep -Milliseconds 250 }
"state " + (m "8008C592" 2)
wr (([Convert]::ToUInt32($geo, 16) + 36).ToString("x8")) "01"
wr $burns "00030000000000000300"   # stacks[4] (u16), turns_left 3, fresh 0
$b = m "8007BACC" 12; $bx = f $b.Substring(0, 8); $bz = f $b.Substring(16, 8)
$e = m "8007C9BC" 12; $ex = f $e.Substring(0, 8); $ez = f $e.Substring(16, 8)
$d = [math]::Sqrt(($ex - $bx) * ($ex - $bx) + ($ez - $bz) * ($ez - $bz)); $ux = ($ex - $bx) / $d; $uz = ($ez - $bz) / $d
wr "8007C9BC" (fh ($bx + 12 * $ux)); wr "8007C9C4" (fh ($bz + 12 * $uz))
wr "8007CAE4" (fh ($bx + 30 * $ux)); wr "8007CAEC" (fh ($bz + 30 * $uz))
Start-Sleep -Milliseconds 200
& "$s\inp.ps1" "A@300" | Out-Null
$last = ""
foreach ($k in 1..40) {
    $line = "hp0 " + (m "8007C9A2" 2) + " hp1 " + (m "8007CACA" 2) + " burns " + (m $burns 16)
    if ($line -ne $last) { "t$k $line"; $last = $line }
    if ($k -eq 6) { & "$s\shot.ps1" "$s\flare.png" | Out-Null }
    Start-Sleep -Milliseconds 50
}
