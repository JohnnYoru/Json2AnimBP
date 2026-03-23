# Json2AnimBP - Unreal Engine 5.3.2 Plugin

Converts a JSON AnimBP export directly into Animation Blueprint nodes using drag-and-drop or a file browser, with full undo support. The converter also auto-creates a **LinkedInputPose → LocalToComponentSpace** chain wired to the first imported node.

---

## Platform Support

| Platform | Status |
|----------|--------|
| **Linux** | Fully supported - use as-is |
| **Windows** | Must be compiled from source |
| **macOS** | Must be compiled from source |

Pre-built binaries are **not included**. Linux users can compile via the editor's built-in rebuild prompt; Windows users require Visual Studio - see below.

---

## Supported Node Types

| JSON key prefix                   | UE class                                                      |
|-----------------------------------|---------------------------------------------------------------|
| `AnimGraphNode_KawaiiPhysics`     | `/Script/KawaiiPhysicsEd.AnimGraphNode_KawaiiPhysics`         |
| `AnimGraphNode_ModifyBone`        | `/Script/AnimGraph.AnimGraphNode_ModifyBone`                  |
| `AnimGraphNode_Constraint`        | `/Script/AnimGraph.AnimGraphNode_Constraint`                  |
| `AnimGraphNode_LayeredBoneBlend`  | `/Script/AnimGraph.AnimGraphNode_LayeredBoneBlend`            |
| `AnimGraphNode_SpringBone`        | `/Script/AnimGraph.AnimGraphNode_SpringBone`                  |

> **KawaiiPhysics nodes** require the [KawaiiPhysics](https://github.com/pafuhana1213/KawaiiPhysics) plugin installed and enabled in your project. All other types are built into UE5.

---

## Requirements

- Unreal Engine **5.3.2** or later
- **Linux:** No extra tools needed beyond UE itself
- **Windows:** Visual Studio 2022 with the **Game Development with C++** workload

---

## Installation

Copy the `Json2AnimBP` folder into your project's `Plugins/` directory:

```
YourProject/
└── Plugins/
    └── Json2AnimBP/
```

## Building from Source (Windows)

### Prerequisites

- Visual Studio 2022 with the following workloads:
  - **Desktop Development with C++**
  - **Game Development with C++**
- Unreal Engine 5.3+ installed

### Steps

1. Copy `Json2AnimBPPlugin/` into your project's `Plugins/` directory.
2. Right-click your `.uproject` file → **"Generate Visual Studio project files"**.
3. Open the generated `.sln` in Visual Studio.
4. Set the configuration to **Development Editor** and build (`Ctrl+Shift+B`).
5. Launch UE5 - the plugin loads automatically.

### Command-line build (Remember to change the paths depending on your UE installation directory)

```bat
:: Windows
"C:\Engine\Build\BatchFiles\Build.bat" ^
    YourProjectEditor Win64 Development ^
    "C:\Path\To\YourProject.uproject" ^
    -WaitMutex -FromMsBuild
```

```bash
# Linux
cd Engine/Build/BatchFiles/Linux
./RunUAT.sh BuildPlugin -plugin="YourProject/Marvel/Plugins/Json2AnimBPPlugin/Json2AnimBPPlugin.uplugin" -Package=../TempPlugin -TargetPlatforms=Linux
```

---

## Usage

1. Open any **Animation Blueprint** in the UE5 editor.
2. In the menu bar go to **Window → Json2AnimBP**.  
   A dockable panel appears - drag it to any side of the editor.
3. The panel header shows which AnimBP is currently targeted.
4. **Drop** a `.json` file onto the drop zone, click **Browse for JSON…**, or paste a file path and press **Enter**.
5. Enable **"Connect nodes in chain"** to wire consecutive nodes together via input/output pins.
6. Nodes are injected into the AnimGraph immediately - press **Ctrl+Z** to undo.

---

## How it Works

```
JSON file
   └─► FJson2AnimBPConverter::Convert()
           └─► "Begin Object … End Object" text (UE clipboard format)
                   └─► FEdGraphUtilities::ImportNodesFromText(AnimGraph, text)
                               └─► Nodes appear in the AnimGraph
```

The converter auto-detects the AnimBP class name from the JSON when none is provided explicitly.

After import, the plugin automatically creates a **LinkedInputPose → LocalToComponentSpace** pair and wires it to the first imported node, so the chain is ready to use out of the box.

---

## Notes / Known Issues

- The panel targets the **most recently opened** AnimBP when multiple are open. If you have several AnimBPs open, close the others or re-open the target one before importing.
- `AnimGraphNode_KawaiiPhysics` nodes silently fail to import if KawaiiPhysics is not installed - UE skips unknown classes without an error.
- Node positions are laid out in a 10-row grid. Rearrange them freely after import.

---

## Build Dependencies

`Slate`, `SlateCore`, `UnrealEd`, `Json`, `Kismet`, `AnimGraph`, `BlueprintGraph`, `InputCore`, `ApplicationCore`, `GraphEditor`, `WorkspaceMenuStructure`

---

## License

See [LICENSE](LICENSE).
