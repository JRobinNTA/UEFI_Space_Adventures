#!/bin/bash
# create_disk.sh - UEFI disk image creator
set -e

IMAGE="uefi_disk.img"
BINARY="build/UEFIApp.efi"
IMAGE_SIZE_MB=48  # 48MB image (much larger than 1.44MB floppy)

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

# Hardcoded OVMF paths
OVMF_CODE="OVMF_CODE.fd"
OVMF_VARS="OVMF_VARS.fd"

echo "Launching QEMU..."
qemu-system-x86_64 \
    -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
    -drive if=pflash,format=raw,file="$OVMF_VARS" \
    -drive file="$IMAGE",format=raw \
    -m 256M \
    -net none \
    -serial stdio \
    -vga std \
    -device qemu-xhci,id=xhci \
    -device usb-mouse,bus=xhci.0 \
    -device usb-tablet,bus=xhci.0
