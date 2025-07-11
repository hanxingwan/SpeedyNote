# SpeedyNote PDF File Association

SpeedyNote now supports opening PDF files directly from Windows File Explorer! This feature allows you to right-click on any PDF file and open it with SpeedyNote for annotation.

## How to Enable PDF Association

### During Installation
When installing SpeedyNote using the installer, you'll see an option:
- ✅ **"Associate PDF files with SpeedyNote (adds SpeedyNote to 'Open with' menu)"**

Check this option to enable PDF file association.

### Manual Installation
If you're using the portable version or want to enable association later:

1. Run SpeedyNote installer
2. During installation, check the "Associate PDF files with SpeedyNote" option
3. Complete the installation

## How to Use PDF Association

### Method 1: Right-click Context Menu
1. Navigate to any PDF file in Windows File Explorer
2. Right-click on the PDF file
3. Select **"Open with"** → **"SpeedyNote"**

### Method 2: Default Application (Optional)
1. Right-click on a PDF file
2. Select **"Open with"** → **"Choose another app"**
3. Select **"SpeedyNote"** from the list
4. Check **"Always use this app to open .pdf files"** (optional)
5. Click **"OK"**

## What Happens When You Open a PDF

When you double-click a PDF file (or open it via context menu), SpeedyNote will show a dialog with three options:

### 1. Create New Notebook Folder
- **What it does**: Creates a new folder with the same name as the PDF file
- **Location**: Same directory as the PDF file
- **Result**: Sets up a new notebook linked to this PDF
- **Use case**: First time opening this PDF for annotation

### 2. Use Existing Notebook Folder
- **What it does**: Lets you select an existing folder to use as the notebook
- **Validation**: Checks if the folder is already linked to the same PDF
- **Conflict handling**: Asks if you want to replace the link if it's linked to a different PDF
- **Use case**: Opening a PDF that you've already been working on

### 3. Cancel
- **What it does**: Closes the dialog without opening the PDF
- **Use case**: Changed your mind or opened the wrong file

## Example Workflow

1. **First Time**: You have a PDF called "Lecture_Notes.pdf"
   - Double-click the PDF
   - Choose "Create New Notebook Folder"
   - SpeedyNote creates a folder called "Lecture_Notes" 
   - The PDF opens in SpeedyNote ready for annotation

2. **Later Sessions**: Opening the same PDF
   - Double-click "Lecture_Notes.pdf" again
   - Choose "Use Existing Notebook Folder"
   - Select the "Lecture_Notes" folder
   - SpeedyNote opens with all your previous annotations

## Technical Details

### File Structure
When you create a notebook from a PDF, SpeedyNote creates:
```
Lecture_Notes/           # Notebook folder
├── .pdf_path.txt       # Links to the original PDF
├── .notebook_id.txt    # Unique notebook identifier
├── notebook_00000.png  # Your annotations for page 1
├── notebook_00001.png  # Your annotations for page 2
└── ...                 # More annotation pages
```

### Supported Features
- ✅ All PDF versions supported by Poppler
- ✅ Multi-page PDFs
- ✅ Password-protected PDFs (will prompt for password)
- ✅ High-resolution rendering (configurable DPI)
- ✅ PDF text selection and copying
- ✅ PDF link navigation
- ✅ PDF outline/bookmarks

### Icon in File Explorer
PDF files associated with SpeedyNote will show the SpeedyNote icon in the "Open with" menu, making it easy to identify.

## Troubleshooting

### "SpeedyNote doesn't appear in Open with menu"
1. Make sure you installed SpeedyNote with the PDF association option checked
2. Try restarting Windows Explorer (Task Manager → Windows Explorer → Restart)
3. Try running the installer again and selecting the PDF association option

### "Error: The PDF file could not be found"
- The PDF file may have been moved or deleted
- Check that the file path is correct and the file exists

### "Different PDF Linked" Warning
- This appears when you try to use a notebook folder that's already linked to a different PDF
- Choose "Yes" to replace the link, or "No" to cancel and select a different folder

## Uninstalling PDF Association

If you want to remove the PDF association:
1. Uninstall SpeedyNote through Windows Settings or Control Panel
2. The installer will automatically clean up the registry entries

## Command Line Usage

You can also open PDFs from the command line:
```bash
NoteApp.exe "C:\path\to\your\document.pdf"
```

This will show the same dialog as double-clicking the file. 