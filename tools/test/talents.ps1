param([string]$Set = "")
# Sets talent ranks in RAM only (never saves): talents.ps1 -Set "SMOKE=3,FLASHPOINT=1". Every other talent goes to 0.
# Ranks are 2 bits a talent in gEo.talent_ranks (eo.h's enum order).
$h = @{ "X-Q64-Dev" = "1" }
function wr($a, $hex) { $n = [Convert]::ToUInt32($a, 16); Invoke-WebRequest -UseBasicParsing -Method Post -Uri "http://127.0.0.1:43064/api/mem" -Headers $h -ContentType "application/json" -Body "{`"addr`":$n,`"hex`":`"$hex`"}" | Out-Null }
$root = "G:\Projects\Quest 64 PC\Eltale-Odyssey\mod"
$header = Get-Content "$root\src\eo.h" -Raw
$enum = [regex]::Match($header, "enum \{\s*// Fire\s*(TALENT_KINDLING[\s\S]*?)TALENT_COUNT").Groups[1].Value
$names = [regex]::Matches($enum, "TALENT_(\w+)") | ForEach-Object { $_.Groups[1].Value }
$map = Get-Content "$root\build\mod.map"
$geo = [Convert]::ToUInt32(((($map | Select-String " gEo$" | Select-Object -First 1).Line -split '\s+')[0]), 16)
$bytes = New-Object byte[] 16
foreach ($pair in ($Set -split ',' | Where-Object { $_ })) {
    $name, $rank = $pair -split '='
    $i = [array]::IndexOf($names, $name.Trim())
    if ($i -lt 0) { throw "no talent $name" }
    $bytes[[math]::Floor($i / 4)] = $bytes[[math]::Floor($i / 4)] -bor ([int]$rank -shl (($i % 4) * 2))
}
wr ($geo + 40).ToString("X8") (($bytes | ForEach-Object { $_.ToString("x2") }) -join "")
"talents set: $Set"
