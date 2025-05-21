
#!/bin/bash
set -e

# 1. Clean and recreate build directory
rm -rf build
mkdir build

# 2. Compile .ts files → .qm using lrelease (from Qt)
# lrelease ./resources/translations/app_zh.ts ./resources/translations/app_fr.ts ./resources/translations/app_es.ts

# 3. Copy .qm files to build dir (optional, unless embedded in .qrc)
cp resources/translations/*.qm build/

# 4. CMake configure and build
cd build
cmake ..
make -j$(nproc)

# 5. Run the app (NoteApp will be the binary, no .exe)
./NoteApp

