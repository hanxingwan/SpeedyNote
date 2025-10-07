param(
    [switch]$old,
    [switch]$legacy
)

rm -r build
mkdir build


# ✅ Compile .ts → .qm files
& "C:\Qt\6.9.2\mingw_64\bin\lrelease.exe" ./resources/translations/app_zh.ts ./resources/translations/app_fr.ts ./resources/translations/app_es.ts


Copy-Item -Path ".\resources\translations\*.qm" -Destination ".\build" -Force

cd .\build

# Determine CPU architecture target
$cpuArch = "modern"
if ($old -or $legacy) {
    $cpuArch = "old"
    Write-Host "Building for older CPUs (SSE4.2 compatible)..." -ForegroundColor Yellow
} else {
    Write-Host "Building for modern CPUs (AVX2 compatible)..." -ForegroundColor Green
}

cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCPU_ARCH=$cpuArch .. 
cmake --build . --config Release -- -j16


# ✅ Optionally, copy compiled .qm files into build or embed them via qrc

# ✅ Deploy Qt runtime
& "C:\Qt\6.9.2\mingw_64\bin\windeployqt.exe" "NoteApp.exe"

# Copy DLLs only if they don't already exist in build folder
Get-ChildItem -Path "..\dllpack\*.dll" | ForEach-Object {
    $destination = Join-Path "..\build" $_.Name
    if (-not (Test-Path $destination)) {
        Copy-Item -Path $_.FullName -Destination $destination
    }
}
# Copy-Item -Path "..\dllpack\bsdtar.exe" -Destination "..\build" -Force

# Copy share folder
Copy-Item -Path "..\share" -Destination "..\build\share" -Recurse -Force
./NoteApp.exe
cd ../