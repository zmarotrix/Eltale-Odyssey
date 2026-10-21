param([int]$Fire = 8, [int]$Earth = 34, [int]$Water = 8, [int]$Wind = 8, [int]$Hp = 160, [int]$Mp = 50,
      [int]$Defense = 18, [int]$Agility = 18)
# Zelse difficulty test (RAM only; never save afterwards). Gives Brian a typical end-of-beat-2 build (pacing plan:
# 44 spirits plus about 10 battle levels, focused on Earth, Zelse's weakness), the gear he could own by then, Earth
# attuned to tier 4 and Water to tier 2 with 22 Spirit Points left for talents, marks Solvaring beaten (charm slot),
# then warps to Zelse's map and puts Brian just outside the boss's trigger range, facing it.
# Back up the save first (dev panel Saves, or POST /api/saves/backup).
$h = @{ "X-Q64-Dev" = "1" }
function m($a, $n) { ((Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:43064/api/mem?addr=0x$a&len=$n" -Headers $h -TimeoutSec 2).Content | ConvertFrom-Json).hex }
function wr([string]$a, [string]$hex) { $n = [Convert]::ToUInt32($a, 16); Invoke-WebRequest -UseBasicParsing -Method Post -Uri "http://127.0.0.1:43064/api/mem" -Headers $h -ContentType "application/json" -Body "{`"addr`":$n,`"hex`":`"$hex`"}" | Out-Null }
function f($hex) { [BitConverter]::ToSingle([BitConverter]::GetBytes([Convert]::ToUInt32($hex, 16)), 0) }
function fh([single]$v) { ([BitConverter]::ToUInt32([BitConverter]::GetBytes($v), 0)).ToString("x8") }
function at([uint32]$base, [int]$off) { ($base + $off).ToString("x8") }
$map = Get-Content "G:\Projects\Quest 64 PC\Eltale-Odyssey\mod\build\mod.map"
$geo = [Convert]::ToUInt32(((($map | Select-String " gEo$").Line -split '\s+')[0]), 16)

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

# Zelse's field location (boss table 0x803A9AD0, record 1): map, submap, x, z.
$loc = m "803A9AE8" 24
$zmap = [Convert]::ToInt32($loc.Substring(0, 4), 16); $zsub = [Convert]::ToInt32($loc.Substring(4, 4), 16)
$zx = f $loc.Substring(24, 8); $zz = f $loc.Substring(32, 8)
wr '80085368' (('{0:x8}{1:x8}00000000') -f $zmap, $zsub)
wr '8007BA4C' '00000108'
wr '8007B2EC' '0000001e'
$gs = [Convert]::ToUInt32((m "8007B2E4" 4), 16) -bor 0x80
wr '8007B2E4' ($gs.ToString("x8"))
Start-Sleep 6
"map now " + (m "80084EE4" 2) + " (Zelse's is {0:x4})" -f $zmap
# Brian 170 units from Zelse (trigger radius 130), on the side he arrived from, facing the boss.
$b = m "8007BACC" 12; $bx = f $b.Substring(0, 8); $bz = f $b.Substring(16, 8)
$dx = $bx - $zx; $dz = $bz - $zz; $len = [math]::Sqrt($dx * $dx + $dz * $dz)
if ($len -lt 1) { $dx = 0; $dz = 1; $len = 1 }
$px = $zx + 170 * $dx / $len; $pz = $zz + 170 * $dz / $len
wr "8007BACC" (fh $px); wr "8007BAD4" (fh $pz)
wr "8007BADC" (fh ([math]::Atan2($zx - $px, $zz - $pz)))
"Brian at ({0:n0}, {1:n0}), Zelse at ({2:n0}, {3:n0}); walk forward to start the fight" -f $px, $pz, $zx, $zz
(Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:43064/api/brian" -Headers $h).Content
