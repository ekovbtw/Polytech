# QuizMaster -- start Node.js server + localhost.run SSH tunnel
$host.UI.RawUI.WindowTitle = "QuizMaster"
$appPath = $PSScriptRoot

Write-Host ""
Write-Host "  ============================================" -ForegroundColor Cyan
Write-Host "   QuizMaster  --  server + tunnel" -ForegroundColor Cyan
Write-Host "  ============================================" -ForegroundColor Cyan
Write-Host ""

# -- Kill old processes --
Get-Process node -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process ssh  -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1

# -- 1. Start Node.js --
Write-Host "  [1/2] Starting Node.js server..." -ForegroundColor Yellow
$node = Start-Process -FilePath "node" `
    -ArgumentList "server.js" `
    -WorkingDirectory $appPath `
    -WindowStyle Minimized `
    -PassThru

Start-Sleep -Seconds 3

if ($node.HasExited) {
    Write-Host "  ERROR: Node.js failed to start." -ForegroundColor Red
    exit 1
}

$listening = netstat -ano | Select-String ":3000.*LISTEN"
if (-not $listening) {
    Write-Host "  ERROR: port 3000 is not open." -ForegroundColor Red
    exit 1
}
Write-Host "  OK -- server is listening on http://localhost:3000" -ForegroundColor Green

# -- 2. Open SSH tunnel --
Write-Host "  [2/2] Opening SSH tunnel via localhost.run..." -ForegroundColor Yellow

$keyPath = "$env:USERPROFILE\.ssh\id_rsa"

# Resolve TEMP to full Unicode path (avoids 8.3 short-path bug with Cyrillic usernames)
$tempDir = (Get-Item $env:TEMP -ErrorAction SilentlyContinue).FullName
if (-not $tempDir) { $tempDir = $env:TEMP }
$logOut  = Join-Path $tempDir "qm_tunnel_out.txt"
$logErr  = Join-Path $tempDir "qm_tunnel_err.txt"

Remove-Item $logOut -Force -ErrorAction SilentlyContinue
Remove-Item $logErr -Force -ErrorAction SilentlyContinue

$sshArgs = @(
    "-i", $keyPath,
    "-o", "StrictHostKeyChecking=no",
    "-o", "ServerAliveInterval=30",
    "-o", "ExitOnForwardFailure=yes",
    "-R", "80:localhost:3000",
    "localhost.run"
)

$tunnel = Start-Process -FilePath "ssh" `
    -ArgumentList $sshArgs `
    -RedirectStandardOutput $logOut `
    -RedirectStandardError  $logErr `
    -WindowStyle Hidden `
    -PassThru

# Wait for URL (up to 40 sec)
$url = $null
for ($i = 0; $i -lt 40; $i++) {
    Start-Sleep -Seconds 1
    $outTxt = ""
    $errTxt = ""
    if (Test-Path $logOut) { $outTxt = Get-Content $logOut -Raw -ErrorAction SilentlyContinue }
    if (Test-Path $logErr) { $errTxt = Get-Content $logErr -Raw -ErrorAction SilentlyContinue }
    $txt = "$outTxt$errTxt"
    if ($txt -match 'https://[a-z0-9\-]+\.lhr\.life') {
        $url = $Matches[0]
        break
    }
}

if (-not $url) {
    Write-Host "  ERROR: tunnel did not respond. Check internet connection." -ForegroundColor Red
    if (Test-Path $logErr) {
        Get-Content $logErr -ErrorAction SilentlyContinue | Select-Object -Last 5
    }
    exit 1
}

# -- Show result --
Write-Host ""
Write-Host "  ============================================" -ForegroundColor Green
Write-Host "   Site is available on the internet!" -ForegroundColor Green
Write-Host "  ============================================" -ForegroundColor Green
Write-Host ""
Write-Host "   Public URL:" -ForegroundColor White
Write-Host "   $url" -ForegroundColor Cyan
Write-Host ""
Write-Host "   Local:  http://localhost:3000" -ForegroundColor Gray
Write-Host ""
Write-Host "   Share the link with students." -ForegroundColor White
Write-Host "   Do not close this window -- site works while it is open." -ForegroundColor Yellow
Write-Host ""

Start-Process $url

# -- Watchdog --
Write-Host "  [Watchdog active] Press Ctrl+C to stop." -ForegroundColor DarkGray
Write-Host ""
while ($true) {
    Start-Sleep -Seconds 15

    if ($node.HasExited) {
        Write-Host "  [!] Node.js crashed, restarting..." -ForegroundColor Red
        $node = Start-Process -FilePath "node" `
            -ArgumentList "server.js" `
            -WorkingDirectory $appPath `
            -WindowStyle Minimized `
            -PassThru
        Start-Sleep -Seconds 3
        Write-Host "  [OK] Node.js restarted." -ForegroundColor Green
    }

    if ($tunnel.HasExited) {
        Write-Host "  [!] SSH tunnel died. URL is no longer accessible." -ForegroundColor Red
        Write-Host "      Close and run start.bat again." -ForegroundColor Yellow
        break
    }
}
