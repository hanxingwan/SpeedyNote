# Flatpak Distribution Guide for SpeedyNote

## Distribution Methods

There are two main ways to distribute SpeedyNote as a Flatpak application:

### 1. GitHub Releases (Recommended for initial distribution)

**File to upload:** `SpeedyNote-0.6.1.flatpak` (3.4MB)

**How to create:**
```bash
./build-flatpak.sh
# OR manually:
flatpak build-bundle repo SpeedyNote-0.6.1.flatpak com.github.alpha_liu_01.SpeedyNote
```

**User installation:**
```bash
# Download from GitHub releases, then:
flatpak install SpeedyNote-0.6.1.flatpak
```

**Advantages:**
- Easy to distribute
- No approval process required
- Full control over releases
- Users can install directly from downloaded file

**Disadvantages:**
- Users need to manually download and install
- No automatic updates
- Limited discoverability

### 2. Flathub (Recommended for wider distribution)

**Files to submit:** Manifest files (not the built package)

**Required files for Flathub submission:**
- `com.github.alpha_liu_01.SpeedyNote.yml` (manifest)
- `com.github.alpha_liu_01.SpeedyNote.desktop` (desktop file)
- `com.github.alpha_liu_01.SpeedyNote.metainfo.xml` (metadata)

**How to submit to Flathub:**
1. Fork the [flathub/flathub](https://github.com/flathub/flathub) repository
2. Create a new repository for your app: `com.github.alpha_liu_01.SpeedyNote`
3. Add your manifest files to the new repository
4. Submit a pull request to the main flathub repository

**User installation (after approval):**
```bash
flatpak install flathub com.github.alpha_liu_01.SpeedyNote
```

**Advantages:**
- Centralized distribution
- Automatic updates
- Better discoverability
- Integrated with Linux software centers

**Disadvantages:**
- Requires approval process
- Less control over release timing
- Must follow Flathub guidelines

## Current Status

✅ **Bundle ready for GitHub releases:** `SpeedyNote-0.6.1.flatpak` (3.4MB)
✅ **Manifest ready for Flathub:** All required files are prepared

## Recommended Distribution Strategy

### Phase 1: GitHub Releases (Immediate)
1. Upload `SpeedyNote-0.6.1.flatpak` to GitHub releases
2. Add installation instructions to README
3. Get user feedback and fix issues

### Phase 2: Flathub Submission (After testing)
1. Submit manifest files to Flathub
2. Wait for review and approval
3. Promote Flathub installation method

## Installation Instructions for Users

### From GitHub Releases
```bash
# Download SpeedyNote-0.6.1.flatpak from GitHub releases, then:
flatpak install SpeedyNote-0.6.1.flatpak
flatpak run com.github.alpha_liu_01.SpeedyNote
```

### From Flathub (after approval)
```bash
flatpak install flathub com.github.alpha_liu_01.SpeedyNote
flatpak run com.github.alpha_liu_01.SpeedyNote
```

## Bundle Information

**File:** `SpeedyNote-0.6.1.flatpak`
**Size:** 3.4MB
**App ID:** `com.github.alpha_liu_01.SpeedyNote`
**Runtime:** org.kde.Platform 6.8
**Architecture:** x86_64

## Testing the Bundle

To test the bundle on a clean system:
```bash
# Remove existing installation
flatpak uninstall com.github.alpha_liu_01.SpeedyNote

# Install from bundle
flatpak install SpeedyNote-0.6.1.flatpak

# Test run
flatpak run com.github.alpha_liu_01.SpeedyNote
```

## Flathub Submission Checklist

Before submitting to Flathub, ensure:
- [ ] App ID follows reverse DNS format: `com.github.alpha_liu_01.SpeedyNote`
- [ ] Desktop file is valid and tested
- [ ] Icon is properly sized (256x256) and formatted
- [ ] Metainfo file contains required metadata
- [ ] Manifest builds successfully
- [ ] Application runs without errors
- [ ] No hardcoded paths or user-specific configurations
- [ ] Follows Flathub quality guidelines

## Updates and Maintenance

### For GitHub Releases
- Update version in CMakeLists.txt
- Update version in manifest file
- Rebuild bundle with new version number
- Upload new bundle to GitHub releases

### For Flathub
- Update version in manifest file
- Commit changes to app repository
- Flathub bot automatically builds and publishes updates 