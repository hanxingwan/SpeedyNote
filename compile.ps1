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
    "libdeflate.dll", "libiconv-2.dll", "libnettle-8.dll", "libreadline8.dll", "Qt6Core.dll", "libdouble-conversion.dll", "libicudt77.dll", "libnghttp2-14.dll", "librhash.dll", "Qt6Gui.dll", "libexpat-1.dll", "libicuin77.dll", "libnghttp3-9.dll", "libsharpyuv-0.dll", "Qt6Network.dll", "libffi-8.dll", "libicuio77.dll", "libngtcp2-16.dll", "libssh2-1.dll", "Qt6Widgets.dll", "libfontconfig-1.dll", "libicutest77.dll", "libngtcp2_crypto_gnutls-8.dll", "libssl-3.dll", "libssl-3-x64.dll", "SDL2.dll", "libformw6.dll", "libicutu77.dll", "libngtcp2_crypto_ossl-0.dll", "libsystre-0.dll", "libLTO.dll", "libfreetype-6.dll", "libicuuc77.dll", "libngtcp2_crypto_ossl.dll", "libtasn1-6.dll", "libLerc.dll", "libgif-7.dll", "libidn2-0.dll", "libnspr4.dll", "libtermcap-0.dll", "libRemarks.dll", "libgio-2.0-0.dll", "libintl-8.dll", "libomp.dll", "libtiff-6.dll", "libarchive-13.dll", "libgirepository-2.0-0.dll", "libjbig-0.dll", "libopenjp2-7.dll", "libtiffxx-6.dll", "libasprintf-0.dll", "libglib-2.0-0.dll", "libjpeg-8.dll", "libopenjpip-7.dll", "libtre-5.dll", "libb2-1.dll", "libgmodule-2.0-0.dll", "libjsoncpp-26.dll", "libp11-kit-0.dll", "libturbojpeg.dll", "libbrotlicommon.dll", "libgmp-10.dll", "liblcms2-2.dll", "libpanelw6.dll", "libunistring-5.dll", "libbrotlidec.dll", "libgmpxx-4.dll", "liblcms2_fast_float-2.dll", "libpcre2-16-0.dll", "libunwind.dll", "libbrotlienc.dll", "libgnutls-30.dll", "liblldb.dll", "libpcre2-32-0.dll", "libuv-1.dll", "libbz2-1.dll", "libgnutls-openssl-27.dll", "liblz4.dll", "libpcre2-8-0.dll", "libwebp-7.dll", "libc++.dll", "libgnutlsxx-30.dll", "liblzma-5.dll", "libpcre2-posix-3.dll", "libwebpdecoder-3.dll", "libcairo-2.dll", "libgobject-2.0-0.dll", "liblzo2-2.dll", "libpixman-1-0.dll", "libwebpdemux-2.dll", "libcairo-gobject-2.dll", "libgraphite2.dll", "libmd4c-html.dll", "libpkgconf-7.dll", "libwebpmux-3.dll", "libcairo-script-interpreter-2.dll", "libgthread-2.0-0.dll", "libmd4c.dll", "libplc4.dll", "libwinpthread-1.dll", "libcares-2.dll", "libharfbuzz-0.dll", "libmenuw6.dll", "libplds4.dll", "libzstd.dll", "libcharset-1.dll", "libharfbuzz-gobject-0.dll", "libmpdec++-4.dll", "libpng16-16.dll", "nss3.dll", "libcrypto-3.dll", "libcrypto-3-x64.dll", "libharfbuzz-subset-0.dll", "libmpdec-4.dll", "nssutil3.dll", "libcurl-4.dll", "libhistory8.dll", "libncurses++w6.dll", "libpoppler-qt6-3.dll", "smime3.dll", "libdbus-1-3.dll", "libhogweed-6.dll", "libncursesw6.dll", "libpsl-5.dll", "zlib1.dll"
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
Copy-Item -Path "$toolchainPath\share\poppler" -Destination "..\build\share\poppler" -Recurse -Force
./NoteApp.exe
cd ../