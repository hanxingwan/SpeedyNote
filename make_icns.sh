#!/bin/bash

ICON_NAME="mainicon"
INPUT_PNG="./resources/icons/$ICON_NAME.png"
ICONSET_DIR="$ICON_NAME.iconset"
OUTPUT_ICNS="$ICON_NAME.icns"

if [ ! -f "$INPUT_PNG" ]; then
    echo "❌ PNG file '$INPUT_PNG' not found. Place it in the same folder and try again."
    exit 1
fi

echo "📦 Creating icon set from $INPUT_PNG..."
mkdir -p "$ICONSET_DIR"

# Generate all required icon sizes
sips -z 16 16     "$INPUT_PNG" --out "$ICONSET_DIR/icon_16x16.png"
sips -z 32 32     "$INPUT_PNG" --out "$ICONSET_DIR/icon_16x16@2x.png"
sips -z 32 32     "$INPUT_PNG" --out "$ICONSET_DIR/icon_32x32.png"
sips -z 64 64     "$INPUT_PNG" --out "$ICONSET_DIR/icon_32x32@2x.png"
sips -z 128 128   "$INPUT_PNG" --out "$ICONSET_DIR/icon_128x128.png"
sips -z 256 256   "$INPUT_PNG" --out "$ICONSET_DIR/icon_128x128@2x.png"
sips -z 256 256   "$INPUT_PNG" --out "$ICONSET_DIR/icon_256x256.png"
sips -z 512 512   "$INPUT_PNG" --out "$ICONSET_DIR/icon_256x256@2x.png"
sips -z 512 512   "$INPUT_PNG" --out "$ICONSET_DIR/icon_512x512.png"
cp "$INPUT_PNG" "$ICONSET_DIR/icon_512x512@2x.png"

echo "🧱 Generating $OUTPUT_ICNS..."
iconutil -c icns "$ICONSET_DIR" -o "$OUTPUT_ICNS"

if [ $? -eq 0 ]; then
    echo "✅ $OUTPUT_ICNS created successfully!"
else
    echo "❌ Failed to generate $OUTPUT_ICNS"
fi

# Optionally clean up
rm -r "$ICONSET_DIR"
