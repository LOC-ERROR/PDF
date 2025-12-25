# Image Folder to PDF (C++ / Qt)

A Windows-friendly GUI app that converts all images in a selected folder into a single PDF.

## Features
- Enter a folder path manually or pick one with a folder browser.
- Converts supported images (`jpg`, `jpeg`, `png`, `tiff`, `bmp`, `webp`) to a single PDF.
- Images are embedded at 100% resolution with a default output of **150 DPI**.
- Outputs `images.pdf` in the selected folder, sorted alphabetically by filename.

## Build on Windows
This project uses Qt (Widgets) and CMake.

```bash
cmake -S . -B build -G "Ninja" -DCMAKE_PREFIX_PATH="C:\Qt\6.6.2\msvc2019_64"
cmake --build build --config Release
```

The executable will be in `build/` (or `build/Release` depending on the generator).

## Notes
- Use Qt 6.x with the Widgets module enabled.
- The app writes `images.pdf` into the selected folder.
