#include "efibind.h"
#include "efidef.h"
#include "efierr.h"
#include <efi.h>

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


EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable) {

    /* Clear Screen */
    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
    /* Red on Black Text*/
    SystemTable->ConOut->SetAttribute(
        SystemTable->ConOut,
        EFI_TEXT_ATTR(EFI_GREEN, EFI_BLACK)
    );
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Initializing Mouse!\r\n");


    EFI_STATUS Status;
    EFI_GUID ptrGuid = EFI_SIMPLE_POINTER_PROTOCOL_GUID;
    EFI_SIMPLE_POINTER_PROTOCOL *Mouse;
    /* Get the Mouse object*/
    Status = SystemTable->BootServices->LocateProtocol(
        &ptrGuid,
        NULL,
        (VOID **)&Mouse
    );

    UINTN index;
    EFI_EVENT Events[2];
    Events[1] = SystemTable->ConIn->WaitForKey;

    EFI_INPUT_KEY key;

    if (EFI_ERROR(Status)){
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Failed to Initialize Mouse!\n");
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Press any key to exit!\n");
        SystemTable->BootServices->WaitForEvent(1,&Events[1], &index);
        SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &key);

        return EFI_NOT_FOUND;
    }

    Events[0] = Mouse->WaitForInput;

    Status = Mouse->Reset(Mouse, TRUE);
    if (EFI_ERROR(Status)) {
        Mouse->Reset(Mouse, FALSE);
    }

    INTN X = 0,Y = 0;
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Waiting for Mouse Output\n");
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Press any key to exit!\n");

    EFI_SIMPLE_POINTER_STATE State;
    while (TRUE) {
        SystemTable->BootServices->WaitForEvent(2, Events, &index);
        if(index == 0){
            Status = Mouse->GetState(Mouse, &State);
            if (!EFI_ERROR(Status)){
                X += State.RelativeMovementX;
                Y += State.RelativeMovementY;
                SystemTable->ConOut->OutputString(SystemTable->ConOut,L"\rMouse Works");
                CHAR16* sX = InttoString(SystemTable, X);
                CHAR16* sY = InttoString(SystemTable, Y);
                SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\rX: ");
                SystemTable->ConOut->OutputString(SystemTable->ConOut, sX);
                SystemTable->ConOut->OutputString(SystemTable->ConOut, L"  Y: ");
                SystemTable->ConOut->OutputString(SystemTable->ConOut, sY);
                SystemTable->ConOut->OutputString(SystemTable->ConOut, L"  L:");
                SystemTable->ConOut->OutputString(SystemTable->ConOut, State.LeftButton ? L"1" : L"0");
                SystemTable->ConOut->OutputString(SystemTable->ConOut, L" R:");
                SystemTable->ConOut->OutputString(SystemTable->ConOut, State.RightButton ? L"1" : L"0");
                SystemTable->ConOut->OutputString(SystemTable->ConOut, L"    ");
                Realloc(SystemTable, sX, 0, 0);
                Realloc(SystemTable, sY, 0, 0);
            }
            else if(Status == EFI_NOT_READY){
                SystemTable->ConOut->OutputString(SystemTable->ConOut,L"\rMouse Not Ready");
            }
        }
        /* Fix the following line to output x and y axis and left and right button states */
        // Wait for Keypress
        if(index == 1){
            SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &key);
            break;
        }
    }

    return EFI_SUCCESS;
}
