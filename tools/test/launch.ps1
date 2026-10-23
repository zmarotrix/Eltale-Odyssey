# Starts Quest 64: Recompiled with the dev panel and quick start (straight into the last save), and waits until the
# save is loaded and the field is up. Close the game first if a new mod build was just installed.
$h = @{ "X-Q64-Dev" = "1" }
Start-Process -FilePath "G:\Projects\Quest 64 PC\Quest64-Recomp\build-Release\Quest64Recompiled.exe" `
    -ArgumentList "--dev-panel", "--quick-start" -WorkingDirectory "G:\Projects\Quest 64 PC\Quest64-Recomp"
for ($i = 0; $i -lt 60; $i++) {
    try {
        $s = (Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:43064/api/status" -Headers $h -TimeoutSec 2).Content
        if ($s -match '"gameStarted":true') { break }
    } catch {}
    Start-Sleep 1
}
Start-Sleep 7
"started"
