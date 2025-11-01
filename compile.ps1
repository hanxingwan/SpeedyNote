param(
    [switch]$arm64,      # Build for ARM64 Windows (Snapdragon)
    [switch]$old,        # Build for older x86_64 CPUs (SSE3/SSSE3)
    [switch]$legacy      # Alias for -old
)

# ✅ Determine architecture and set appropriate toolchain
if ($arm64) {
    $toolchain = "clangarm64"
    $archName = "ARM64"
    $archColor = "Magenta"
    Write-Host "🚀 Building for ARM64 Windows (Snapdragon)" -ForegroundColor $archColor
} else {
    $toolchain = "clang64"
    $archName = "x86_64"
    $archColor = "Cyan"
    Write-Host "🚀 Building for x86_64 Windows" -ForegroundColor $archColor
}

$toolchainPath = "C:\msys64\$toolchain"

rm -r build
mkdir build

# ✅ Compile .ts → .qm files
& "$toolchainPath\bin\lrelease-qt6.exe" ./resources/translations/app_zh.ts ./resources/translations/app_fr.ts ./resources/translations/app_es.ts

Copy-Item -Path ".\resources\translations\*.qm" -Destination ".\build" -Force

cd .\build

# ✅ Set PATH to use the selected toolchain (critical for DLLs and compiler detection)
$env:PATH = "$toolchainPath\bin;$env:PATH"

# ✅ Prepare CMake configuration
$cmakeArgs = @(
    "-G", "MinGW Makefiles",
    "-DCMAKE_C_COMPILER=$toolchainPath/bin/clang.exe",
    "-DCMAKE_CXX_COMPILER=$toolchainPath/bin/clang++.exe",
    "-DCMAKE_MAKE_PROGRAM=$toolchainPath/bin/mingw32-make.exe",
    "-DCMAKE_BUILD_TYPE=Release"
)

# Architecture-specific configuration
if ($arm64) {
    # ARM64 build - set processor type
    $cmakeArgs += "-DCMAKE_SYSTEM_PROCESSOR=arm64"
    Write-Host "Target: ARM64 (Cortex-A75/Snapdragon optimized)" -ForegroundColor $archColor
} else {
    # x86_64 build - determine CPU architecture target
    $cpuArch = "modern"
    if ($old -or $legacy) {
        $cpuArch = "old"
        Write-Host "Target: Older x86_64 CPUs (SSE3/SSSE3 compatible - Core 2 Duo era)" -ForegroundColor Yellow
    } else {
        Write-Host "Target: Modern x86_64 CPUs (SSE4.2 compatible - Core i series)" -ForegroundColor Green
    }
    $cmakeArgs += "-DCPU_ARCH=$cpuArch"
}

# ✅ Configure and build
& "$toolchainPath\bin\cmake.exe" @cmakeArgs ..
& "$toolchainPath\bin\cmake.exe" --build . --config Release -- -j16

# ✅ Deploy Qt runtime
& "$toolchainPath\bin\windeployqt6.exe" "NoteApp.exe"

# ✅ Copy required DLLs (for Poppler and SDL2 dependencies)
Write-Host "Copying required DLLs from $toolchainPath\bin..." -ForegroundColor Cyan

$requiredDlls = @(
    "SDL2.dll",
    "libpoppler-qt6-3.dll",
    "libbrotlicommon.dll", "libbrotlidec.dll", "libbrotlienc.dll",
    "libbz2-1.dll", "libcrypto-3.dll", "libssl-3.dll", "libc++.dll", "libunwind.dll"
    "libcurl-4.dll", "libexpat-1.dll", "libfreetype-6.dll",
    "libiconv-2.dll", "libidn2-0.dll", "libintl-8.dll",
    "libjpeg-8.dll", "liblcms2-2.dll", "liblcms2_fast_float-2.dll",
    "liblzma-5.dll", "libnghttp2-14.dll", "libnghttp3-9.dll",
    "libngtcp2-16.dll", "libngtcp2_crypto_ossl-0.dll",
    "libopenjp2-7.dll", "libpng16-16.dll", "libpsl-5.dll",
    "libssh2-1.dll", "libtiff-6.dll", "libunistring-5.dll",
    "libwinpthread-1.dll", "libzstd.dll", "zlib1.dll",
    "libfontconfig-1.dll", "libharfbuzz-0.dll", "libglib-2.0-0.dll",
    "libgobject-2.0-0.dll", "libpcre2-8-0.dll", "libgraphite2.dll",
    "libjbig-0.dll", "libLerc.dll", "libdeflate.dll", "libsharpyuv-0.dll",
    "libwebp-7.dll", "libcairo-2.dll", "libpixman-1-0.dll",
    "libffi-8.dll", "libgmodule-2.0-0.dll", "libgio-2.0-0.dll",
    "libdbus-1-3.dll", "liblz4.dll", "libgnutls-30.dll",
    "libnettle-8.dll", "libhogweed-6.dll", "libgmp-10.dll",
    "libtasn1-6.dll", "libp11-kit-0.dll", "libuv-1.dll",
    "libcares-2.dll", "libnspr4.dll", "nss3.dll", "nssutil3.dll",
    "smime3.dll", "libplc4.dll", "libplds4.dll"
)

$sourceDir = "$toolchainPath\bin"
$copiedCount = 0

# ✅ Copy libpoppler-*.dll dynamically (handles any version like 152, 153, etc.)
$popplerDlls = Get-ChildItem -Path "$sourceDir\libpoppler-*.dll" -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^libpoppler-\d{3}\.dll$' }
foreach ($popplerDll in $popplerDlls) {
    $destPath = $popplerDll.Name
    if (-not (Test-Path $destPath)) {
        Copy-Item -Path $popplerDll.FullName -Destination $destPath -Force
        $copiedCount++
        Write-Host "  Found and copied: $($popplerDll.Name)" -ForegroundColor Gray
    }
}

# ✅ Copy other required DLLs
foreach ($dll in $requiredDlls) {
    $sourcePath = Join-Path $sourceDir $dll
    $destPath = $dll  # We're already in the build directory
    
    if (Test-Path $sourcePath) {
        if (-not (Test-Path $destPath)) {
            Copy-Item -Path $sourcePath -Destination $destPath -Force
            $copiedCount++
        }
    }
}

Write-Host "Copied $copiedCount DLL(s) from $toolchain\bin" -ForegroundColor Green

# Copy share folder
Copy-Item -Path "..\share" -Destination "..\build\share" -Recurse -Force
./NoteApp.exe
cd ../