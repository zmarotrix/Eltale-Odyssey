# Finisher test (in a battle): raises Earth to 70 in RAM so tier 5 is open, then sets Charge to 0, 1, 2 and logs
# whether each tier-5 spell's level requirement is open (1) or locked (ff). Expected: locked at 0 and 1, open at 2.
# Restores Earth's level and Charge afterwards.
$h = @{ "X-Q64-Dev" = "1" }
function m($a, $n) { ((Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:43064/api/mem?addr=0x$a&len=$n" -Headers $h).Content | ConvertFrom-Json).hex }
function wr($a, $hex) { $n = [Convert]::ToUInt32($a, 16); Invoke-WebRequest -UseBasicParsing -Method Post -Uri "http://127.0.0.1:43064/api/mem" -Headers $h -ContentType "application/json" -Body "{`"addr`":$n,`"hex`":`"$hex`"}" | Out-Null }
function sym($name) { $map = Get-Content "G:\Projects\Quest 64 PC\Eltale-Odyssey\mod\build\mod.map"; (($map | Select-String " $name$").Line -split '\s+')[0] }
$charge = sym "charge"; $tiers = [Convert]::ToUInt32((sym "spell_tier"), 16)
$earthTiers = m (($tiers + 15).ToString("X8")) 15   # spell_tier[1][0..14]
"Earth tiers: $earthTiers"
$fin = @(); for ($i = 0; $i -lt 15; $i++) { if ($earthTiers.Substring($i * 2, 2) -eq "05") { $fin += $i } }
"tier-5 Earth slots: $($fin -join ',')"
$table = [Convert]::ToUInt32((m "800C1B18" 4), 16)   # SPELL_STAT_TABLES[1] (Earth)
$oldLevel = m "8007BAA5" 1; $oldCharge = m $charge 1
wr "8007BAA5" "46"
foreach ($c in "00", "01", "02") {
    wr $charge $c
    Start-Sleep -Milliseconds 400
    $reqs = $fin | ForEach-Object { m (($table + $_ * 0x44).ToString("X8")) 2 }
    "charge $c -> req $($reqs -join ',')"
}
wr "8007BAA5" $oldLevel; wr $charge $oldCharge
"restored Earth $oldLevel charge $oldCharge"
