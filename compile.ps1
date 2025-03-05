rm -r build
mkdir build
cd .\build
cmake -G "MinGW Makefiles" .. 
cmake --build .  
& "C:\Qt\6.8.2\mingw_64\bin\windeployqt.exe" "NoteApp.exe"
Copy-Item -Path "C:\Games\notecpp_2rowlowres\dllpack\*.dll" -Destination "C:\Games\notecpp_2rowlowres_legacy\build" -Force
./NoteApp.exe
cd ../