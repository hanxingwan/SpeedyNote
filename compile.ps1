# Completely remove build directory and recreate
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path build -Force

Write-Host "🔨 Building SpeedyNote..." -ForegroundColor Green

# ✅ Compile .ts → .qm files
Write-Host "📝 Compiling translation files..." -ForegroundColor Yellow
& "C:\Qt\5.15.2\mingw81_32\bin\lrelease.exe" ./resources/translations/app_zh.ts ./resources/translations/app_fr.ts ./resources/translations/app_es.ts

Copy-Item -Path ".\resources\translations\*.qm" -Destination ".\build" -Force

cd .\build
Write-Host "⚙️ Configuring CMake..." -ForegroundColor Yellow
C:\msys64\mingw32\bin\cmake.exe -G "MinGW Makefiles" .. 

Write-Host "🔧 Compiling source code..." -ForegroundColor Yellow
C:\msys64\mingw32\bin\cmake.exe --build . -- -j16



# Check if build was successful
if (-not (Test-Path "NoteApp.exe")) {
    Write-Host "❌ Build failed! NoteApp.exe not found." -ForegroundColor Red
    cd ..
    exit 1
}

Write-Host "✅ Build successful! Deploying DLLs..." -ForegroundColor Green

# ✅ Deploy Qt runtime DLLs and plugins
Write-Host "📦 Deploying Qt DLLs and plugins..." -ForegroundColor Yellow
$windeployResult = & "C:\Qt\5.15.2\mingw81_32\bin\windeployqt.exe" "NoteApp.exe" --no-translations --no-system-d3d-compiler --no-opengl-sw --force --verbose



# ✅ Verify critical Qt plugins are present
Write-Host "🔍 Verifying Qt plugins..." -ForegroundColor Yellow
$criticalPlugins = @(
    "platforms\qwindows.dll",
    "imageformats\qjpeg.dll", 
    "imageformats\qpng.dll",
    "styles\qwindowsvistastyle.dll"
)

$missingPlugins = @()
foreach ($plugin in $criticalPlugins) {
    if (-not (Test-Path $plugin)) {
        $missingPlugins += $plugin
        Write-Host "  ❌ Missing: $plugin" -ForegroundColor Red
    } else {
        Write-Host "  ✅ Found: $plugin" -ForegroundColor Green
    }
}

# Manual copy if windeployqt failed
if ($missingPlugins.Count -gt 0) {
    Write-Host "🔧 Manually copying missing Qt plugins..." -ForegroundColor Yellow
    
    # Copy platforms directory
    if (-not (Test-Path "platforms")) {
        New-Item -ItemType Directory -Path "platforms" -Force
        Copy-Item "C:\Qt\5.15.2\mingw81_32\plugins\platforms\*.dll" "platforms\" -Force
        Write-Host "  ✅ Copied platforms plugins" -ForegroundColor Green
    }
    
    # Copy imageformats directory  
    if (-not (Test-Path "imageformats")) {
        New-Item -ItemType Directory -Path "imageformats" -Force
        Copy-Item "C:\Qt\5.15.2\mingw81_32\plugins\imageformats\*.dll" "imageformats\" -Force
        Write-Host "  ✅ Copied imageformats plugins" -ForegroundColor Green
    }
    
    # Copy styles directory
    if (-not (Test-Path "styles")) {
        New-Item -ItemType Directory -Path "styles" -Force
        Copy-Item "C:\Qt\5.15.2\mingw81_32\plugins\styles\*.dll" "styles\" -Force
        Write-Host "  ✅ Copied styles plugins" -ForegroundColor Green
    }
    
    # Copy other essential plugin directories
    $pluginDirs = @("iconengines", "generic", "networkinformation", "tls")
    foreach ($dir in $pluginDirs) {
        $sourcePath = "C:\Qt\5.15.2\mingw81_32\plugins\$dir"
        if ((Test-Path $sourcePath) -and -not (Test-Path $dir)) {
            New-Item -ItemType Directory -Path $dir -Force
            Copy-Item "$sourcePath\*.dll" "$dir\" -Force -ErrorAction SilentlyContinue
            Write-Host "  ✅ Copied $dir plugins" -ForegroundColor Green
        }
    }
}

Get-ChildItem -Path "..\32-bitdll\*.dll" | ForEach-Object {
    $destination = Join-Path "..\build" $_.Name
    if (-not (Test-Path $destination)) {
        Copy-Item -Path $_.FullName -Destination $destination
    }
}

# Copy share folder if it exists
if (Test-Path "..\share") {
    Write-Host "📁 Copying share folder..." -ForegroundColor Yellow
    Copy-Item -Path "..\share" -Destination ".\share" -Recurse -Force
} else {
    Write-Host "  ⚠️  Share folder not found, skipping..." -ForegroundColor Yellow
}

# ✅ Verify DLL architectures (diagnose 0xc000007b errors)
Write-Host "🔍 Checking DLL architectures..." -ForegroundColor Yellow

function Get-FileArchitecture {
    param([string]$FilePath)
    try {
        $bytes = [System.IO.File]::ReadAllBytes($FilePath)
        if ($bytes.Length -lt 64) { return "Unknown" }
        
        # Check PE header
        $peOffset = [BitConverter]::ToUInt32($bytes, 60)
        if ($peOffset + 4 -ge $bytes.Length) { return "Unknown" }
        
        $machineType = [BitConverter]::ToUInt16($bytes, $peOffset + 4)
        switch ($machineType) {
            0x014c { return "32-bit" }
            0x8664 { return "64-bit" }
            default { return "Unknown" }
        }
    } catch {
        return "Error"
    }
}

# Check main executable
$exeArch = Get-FileArchitecture "NoteApp.exe"
Write-Host "  📋 NoteApp.exe: $exeArch" -ForegroundColor Cyan

# Check all DLLs in current directory
$mismatchFound = $false
Get-ChildItem -Filter "*.dll" | ForEach-Object {
    $dllArch = Get-FileArchitecture $_.FullName
    if ($dllArch -eq "64-bit" -and $exeArch -eq "32-bit") {
        Write-Host "  ❌ MISMATCH: $($_.Name) is $dllArch but executable is $exeArch" -ForegroundColor Red
        $mismatchFound = $true
    } elseif ($dllArch -eq "32-bit") {
        Write-Host "  ✓ OK: $($_.Name) is $dllArch" -ForegroundColor Green
    } else {
        Write-Host "  ⚠️  $($_.Name): $dllArch" -ForegroundColor Yellow
    }
}



Write-Host "🚀 Launching SpeedyNote..." -ForegroundColor Green
./NoteApp.exe
cd ../