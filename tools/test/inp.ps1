param([string[]]$Steps)
# Scripted controller input through the dev panel. Each step: "BUTTONS@ms" or "stick:x,y@ms" or "wait@ms".
#   inp.ps1 "stick:0,1@800" "A@100" "wait@500" "A+B@100"
$h = @{ "X-Q64-Dev" = "1" }
foreach ($step in $Steps) {
    $what, $ms = $step -split '@'
    $ms = if ($ms) { [int]$ms } else { 100 }
    if ($what -eq 'wait') { Start-Sleep -Milliseconds $ms; continue }
    $x = 0.0; $y = 0.0; $buttons = @()
    if ($what -like 'stick:*') {
        $xy = ($what -replace 'stick:', '') -split ','
        $x = [double]$xy[0]; $y = [double]$xy[1]
    } else {
        $buttons = $what -split '\+'
    }
    $body = @{ buttons = $buttons; x = $x; y = $y; ms = $ms } | ConvertTo-Json -Compress
    if ($buttons.Count -eq 1) { $body = $body -replace '"buttons":"([A-Z]+)"', '"buttons":["$1"]' }
    if ($buttons.Count -eq 0) { $body = $body -replace '"buttons":(\{\}|null|""|\[\])', '"buttons":[]' }
    Invoke-WebRequest -UseBasicParsing -Method Post -Uri "http://127.0.0.1:43064/api/input" -Headers $h -ContentType "application/json" -Body $body -TimeoutSec 3 | Out-Null
    Start-Sleep -Milliseconds ($ms + 80)
}
"done: $($Steps -join ' ')"
