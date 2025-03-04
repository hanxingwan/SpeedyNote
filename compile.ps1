rm -r build
mkdir build
cd .\build
cmake -G "MinGW Makefiles" .. 
cmake --build .  
& "C:\Qt\5.15.2\mingw81_64\bin\windeployqt.exe" "NoteApp.exe"
Copy-Item -Path "C:\Games\notecpp_2rowlowres_legacy\dllpack\*.dll" -Destination "C:\Games\notecpp_2rowlowres_legacy\build" -Force
./NoteApp.exe
cd ../