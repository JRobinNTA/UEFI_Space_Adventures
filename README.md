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

# 🗺️ Project Roadmap & TODO List

## 🏗️ 1. Architecture & OOP Refactor
> *Goal: Move from a monolithic procedural structure to a scalable, object-oriented engine.*

- [ ] **Fix "Include Hell"**
  - Resolve circular dependencies between `main.h`, `helpers.h`, and `images.h`.
  - Implement a strict top-down include strategy (e.g., `Types.h` -> `Protocols.h` -> `Core.h`).
- [ ] **Object-Oriented Abstraction**
  - Transition to "struct-as-class" patterns using function pointers:
    - `Sprite`: Encapsulates `ImageData`, position, and `Draw()` logic.
    - `Button`: Extends `Sprite` with `OnClick` callbacks and hover-state detection.
    - `InputManager`: A singleton to abstract Mouse and Keyboard hardware events.
- [ ] **Memory Management Audit**
  - Standardize `Realloc` usage.
  - Implement a safe `Free` mechanism to prevent memory leaks during scene transitions.

---

## 🖥️ 2. UI & Menu System
> *Goal: Create a navigable game interface.*

- [ ] **Global State Machine**
  - Manage transitions between logic states: `BOOT` ➡️ `MAIN_MENU` ➡️ `SETTINGS` ➡️ `GAMEPLAY`.
- [ ] **Interactive Main Menu**
  - Build a clickable 8-bit interface using the newly implemented `Button` class.
- [ ] **Settings Configuration**
  - **Resolution Picker:** Query GOP modes and allow dynamic switching.
  - **Input Sensitivity:** Slider to adjust mouse movement scaling.
  - **Movement Toggle:** Switch between *Smooth Interpolation* and *Grid-Locked* movement.

---

## 🕹️ 3. Game Engine Core
> *Goal: Standardize physics and rendering logic.*

- [ ] **Collision Engine**
  - Implement **AABB (Axis-Aligned Bounding Box)** logic for cursor-to-button and sprite-to-sprite detection.
- [ ] **Delta Time Integration**
  - Use the UEFI High-Resolution Timer to calculate `deltaTime`.
  - Ensure game physics (movement/animations) remain consistent regardless of CPU clock speed.
- [ ] **Blt Logic Optimization**
  - Refine the "Dirty Rectangle" system to minimize framebuffer writes by only restoring regions modified in the previous frame.

---

## 🛠️ 4. Stability & Build Tooling
> *Goal: Improve the developer experience and hardware compatibility.*

- [ ] **GOP Resize Handling**
  - Add callbacks to refresh GOP pointers and screen dimensions on resolution change.
  - *Fix:* Prevents the `#UD (Invalid Opcode)` exception during QEMU window resizing.
- [ ] **Automated Asset Pipeline**
  - Integrate a script into the `CMake` build process to convert `.png` or `.bmp` files directly into C-style `ImageData` arrays.
- [ ] **Advanced Debugging**
  - Update `create_disk.sh` to support GDB attachment via QEMU `-s -S` flags.
