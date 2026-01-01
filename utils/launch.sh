#!/bin/bash
# create_disk_efidsk.sh - Build and boot UEFI application using efidsk
set -e
SCRIPT_DIR="$(dirname "$(realpath "$0")")"
BINARY="$SCRIPT_DIR/../build/UEFIApp.efi"
EFI_ROOT="efi_root"
IMAGE="uefi_disk.img"
IMAGE_SIZE_MB=48
OVMF_VARS_COPY="OVMF_VARS.fd"

# Check if binary exists
if [ ! -f "$BINARY" ]; then
    echo "Error: $BINARY not found!"
    echo "Build your project first with: cd build && cmake .. && make"
    exit 1
fi

# Check if efidsk exists, if not try to compile it
if [ ! -f "$SCRIPT_DIR/efidsk" ]; then
    echo "efidsk utility not found. Attempting to compile..."
    
    if [ ! -f "$SCRIPT_DIR/efidsk.c" ]; then
        echo "Error: efidsk.c not found in current directory!"
        echo "Make sure you're in the POSIX-UEFI directory or copy the utils here"
        exit 1
    fi
    
    echo "Compiling efidsk..."
    gcc -o efidsk efidsk.c
    
    if [ ! -f "$SCRIPT_DIR/efidsk" ]; then
        echo "Error: Failed to compile efidsk"
        exit 1
    fi
    
    echo "✓ efidsk compiled successfully"
fi

# Cleanup old files
echo "Cleaning up old files..."
rm -rf "$EFI_ROOT"
rm -f "$IMAGE"

# Create directory structure
echo "Creating EFI directory structure..."
mkdir -p "$EFI_ROOT/EFI/BOOT"

# Copy binary
echo "Copying $BINARY to BOOTX64.EFI..."
cp "$BINARY" "$EFI_ROOT/EFI/BOOT/BOOTX64.EFI"

# Create disk image with efidsk
echo "Creating ${IMAGE_SIZE_MB}MB disk image with efidsk..."
$SCRIPT_DIR/efidsk -s "$IMAGE_SIZE_MB" "$EFI_ROOT" "$IMAGE"

echo ""
echo "✓ Created $IMAGE successfully!"
echo ""

# Detect OVMF firmware location (distro-agnostic)
OVMF_CODE=""
OVMF_VARS=""

# Try different possible locations
if [ -f "/usr/share/ovmf/x64/OVMF_CODE.4m.fd" ]; then
    # Ubuntu/Debian with 4MB firmware
    OVMF_CODE="/usr/share/ovmf/x64/OVMF_CODE.4m.fd"
    OVMF_VARS="/usr/share/ovmf/x64/OVMF_VARS.4m.fd"
    echo "Detected: Ubuntu/Debian OVMF (4MB)"
elif [ -f "/usr/share/ovmf/OVMF.fd" ]; then
    # Arch Linux unified firmware
    OVMF_CODE="/usr/share/ovmf/OVMF.fd"
    OVMF_VARS=""
    echo "Detected: Arch Linux OVMF (unified)"
elif [ -f "/usr/share/edk2-ovmf/x64/OVMF_CODE.fd" ]; then
    # Fedora/RHEL location
    OVMF_CODE="/usr/share/edk2-ovmf/x64/OVMF_CODE.fd"
    OVMF_VARS="/usr/share/edk2-ovmf/x64/OVMF_VARS.fd"
    echo "Detected: Fedora/RHEL OVMF"
elif [ -f "/usr/share/OVMF/OVMF_CODE.fd" ]; then
    # Alternative common location
    OVMF_CODE="/usr/share/OVMF/OVMF_CODE.fd"
    OVMF_VARS="/usr/share/OVMF/OVMF_VARS.fd"
    echo "Detected: Generic OVMF location"
else
    echo "Error: OVMF firmware not found!"
    echo ""
    echo "Please install OVMF for your distribution:"
    echo "  Ubuntu/Debian: sudo apt install ovmf"
    echo "  Arch Linux:    sudo pacman -S edk2-ovmf"
    echo "  Fedora/RHEL:   sudo dnf install edk2-ovmf"
    exit 1
fi

echo "Using OVMF firmware: $OVMF_CODE"
echo ""
echo "Launching QEMU..."
echo "Press Ctrl+C to exit"
echo ""

# Prepare QEMU command
QEMU_CMD="qemu-system-x86_64"

if [ -n "$OVMF_VARS" ] && [ -f "$OVMF_VARS" ]; then
    # Split CODE/VARS firmware (Ubuntu/Debian, Fedora)
    echo "Copying OVMF VARS to local directory (writable copy)..."
    cp "$OVMF_VARS" "$OVMF_VARS_COPY"
    chmod 644 "$OVMF_VARS_COPY"
    
    $QEMU_CMD \
        -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
        -drive if=pflash,format=raw,file="$OVMF_VARS_COPY" \
        -drive file="$IMAGE",format=raw \
        -m 256M \
        -usb \
        -device usb-mouse \
        -net none \
        -serial stdio \
        -vga std
else
    # Unified firmware (Arch Linux)
    $QEMU_CMD \
        -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
        -drive file="$IMAGE",format=raw \
        -m 256M \
        -usb \
        -device usb-mouse \
        -net none \
        -serial stdio \
        -vga std
fi
