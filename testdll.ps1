cd build
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
cd ../