param([int]$Port=8080)
$ErrorActionPreference='Stop'
for($attempt=0;$attempt -lt 80;$attempt++) {
    try {
        $health=Invoke-RestMethod -Uri "http://127.0.0.1:$Port/api/health" -TimeoutSec 1
        if($health.ok -and $health.service -eq 'MiniDiDi') { Start-Process "http://localhost:$Port"; exit 0 }
    } catch { }
    Start-Sleep -Milliseconds 250
}
exit 1
