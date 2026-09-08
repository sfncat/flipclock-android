# build-release.ps1 - Build signed release APK on Windows
# Usage: powershell -ExecutionPolicy Bypass -File scripts\build-release.ps1

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
Set-Location $ProjectRoot

Write-Host "=== FlipClock Android Release Build ===" -ForegroundColor Cyan

# 1. Check signing config
$PropsFile = Join-Path $ProjectRoot "keystore\keystore.properties"
$TemplateFile = Join-Path $ProjectRoot "keystore\keystore.properties.template"
if (-not (Test-Path $PropsFile)) {
    Write-Host "[!] keystore\keystore.properties not found, creating from template..." -ForegroundColor Yellow
    Copy-Item $TemplateFile $PropsFile
    Write-Host "    Please edit keystore\keystore.properties with real passwords and re-run." -ForegroundColor Yellow
    exit 1
}
$PropsContent = Get-Content $PropsFile -Raw
if ($PropsContent -match "your_store_password|your_key_alias") {
    Write-Host "[!] keystore.properties still has template placeholders, please fill in real passwords." -ForegroundColor Yellow
    exit 1
}
Write-Host "[+] Signing config loaded" -ForegroundColor Green

# 2. Check Java
if (-not $env:JAVA_HOME) {
    Write-Host "[!] JAVA_HOME not set, trying system java..." -ForegroundColor Yellow
    $JavaCmd = Get-Command java -ErrorAction SilentlyContinue
    if (-not $JavaCmd) {
        Write-Host "[X] Java not found. Please install JDK 21 and set JAVA_HOME." -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "[+] JAVA_HOME = $env:JAVA_HOME" -ForegroundColor Green
}

# 3. Read version
$GradleProps = Join-Path $ProjectRoot "gradle.properties"
$VersionName = "unknown"
$VersionCode = "unknown"
foreach ($line in Get-Content $GradleProps) {
    if ($line -match "^APP_VERSION_NAME=(.+)") { $VersionName = $Matches[1].Trim() }
    if ($line -match "^APP_VERSION_CODE=(.+)") { $VersionCode = $Matches[1].Trim() }
}
Write-Host "[+] Version: $VersionName (code $VersionCode)" -ForegroundColor Green

# 4. Build
Write-Host ""
Write-Host "Building release APK..." -ForegroundColor Cyan
$Gradlew = Join-Path $ProjectRoot "gradlew.bat"
& $Gradlew assembleRelease --no-daemon
if ($LASTEXITCODE -ne 0) {
    Write-Host "[X] Build failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}

# 5. Collect artifact
$ApkSrc = Join-Path $ProjectRoot "app\build\outputs\apk\release\app-release.apk"
if (-not (Test-Path $ApkSrc)) {
    Write-Host "[X] app-release.apk not found. Signing may have failed." -ForegroundColor Red
    exit 1
}

$ReleaseDir = Join-Path $ProjectRoot "releases"
if (-not (Test-Path $ReleaseDir)) { New-Item -ItemType Directory -Path $ReleaseDir | Out-Null }
$ApkDst = Join-Path $ReleaseDir "FlipClockV2-${VersionName}-release.apk"
Copy-Item $ApkSrc $ApkDst -Force

Write-Host ""
Write-Host "=== Build SUCCESS ===" -ForegroundColor Green
Write-Host "APK: $ApkDst"
Write-Host "Version: $VersionName (code $VersionCode)"

# 6. Verify signature
$SdkRoot = $env:ANDROID_HOME
if (-not $SdkRoot) { $SdkRoot = $env:ANDROID_SDK_ROOT }
if (-not $SdkRoot) { $SdkRoot = "$env:LOCALAPPDATA\Android\Sdk" }
if ($SdkRoot -and (Test-Path $SdkRoot)) {
    $BuildTools = Get-ChildItem "$SdkRoot\build-tools" -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending | Select-Object -First 1
    if ($BuildTools) {
        $Apksigner = Join-Path $BuildTools.FullName "apksigner.bat"
        if (Test-Path $Apksigner) {
            Write-Host ""
            Write-Host "Verifying signature..." -ForegroundColor Cyan
            & $Apksigner verify --print-certs $ApkDst
        }
    }
}
