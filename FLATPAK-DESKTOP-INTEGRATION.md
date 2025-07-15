# Flatpak Desktop Integration Fix for GNOME

## Problem
Flatpak applications sometimes don't appear in GNOME's application menu (Activities overview) even after successful installation. This is particularly common with GNOME 47+ versions.

## Root Cause
Flatpak creates symbolic links for desktop files and icons in:
- `~/.local/share/flatpak/exports/share/applications/`
- `~/.local/share/flatpak/exports/share/icons/hicolor/256x256/apps/`

However, GNOME doesn't always reliably follow these symlinks or refresh its application cache to detect new Flatpak applications.

## Solution
The fix involves copying the actual files from the Flatpak exports directory to the standard XDG directories where GNOME looks for applications:

1. **Copy desktop file** from:
   ```
   ~/.local/share/flatpak/exports/share/applications/com.github.alpha_liu_01.SpeedyNote.desktop
   ```
   to:
   ```
   ~/.local/share/applications/com.github.alpha_liu_01.SpeedyNote.desktop
   ```

2. **Copy icon file** from:
   ```
   ~/.local/share/flatpak/exports/share/icons/hicolor/256x256/apps/com.github.alpha_liu_01.SpeedyNote.png
   ```
   to:
   ```
   ~/.local/share/icons/hicolor/256x256/apps/com.github.alpha_liu_01.SpeedyNote.png
   ```

3. **Update desktop database**:
   ```bash
   update-desktop-database ~/.local/share/applications/
   ```

## Implementation
This fix has been integrated into both build scripts:

### build-flatpak.sh
- Added `fix_desktop_integration()` function
- Called automatically after successful installation in `install_and_test()`
- Called automatically after successful rebuild in `rebuild_speedynote_only()`

### build-speedynote-only.sh
- Added `fix_desktop_integration()` function
- Called automatically after successful installation

## Manual Fix
If you need to apply this fix manually for an existing installation:

```bash
# Create directories
mkdir -p ~/.local/share/applications
mkdir -p ~/.local/share/icons/hicolor/256x256/apps

# Copy desktop file
cp ~/.local/share/flatpak/exports/share/applications/com.github.alpha_liu_01.SpeedyNote.desktop ~/.local/share/applications/

# Copy icon file
cp ~/.local/share/flatpak/exports/share/icons/hicolor/256x256/apps/com.github.alpha_liu_01.SpeedyNote.png ~/.local/share/icons/hicolor/256x256/apps/

# Update desktop database
update-desktop-database ~/.local/share/applications/
```

## Verification
After applying the fix, SpeedyNote should appear in GNOME's Activities overview when you search for "SpeedyNote" or browse the applications grid.

## Notes
- This fix is specific to GNOME desktop environment
- The fix doesn't interfere with Flatpak's normal operation
- The copied files will be automatically updated when the Flatpak application is updated
- This workaround may not be needed in future GNOME versions if the underlying issue is resolved

## Alternative Solutions
If the above doesn't work, you can try:
1. Restart GNOME Shell: `Alt+F2`, type `r`, press Enter
2. Restart the user session
3. Clear GNOME's application cache: `rm -rf ~/.cache/gnome-shell/` 