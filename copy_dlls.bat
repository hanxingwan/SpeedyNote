@echo off
setlocal enabledelayedexpansion

set "EXE_PATH=C:\Games\notecpp\build\NoteApp.exe"
set "DEST_DIR=C:\Games\notecpp\build\"
set "MINGW_BIN=C:\msys64\mingw64\bin"

echo Finding missing DLLs using `ldd`...
for /f "tokens=3" %%i in ('C:\msys64\usr\bin\ldd "%EXE_PATH%" ^| findstr "=>"') do (
    set "DLL_NAME=%%~nxi"
    
    rem Skip Windows system DLLs
    echo !DLL_NAME! | findstr /i "ntdll.dll kernel32.dll msvcrt.dll user32.dll gdi32.dll combase.dll ole32.dll shell32.dll" >nul && (
        echo Skipping system DLL: !DLL_NAME!
        goto :continue
    )

    rem Try to copy from MINGW bin folder
    if exist "%MINGW_BIN%\!DLL_NAME!" (
        echo Copying: %MINGW_BIN%\!DLL_NAME!
        copy /Y "%MINGW_BIN%\!DLL_NAME!" "%DEST_DIR%"
    ) else (
        echo Missing DLL: !DLL_NAME!
    )

    :continue
)
echo Done!
pause
