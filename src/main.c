// src/main.c
#include <efi.h>

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    // Use SystemTable directly, not ST
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Hello UEFI!\r\n");
    
    // Wait for keypress
    UINTN index;
    EFI_INPUT_KEY key;
    SystemTable->BootServices->WaitForEvent(1, &SystemTable->ConIn->WaitForKey, &index);
    SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &key);
    
    return EFI_SUCCESS;
}
