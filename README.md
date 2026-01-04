# UEFI Pixel-Game Project

A retro-style game project running natively on UEFI x86_64 hardware. This project implements a custom "Big-Pixel" engine to scale 8x8 assets into a high-resolution graphical environment without the overhead of a traditional Operating System.

## 📂 Environment & Tools

* **Custom OVMF:** A pre-compiled `OVMF.fd` is included in the root directory. It is specially configured for X64 with full **GOP (Graphics Output Protocol)** and **Simple Pointer Protocol** support for QEMU.
* **Build System:** Uses **CMake** for cross-compilation targeting the EFI PE executable format.
* **Deployment Script:** `create_disk.sh` automates the following:
    1. Generates a virtual disk image.
    2. Formats it to FAT32 (UEFI Requirement).
    3. Deploys the binary to `\EFI\BOOT\BOOTX64.EFI`.
    4. Launches QEMU with the custom OVMF firmware.

## 🛠 Build & Test Instructions

### 1. Compile the EFI Binary
```bash
mkdir build
cd build
cmake ..
make

2. Run in QEMU
Bash

# From the project root
chmod +x create_disk.sh
./create_disk.sh
```

📝 TODO List
1. Architecture & OOP Refactor

    [ ] Fix Include Hell: Refactor headers to remove circular dependencies between main.h, helpers.h, and images.h. Implement a strictly hierarchical include strategy.

    [ ] Object-Oriented Abstraction: Transition from procedural code to a "struct-as-class" pattern using function pointers:

        Sprite: Encapsulate ImageData, position, and Draw() logic.

        Button: Extend Sprite with OnClick callbacks and hover states.

        InputManager: Singleton to abstract Mouse and Keyboard events.

    [ ] Memory Management: Audit Realloc usage and implement a safe Free mechanism to prevent leaks during state transitions.

2. UI & Menu System

    [ ] State Machine: Implement a GameState handler to manage transitions between BOOT, MAIN_MENU, SETTINGS, and GAMEPLAY.

    [ ] Main Menu: Build a clickable 8-bit interface using the Button class.

    [ ] Settings Menu:

        Dynamic Resolution Picker (querying and switching GOP modes).

        Mouse Sensitivity slider.

        Toggle for movement interpolation (Smooth vs. Grid-Locked).

3. Game Engine Implementation

    [ ] Collision Engine: Implement AABB (Axis-Aligned Bounding Box) logic for sprite-to-sprite and cursor-to-button interactions.

    [ ] Delta Time Integration: Use the UEFI Timer event to calculate time-deltas, ensuring game physics remain consistent across different CPU speeds.

    [ ] Refine Blt Logic: Optimize the "Dirty Rectangle" system to ensure background restoration only occurs for modified screen regions.

4. Stability & Build Tooling

    [ ] GOP Resize Handling: Add a callback to refresh GOP pointers and screen dimensions when QEMU/Hardware changes resolutions to prevent #UD (Invalid Opcode) exceptions.

    [ ] Asset Pipeline: Automate the conversion of .png or .bmp files into C-style ImageData arrays during the CMake build process.

    [ ] QEMU Debugging: Add GDB support to create_disk.sh using QEMU -s -S flags.
