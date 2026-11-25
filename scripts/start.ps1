param([int]$Port=8080,[switch]$Lan,[switch]$NoBrowser,[switch]$WithMatlab)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $exe=Join-Path $root 'build\minididi.exe'
    if (!(Test-Path -LiteralPath $exe)) { & "$PSScriptRoot\build.ps1" }
    if (!(Test-Path -LiteralPath $exe)) { $exe=Join-Path $root 'build\Release\minididi.exe' }
    $listenAddress=if($Lan){'0.0.0.0'}else{'127.0.0.1'}
    if (!$NoBrowser) {
        # Open the page only after HTTP is ready, avoiding a first-start browser error.
        $openBrowserScript=Join-Path $PSScriptRoot 'open-browser.ps1'
        Start-Process -FilePath 'powershell.exe' -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$openBrowserScript+'"'),'-Port',"$Port") -WindowStyle Hidden
    }
    if ($WithMatlab) {
        $matlab=Get-Command matlab -ErrorAction SilentlyContinue
        if (!$matlab) { throw 'MATLAB is not on PATH. Run clients/matlab/run_twin.m from MATLAB.' }
        $matlabFolder=(Join-Path $root 'clients\matlab').Replace("'","''")
        Start-Process -FilePath $matlab.Source -ArgumentList @('-batch',"`"addpath('$matlabFolder'); pause(3); run_twin('http://127.0.0.1:$Port');`"") -WindowStyle Hidden
    }
    Write-Host "MiniDiDi: http://localhost:$Port (Ctrl+C stops server)"
    if($Lan){Get-NetIPAddress -AddressFamily IPv4 | Where-Object {$_.IPAddress -notmatch '^(127\.|169\.254\.)'} | ForEach-Object {Write-Host "Phone server address: http://$($_.IPAddress):$Port"}}
    & $exe --host $listenAddress --port $Port --web (Join-Path $root 'web')
    if ($LASTEXITCODE -ne 0) { throw 'Server exited with an error' }
} finally { Pop-Location }
