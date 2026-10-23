# Freeze-window test (run in a battle, on Brian's turn): freezes enemy 0 for 1 turn in RAM, ends Brian's turn with A,
# and logs the battle state and enemy 0's freeze bit as they change. Expected: the bit stays on through the enemy's
# skipped turn AND Brian's next turn, and goes off when Brian ends that turn.
param([switch]$NoFreeze)
$s = $PSScriptRoot
$h = @{ "X-Q64-Dev" = "1" }
function m($a, $n) { ((Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:43064/api/mem?addr=0x$a&len=$n" -Headers $h).Content | ConvertFrom-Json).hex }
function wr($a, $hex) { $n = [Convert]::ToUInt32($a, 16); Invoke-WebRequest -UseBasicParsing -Method Post -Uri "http://127.0.0.1:43064/api/mem" -Headers $h -ContentType "application/json" -Body "{`"addr`":$n,`"hex`":`"$hex`"}" | Out-Null }
$statusPtr = (m "8007CA24" 4)   # enemy 0: slot 0x8007C998 + 0x24 battle data + 0x68 status pointer
$sp = [Convert]::ToUInt32($statusPtr, 16)
"status at {0:X8}" -f $sp
if (-not $NoFreeze) {
    wr "8007C9A2" "07d0"   # enemy 0 HP 2000
    $bits = [Convert]::ToUInt16((m ($sp.ToString("X8")) 2), 16) -bor 8
    wr ($sp.ToString("X8")) ($bits.ToString("x4"))
    wr (($sp + 5).ToString("X8")) "01"
}
$last = ""
$presses = 0
$sw = [Diagnostics.Stopwatch]::StartNew()
$idleSince = 0
while ($sw.ElapsedMilliseconds -lt 45000) {
    $state = m "8008C592" 2
    $st = m ($sp.ToString("X8")) 2
    $dur = m (($sp + 5).ToString("X8")) 1
    $line = "state $state status $st dur $dur"
    if ($line -ne $last) { "{0,6}ms {1}" -f $sw.ElapsedMilliseconds, $line; $last = $line; $idleSince = $sw.ElapsedMilliseconds }
    # End Brian's turn with A when it's been his turn (state 0001) for a moment, twice in all.
    if ($state -eq "0001" -and $presses -lt 2 -and ($sw.ElapsedMilliseconds - $idleSince) -gt 1500) {
        & "$s\inp.ps1" "A@120" | Out-Null; $presses++; "        A pressed"; $idleSince = $sw.ElapsedMilliseconds + 3000
    }
    Start-Sleep -Milliseconds 80
}
