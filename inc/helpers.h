#include "efibind.h"
#include "efidef.h"
#include <efi.h>

VOID* Realloc(EFI_SYSTEM_TABLE* SystemTable, void* Oldptr, UINTN Oldsize, UINTN Newsize);
CHAR16* InttoString(EFI_SYSTEM_TABLE* SystemTable, INTN val);
VOID connect_all_controllers(EFI_SYSTEM_TABLE* SystemTable);
