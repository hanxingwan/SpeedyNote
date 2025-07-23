rm -r build
mkdir build


# ✅ Compile .ts → .qm files
& "C:\Qt\6.8.2\mingw_64\bin\lrelease.exe" ./resources/translations/app_zh.ts ./resources/translations/app_fr.ts ./resources/translations/app_es.ts


Copy-Item -Path ".\resources\translations\*.qm" -Destination ".\build" -Force

cd .\build
cmake -G "MinGW Makefiles" .. 
cmake --build . -- -j16


# ✅ Optionally, copy compiled .qm files into build or embed them via qrc

# ✅ Deploy Qt runtime
& "C:\Qt\6.8.2\mingw_64\bin\windeployqt.exe" "NoteApp.exe"

Copy-Item -Path "..\dllpack\*.dll" -Destination "..\build" -Force
Copy-Item -Path "..\dllpack\bsdtar.exe" -Destination "..\build" -Force
./NoteApp.exe
cd ../