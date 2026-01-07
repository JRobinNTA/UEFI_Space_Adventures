#include "efibind.h"
#include "efidef.h"
#include <efi.h>

extern SIMPLE_TEXT_OUTPUT_INTERFACE *cout;
extern SIMPLE_INPUT_INTERFACE *cin;
extern SIMPLE_TEXT_OUTPUT_INTERFACE *cerr;  // Stderr can be set to a serial output or other non-display device.
extern EFI_SYSTEM_TABLE *gST;
extern EFI_BOOT_SERVICES *gBS;
extern EFI_RUNTIME_SERVICES *gRS;
extern EFI_HANDLE image;

VOID* Realloc(void* Oldptr, UINTN Oldsize, UINTN Newsize);
CHAR16* InttoString(INTN val);
VOID connect_all_controllers();
void Globalize(EFI_HANDLE handle, EFI_SYSTEM_TABLE *systable);
EFI_SIMPLE_POINTER_PROTOCOL* InitMouse();
EFI_GRAPHICS_OUTPUT_PROTOCOL* InitGraphics(UINTN targetWidth, UINTN targetHeight);

