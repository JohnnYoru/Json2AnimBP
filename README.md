# Json2AnimBP

Lightweight GUI utility that converts structured JSON data into Animation Blueprint nodes for Unreal Engine.

Designed for modders who need a fast way to transform external data into usable AnimBP node structures without manual setup.

---

## Overview

Json2AnimBP parses formatted JSON input and generates node data ready to be implemented inside Unreal Engine Animation Blueprints.

---

## Features

- Simple graphical interface
- JSON to AnimBP node conversion
- Fast processing
- No setup required
- Lightweight

---

## Usage

Download the executable from the Releases section and run it.

1. Open **Json2AnimBP**.

2. Load your JSON file:
   - Click **Browse...** and select your file
   - Or drag & drop the JSON file directly into the window

3. Confirm the **AnimBP Class**:
   - In almost all cases, use **Auto-Detect**
   - If needed, manually enter the Animation Blueprint class name

4. Choose your output options:
   - **Copy output to clipboard** – Copies the generated node data automatically
   - **Save to .txt file** – Saves the generated output as a text file
   - **Connect nodes in chain** – Automatically links generated nodes together to avoid manual connection inside Unreal Engine

5. Click **Convert**.

6. Paste the generated output into your Animation Blueprint inside Unreal Engine.

The nodes will be ready to use, optionally pre-connected if the chain option was enabled.

## Build Notes

If compiling the project manually with PyInstaller, it is recommended to keep the main script and the assets directory at the same directory level during the build process.

This ensures that the application icon and bundled resources resolve correctly when generating the executable.

---

## License

This project is released under the MIT License.

---

## Credits

Application icon provided by [Icons8](https://icons8.com) under their Universal Multimedia License.  
Icon assets are not covered by the MIT License.
