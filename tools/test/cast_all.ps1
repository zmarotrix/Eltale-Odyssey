param([string[]]$Combos, [int]$Levels = 70, [int]$Dist = 30)
# Casts a list of spells, one a turn, and prints what each did: "FFA:3" casts Fire+Fire+Wind with 3 Charge set
# first (finishers need 2). Foes are reset to 1000 HP before each cast; Burn, Frost and the rest carry over.
foreach ($c in $Combos) {
    $combo, $ch = $c -split ':'
    $charge = if ($ch) { [int]$ch } else { -1 }
    $out = & "$PSScriptRoot\cast.ps1" -Combo $combo -Charge $charge -Levels $Levels -Dist $Dist 2>$null
    "== $c"
    "   " + ($out | Select-Object -First 1)
    "   " + ($out | Select-Object -Last 1)
}
