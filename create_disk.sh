#!/bin/bash
# create_disk.sh

set -e

IMAGE="uefi_disk.img"
BINARY="build/UEFIApp.efi"
IMAGE_SIZE_MB=48  # 48MB image (much larger than 1.44MB floppy)
OVMF_VARS_COPY="OVMF_VARS.fd"  # Local writable copy

# Check if binary exists
if [ ! -f "$BINARY" ]; then
    echo "Error: $BINARY not found!"
    echo "Build your project first with: cd build && cmake .. && make"
    exit 1
fi

# Cleanup old image if it exists
if [ -f "$IMAGE" ]; then
    echo "Removing old image: $IMAGE"
    rm -f "$IMAGE"
fi

echo "Creating ${IMAGE_SIZE_MB}MB disk image..."
dd if=/dev/zero of="$IMAGE" bs=1M count=$IMAGE_SIZE_MB status=progress

echo "Formatting as FAT32..."
mformat -i "$IMAGE" -F ::  # -F for FAT32 (instead of FAT12)

echo "Creating /EFI/BOOT directories..."
mmd -i "$IMAGE" ::/EFI
mmd -i "$IMAGE" ::/EFI/BOOT

echo "Copying $BINARY to /EFI/BOOT/BOOTX64.EFI..."
mcopy -i "$IMAGE" "$BINARY" ::/EFI/BOOT/BOOTX64.EFI

echo "Verifying image contents:"
mdir -i "$IMAGE" ::/EFI/BOOT

echo ""
echo "✓ Created $IMAGE successfully!"
echo ""

# Check if OVMF firmware exists
OVMF_CODE="/usr/share/ovmf/x64/OVMF_CODE.4m.fd"
OVMF_VARS="/usr/share/ovmf/x64/OVMF_VARS.4m.fd"

if [ ! -f "$OVMF_CODE" ]; then
    echo "Warning: OVMF CODE not found at $OVMF_CODE"
    echo "Install with: sudo apt install ovmf"
    exit 1
fi

# Copy VARS file to local directory (writable)
if [ -f "$OVMF_VARS" ]; then
    echo "Copying OVMF VARS to local directory (writable copy)..."
    cp "$OVMF_VARS" "$OVMF_VARS_COPY"
    chmod 644 "$OVMF_VARS_COPY"
fi

echo "Using OVMF firmware: $OVMF_CODE"
echo "Launching QEMU..."
echo "Press Ctrl+C to exit"
echo ""

# Launch QEMU with local VARS copy
qemu-system-x86_64 \
    -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
    -drive if=pflash,format=raw,file="$OVMF_VARS_COPY" \
    -drive file="$IMAGE",format=raw \
    -m 256M \
    -net none \
    -serial stdio \
    -vga std
