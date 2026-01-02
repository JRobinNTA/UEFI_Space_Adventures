#include "helpers.h"
/*
=========================
To allocate and free heap
=========================
*/
VOID* Realloc(EFI_SYSTEM_TABLE* SystemTable, void* Oldptr, UINTN Oldsize, UINTN Newsize){
    VOID* Newptr = NULL;
    EFI_STATUS Status;
    if(Newsize == 0){
        if(Oldptr != NULL){
            SystemTable->BootServices->FreePool(Oldptr);
        }
        return NULL;
    }
    Status = SystemTable->BootServices->AllocatePool(EfiBootServicesData, Newsize, &Newptr);
    if (EFI_ERROR(Status)) {
        return NULL;
    }
    if(Oldptr != NULL){
        UINTN BytesToCopy = (Oldsize < Newsize) ? Oldsize : Newsize;
        SystemTable->BootServices->CopyMem(Newptr, Oldptr, BytesToCopy);
        SystemTable->BootServices->FreePool(Oldptr);
    }
    return Newptr;
}

/*
===================
Int to CHAR16 array
===================
*/
CHAR16* InttoString(EFI_SYSTEM_TABLE* SystemTable, INTN val) {
    // Handle special case: zero
    if (val == 0) {
        CHAR16* result = NULL;
        SystemTable->BootServices->AllocatePool(EfiBootServicesData, 2 * sizeof(CHAR16), (VOID**)&result);
        if (result) {
            result[0] = L'0';
            result[1] = L'\0';
        }
        return result;
    }

    // Handle negative numbers
    BOOLEAN isNegative = FALSE;
    if (val < 0) {
        isNegative = TRUE;
        val = -val;
    }

    // Count digits
    INTN temp = val;
    UINTN digitCount = 0;
    while (temp > 0) {
        digitCount++;
        temp /= 10;
    }

    // Allocate string (digits + optional '-' + null terminator)
    UINTN strLen = digitCount + (isNegative ? 1 : 0) + 1;
    CHAR16* result = NULL;
    result = Realloc(SystemTable,NULL,0,strLen*sizeof(CHAR16));

    if(result == NULL) return NULL;

    // Fill in digits (backwards)
    UINTN pos = digitCount + (isNegative ? 1 : 0);
    result[pos] = L'\0';  // Null terminator

    temp = val;
    while (temp > 0) {
        pos--;
        result[pos] = L'0' + (temp % 10);
        temp /= 10;
    }

    // Add minus sign if negative
    if (isNegative) {
        result[0] = L'-';
    }

    return result;
}

VOID connect_all_controllers(EFI_SYSTEM_TABLE* SystemTable) {
    // Retrieve the list of all handles from the handle database
    EFI_STATUS Status;
    UINTN HandleCount = 0;
    EFI_HANDLE *HandleBuffer = NULL;
    UINTN HandleIndex = 0;
    Status = SystemTable->BootServices->LocateHandleBuffer(
        AllHandles, 
        NULL, 
        NULL, 
        &HandleCount, 
        &HandleBuffer
    );
    if (EFI_ERROR(Status) || !HandleBuffer) return;

    // Connect all controllers found on all handles
    for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) 
        Status = SystemTable->BootServices->ConnectController(
            HandleBuffer[HandleIndex], 
            NULL, 
            NULL, 
            TRUE
        );

    // Free handle buffer when done
    SystemTable->BootServices->FreePool(HandleBuffer);
}
