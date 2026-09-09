$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$androidRoot = Join-Path $projectRoot "Android"
Set-Location $androidRoot

Write-Host "Building Shipwright Android APK (all configured architectures)..." -ForegroundColor Cyan
Write-Host "Project: $androidRoot" -ForegroundColor DarkGray
Write-Host ""

& .\gradlew.bat :app:assembleDebug --console=plain
$buildExitCode = $LASTEXITCODE

Write-Host ""
if ($buildExitCode -eq 0) {
    Write-Host "APK generated successfully." -ForegroundColor Green
    Write-Host "C:\Shipwright-Android\Android\app\build\outputs\apk\debug\SOH-Mobile-Anchor.apk" -ForegroundColor Green
} else {
    Write-Host "APK build failed with exit code $buildExitCode." -ForegroundColor Red
    exit $buildExitCode
}
