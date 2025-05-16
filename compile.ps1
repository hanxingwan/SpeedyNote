rm -r build
mkdir build


# ✅ Compile .ts → .qm files
& "C:\Qt\6.8.2\mingw_64\bin\lrelease.exe" ./resources/translations/app_zh.ts ./resources/translations/app_fr.ts ./resources/translations/app_es.ts


Copy-Item -Path "C:\Games\notecpp_2rowlowres_legacy\resources\translations\*.qm" -Destination "C:\Games\notecpp_2rowlowres_legacy\build" -Force

cd .\build
cmake -G "MinGW Makefiles" .. 
cmake --build . -- -j8


# ✅ Optionally, copy compiled .qm files into build or embed them via qrc

# ✅ Deploy Qt runtime
& "C:\Qt\6.8.2\mingw_64\bin\windeployqt.exe" "NoteApp.exe"

Copy-Item -Path "C:\Games\notecpp_2rowlowres\dllpack\*.dll" -Destination "C:\Games\notecpp_2rowlowres_legacy\build" -Force
Copy-Item -Path "C:\Games\notecpp_2rowlowres_legacy\dllpack\bsdtar.exe" -Destination "C:\Games\notecpp_2rowlowres_legacy\build" -Force
./NoteApp.exe
cd ../