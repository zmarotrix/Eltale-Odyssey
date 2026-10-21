param([int]$Fire = 8, [int]$Earth = 34, [int]$Water = 8, [int]$Wind = 8, [int]$Hp = 160, [int]$Mp = 50,
      [int]$Defense = 18, [int]$Agility = 35, [switch]$SetupOnly, [switch]$FightOnly,
      [int]$Boss = 1)
# Zelse difficulty test (RAM only; never save afterwards). Gives Brian a typical end-of-beat-2 build (pacing plan:
# 44 spirits plus about 10 battle levels, focused on Earth, Zelse's weakness), the gear he could own by then, Earth
# attuned to tier 4 and Water to tier 2 with 22 Spirit Points left for talents, marks Solvaring beaten (charm slot),
# then warps to Zelse's map and puts Brian inside the boss's trigger range, so the fight starts.
# -Boss picks another boss's fight (0 Solvaring ... 6 Beigis, 7 Mammon); the build stays Zelse's.
# Back up the save first (dev panel Saves, or POST /api/saves/backup).
# -SetupOnly: stats, gear and items only, so talents can be bought (D-pad up) before the fight; then -FightOnly.
# Agility 35 gives about 82% to hit Zelse (agility 60): 100 x a / (a + 60 / 8). 18 gave 71% and felt like constant
# misses (playtest, 2026-10-21).
$h = @{ "X-Q64-Dev" = "1" }
function m($a, $n) { ((Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:43064/api/mem?addr=0x$a&len=$n" -Headers $h -TimeoutSec 2).Content | ConvertFrom-Json).hex }
function wr([string]$a, [string]$hex) { $n = [Convert]::ToUInt32($a, 16); Invoke-WebRequest -UseBasicParsing -Method Post -Uri "http://127.0.0.1:43064/api/mem" -Headers $h -ContentType "application/json" -Body "{`"addr`":$n,`"hex`":`"$hex`"}" | Out-Null }
function f($hex) { [BitConverter]::ToSingle([BitConverter]::GetBytes([Convert]::ToUInt32($hex, 16)), 0) }
function fh([single]$v) { ([BitConverter]::ToUInt32([BitConverter]::GetBytes($v), 0)).ToString("x8") }
function at([uint32]$base, [int]$off) { ($base + $off).ToString("x8") }
$map = Get-Content "G:\Projects\Quest 64 PC\Eltale-Odyssey\mod\build\mod.map"
$geo = [Convert]::ToUInt32(((($map | Select-String " gEo$").Line -split '\s+')[0]), 16)

if (-not $FightOnly) {
# Recovery items a player would carry into Zelse: 3 Healing Potions, 2 Honey Bread, 2 Heroes Drinks, 1 Mint Leaves
# (game item IDs), put into empty inventory slots (0x8008CF78, one item ID per byte, 0xFF = empty).
$inv = m "8008CF78" 0x96
$give = @(3, 3, 3, 2, 2, 7, 7, 6)
$k = 0
for ($i = 0; $i -lt 0x96 -and $k -lt $give.Count; $i++) {
    if ($inv.Substring($i * 2, 2) -eq "ff") { wr ((0x8008CF78 + $i).ToString("x8")) ($give[$k].ToString("x2")); $k++ }
}
"gave $k items"
# Spirits: tiers Fire 1, Earth 4, Water 2, Wind 1 (3 + 6 + 10 + 3 = 22 of 44 points spent), 22 left.
wr (at $geo 0x11) "16"
wr (at $geo 0x14) "01040201"
# Gear Brian could own before Zelse: Ember, Heavy, Stone, Tide staves, Traveler's Robe, Pilgrim's Soul, Gambler's Coin,
# Wool Cloak, Lead Mantle, Glass Charm. Worn: Stone Staff, Lead Mantle, Gambler's Coin (sigil slot opens after Zelse).
wr (at $geo 0x20) "080031af03 0e0900".Replace(" ", "")
# Solvaring beaten (defeated bits 0x8007D19C, bit 0).
$d = [Convert]::ToByte((m "8007D19C" 1), 16) -bor 1
wr "8007D19C" ($d.ToString("x2"))
# Stats (writing element levels makes the spirit system re-apply the tiers).
$body = @{ fire = $Fire; earth = $Earth; water = $Water; wind = $Wind; hp = $Hp; maxHp = $Hp; mp = $Mp; maxMp = $Mp;
           defense = $Defense; agility = $Agility } | ConvertTo-Json -Compress
Invoke-WebRequest -UseBasicParsing -Method Post -Uri "http://127.0.0.1:43064/api/brian" -Headers $h -ContentType "application/json" -Body $body | Out-Null

(Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:43064/api/brian" -Headers $h).Content
}
if ($SetupOnly) { "Set up. Buy talents (D-pad up), then run again with -FightOnly."; return }

# Zelse's field location (boss table 0x803A9AD0, record 1): map, submap, x, z.
$loc = m ((0x803A9AD0 + 0x18 * $Boss).ToString("x8")) 24
$zmap = [Convert]::ToInt32($loc.Substring(0, 4), 16); $zsub = [Convert]::ToInt32($loc.Substring(4, 4), 16)
$zx = f $loc.Substring(24, 8); $zz = f $loc.Substring(32, 8)
wr '80085368' (('{0:x8}{1:x8}00000000') -f $zmap, $zsub)
wr '8007BA4C' '00000108'
wr '8007B2EC' '0000001e'
$gs = [Convert]::ToUInt32((m "8007B2E4" 4), 16) -bor 0x80
wr '8007B2E4' ($gs.ToString("x8"))
Start-Sleep 6
"map now " + (m "80084EE4" 4) + " (Zelse's is {0:x4})" -f $zmap
# Brian 80 units from Zelse, inside the trigger radius (130), so the intro and fight start on their own. Spots just
# outside the radius are on ledges walled off from the clearing (playtest, 2026-10-21: an invisible wall), so the
# script doesn't leave Brian there. Zelse's field actor (0x8007D1A8) holds the boss's real position and height.
$za = m "8007D1A8" 48
$zx = f $za.Substring(72, 8); $zy = f $za.Substring(80, 8); $zz = f $za.Substring(88, 8)
$b = m "8007BACC" 12; $bx = f $b.Substring(0, 8); $bz = f $b.Substring(16, 8)
$dx = $bx - $zx; $dz = $bz - $zz; $len = [math]::Sqrt($dx * $dx + $dz * $dz)
if ($len -lt 1) { $dx = 0; $dz = 1; $len = 1 }
$px = $zx + 80 * $dx / $len; $pz = $zz + 80 * $dz / $len
wr "8007BACC" ((fh $px) + (fh ($zy + 5)) + (fh $pz))
wr "8007BADC" (fh ([math]::Atan2($zx - $px, $zz - $pz)))
"Brian at ({0:n0}, {1:n0}), Zelse at ({2:n0}, {3:n0}); the fight starts on its own" -f $px, $pz, $zx, $zz
(Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:43064/api/brian" -Headers $h).Content
# The fight puts Brian where he stood, which is inside a tree trunk; once it starts, move him to open ground in the
# clearing between the trees (playtest, 2026-10-21), facing Zelse.
# Zelse's intro dialogue holds the fight until A is pressed.
for ($i = 0; $i -lt 40; $i++) {
    if ((m "8008C592" 2) -ne "0000") { break }
    & "$PSScriptRoot\inp.ps1" "A@100" | Out-Null
    Start-Sleep -Milliseconds 400
}
Start-Sleep 1
wr "8007BACC" ((fh ($zx - 25)) + (fh ($zy + 3)) + (fh ($zz + 55)))
wr "8007BADC" (fh ([math]::Atan2(25, -55)))
"battle " + (m "8008C592" 2) + ": Brian moved to open ground"
