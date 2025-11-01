rm -r build
mkdir build

# ✅ Update translation source files (ensure the .ts files exist already)
& "C:\msys64\clang64\bin\lupdate-qt6.exe" . -ts ./resources/translations/app_fr.ts ./resources/translations/app_zh.ts ./resources/translations/app_es.ts
& "C:\msys64\clang64\bin\linguist-qt6.exe" resources/translations/app_zh.ts
# & "C:\msys64\clang64\bin\linguist-qt6.exe" resources/translations/app_fr.ts
# & "C:\msys64\clang64\bin\linguist-qt6.exe" resources/translations/app_es.ts
