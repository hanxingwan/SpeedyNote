rm -r build
mkdir build


# ✅ Compile .ts → .qm files
& "C:\Qt\5.15.2\mingw81_64\bin\lrelease.exe" ./resources/translations/app_zh.ts ./resources/translations/app_fr.ts ./resources/translations/app_es.ts


Copy-Item -Path "C:\Games\notecpp_2rowlowres_latest_qt5_0.4.10\resources\translations\*.qm" -Destination "C:\Games\notecpp_2rowlowres_latest_qt5_0.4.10\build" -Force

cd .\build
cmake -G "MinGW Makefiles" .. 
cmake --build . -- -j8


# ✅ Optionally, copy compiled .qm files into build or embed them via qrc

# ✅ Deploy Qt runtime
& "C:\Qt\5.15.2\mingw81_64\bin\windeployqt.exe" "NoteApp.exe"

Copy-Item -Path "C:\Games\notecpp_2rowlowres_qt5_complete\dllpack\*.dll" -Destination "C:\Games\notecpp_2rowlowres_latest_qt5_0.4.10\build" -Force
Copy-Item -Path "C:\Games\notecpp_2rowlowres_legacy\dllpack\bsdtar.exe" -Destination "C:\Games\notecpp_2rowlowres_latest_qt5_0.4.10\build" -Force
./NoteApp.exe
cd ../