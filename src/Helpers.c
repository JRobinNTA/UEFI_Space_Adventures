#include "Helpers.h"
#include "efibind.h"
#include "efidef.h"
#include "efipoint.h"
#include "efiprot.h"


SIMPLE_TEXT_OUTPUT_INTERFACE *cout;
SIMPLE_INPUT_INTERFACE *cin;
SIMPLE_TEXT_OUTPUT_INTERFACE *cerr;  // Stderr can be set to a serial output or other non-display device.
EFI_SYSTEM_TABLE *gST;
EFI_BOOT_SERVICES *gBS;
EFI_RUNTIME_SERVICES *gRS;
EFI_HANDLE image;

/*
=========================
To allocate and free heap
=========================
*/
VOID* Realloc(void* Oldptr, UINTN Oldsize, UINTN Newsize){
    VOID* Newptr = NULL;
    EFI_STATUS Status;
    if(Newsize == 0){
        if(Oldptr != NULL){
            gBS->FreePool(Oldptr);
        }
        return NULL;
    }
    Status = gBS->AllocatePool(EfiBootServicesData, Newsize, &Newptr);
    if (EFI_ERROR(Status)) {
        return NULL;
    }
    if(Oldptr != NULL){
        UINTN BytesToCopy = (Oldsize < Newsize) ? Oldsize : Newsize;
        gBS->CopyMem(Newptr, Oldptr, BytesToCopy);
        gBS->FreePool(Oldptr);
    }
    return Newptr;
}

/*
===================
Int to CHAR16 array
===================
*/
CHAR16* InttoString(INTN val) {
    // Handle special case: zero
    if (val == 0) {
        CHAR16* result = NULL;
        gBS->AllocatePool(EfiBootServicesData, 2 * sizeof(CHAR16), (VOID**)&result);
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
    result = Realloc(NULL,0,strLen*sizeof(CHAR16));

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

VOID connect_all_controllers() {
    // Retrieve the list of all handles from the handle database
    EFI_STATUS Status;
    UINTN HandleCount = 0;
    EFI_HANDLE *HandleBuffer = NULL;
    UINTN HandleIndex = 0;
    Status = gBS->LocateHandleBuffer(
        AllHandles, 
        NULL, 
        NULL, 
        &HandleCount, 
        &HandleBuffer
    );
    if (EFI_ERROR(Status) || !HandleBuffer) return;

    // Connect all controllers found on all handles
    for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) 
        Status = gBS->ConnectController(
            HandleBuffer[HandleIndex], 
            NULL, 
            NULL, 
            TRUE
        );

    // Free handle buffer when done
    gBS->FreePool(HandleBuffer);
}

/*
==============================
Initialize all the global Vars
==============================
*/
void Globalize(EFI_HANDLE handle, EFI_SYSTEM_TABLE *systable) {
    cout = systable->ConOut;
    cin = systable->ConIn;
    cerr = systable->StdErr;  // Stderr can be set to a serial output or other non-display device.
    cerr = cout;                // Use stdout for error printing 
    gST = systable;
    gBS = gST->BootServices;
    gRS = gST->RuntimeServices;
    image = handle;
}

/*
=================================
Search and return an active Mouse
=================================
*/
EFI_SIMPLE_POINTER_PROTOCOL* InitMouse(){
    cout->OutputString(cout, L"Initializing Mouse!\n");


    EFI_STATUS Status;
    EFI_GUID ptrGuid = EFI_SIMPLE_POINTER_PROTOCOL_GUID;
    EFI_HANDLE *Ptrhandlebuffer = NULL;
    UINTN Ptrhandlecount = 0;

    /* Get all the devices supporting SPP protocol */
    Status = gBS->LocateHandleBuffer(
        /* Search by protocol GUID */
        ByProtocol,
        /* GUID to search for */
        &ptrGuid,
        NULL,
        &Ptrhandlecount,
        &Ptrhandlebuffer
    );

    /* Empty buffer to store the working mouse*/
    EFI_SIMPLE_POINTER_PROTOCOL *activeMouse = NULL;

    /* No Mouse found */
    if(EFI_ERROR(Status) || Ptrhandlecount == 0){
        cout->OutputString(cout, L"\rNo Mouse devices found\n");
    }
    else {
        cout->OutputString(cout, L"\rNumber of mouse found :");
        CHAR16* count = InttoString(Ptrhandlecount);
        cout->OutputString(cout, count);
        cout->OutputString(cout, L"\n");
        Realloc(count, 0, 0);

        /* Find the first working handle */
        for(UINTN i = 0; i < Ptrhandlecount; i++){
            EFI_SIMPLE_POINTER_PROTOCOL *Mouse = NULL;
            Status = gBS->HandleProtocol(
                Ptrhandlebuffer[i],
                &ptrGuid,
                (VOID**)&Mouse
            );

            if (EFI_ERROR(Status)) {
                CHAR16* indexStr = InttoString(i);
                cout->OutputString(cout, L"\r  Device ");
                cout->OutputString(cout, indexStr);
                cout->OutputString(cout, L":\n");
                cout->OutputString(cout, L":\r      Failed to open protocol\n");
                Realloc(indexStr, 0, 0);
                continue;
            }
            /* Show the Device Index */
            CHAR16* indexStr = InttoString(i);
            cout->OutputString(cout, L"\r  Device ");
            cout->OutputString(cout, indexStr);
            cout->OutputString(cout, L":\n");
            Realloc(indexStr, 0, 0);

            /* Try to reset the device with extended verification */
            Status = Mouse->Reset(Mouse, TRUE);
            if (EFI_ERROR(Status)) {
                cout->OutputString(cout, L"\r     Extended reset failed, trying simple reset...\n");
                /* If extended reset fails, try simple reset */
                Status = Mouse->Reset(Mouse, FALSE);
                if (EFI_ERROR(Status)) {
                    cout->OutputString(cout, L"\r     Simple reset also failed!\n");
                } else {
                    cout->OutputString(cout, L"\r     Simple reset succeeded\n");
                    activeMouse = Mouse;
                    break;
                }
            } else {
                cout->OutputString(cout, L"\r     Extended reset succeeded\n");
                cout->OutputString(cout, L"\rWorking Mouse Found!\n");
                activeMouse = Mouse;
                break;
            }
        }
    }

    gBS->FreePool(Ptrhandlebuffer);
    return activeMouse;
}

/*
========================================================
Search and Setup a graphics Mode with a given resolution
========================================================
*/
EFI_GRAPHICS_OUTPUT_PROTOCOL* InitGraphics(UINTN targetWidth, UINTN targetHeight){
    EFI_GUID guGraphics = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Graphics = NULL;
    EFI_STATUS Status;

    Status = gBS->LocateProtocol(&guGraphics, NULL, (VOID**)&Graphics);

    if(EFI_ERROR(Status) || Graphics == NULL){
        cout->OutputString(cout, L"\rGraphics Not Supported\n");
        return NULL;
    }

    cout->OutputString(cout, L"\rSearching for ");
    CHAR16* tgtW = InttoString(targetWidth);
    CHAR16* tgtH = InttoString(targetHeight);
    cout->OutputString(cout, tgtW);
    cout->OutputString(cout, L" x ");
    cout->OutputString(cout, tgtH);
    cout->OutputString(cout, L" mode...\n");
    Realloc(tgtW, 0, 0);
    Realloc(tgtH, 0, 0);

    UINTN maxMode = Graphics->Mode->MaxMode;
    UINTN bestMode = Graphics->Mode->Mode;
    UINTN bestDiff = (UINTN)-1;

    for(UINTN i = 0; i < maxMode; i++){
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = NULL;
        UINTN size;

        Status = Graphics->QueryMode(Graphics, i, &size, &info);
        if(EFI_ERROR(Status)) continue;

        /* Calculate difference from target */
        UINTN widthDiff = info->HorizontalResolution > targetWidth ? 
                          info->HorizontalResolution - targetWidth : 
                          targetWidth - info->HorizontalResolution;
        UINTN heightDiff = info->VerticalResolution > targetHeight ? 
                           info->VerticalResolution - targetHeight : 
                           targetHeight - info->VerticalResolution;
        UINTN totalDiff = widthDiff + heightDiff;

        /* Check for exact match */
        if (info->HorizontalResolution == targetWidth && 
            info->VerticalResolution == targetHeight) {
            cout->OutputString(cout, L"\rFound exact match at mode ");
            CHAR16* modeStr = InttoString(i);
            cout->OutputString(cout, modeStr);
            cout->OutputString(cout, L"\n");
            Realloc(modeStr, 0, 0);

            if (i != Graphics->Mode->Mode) {
                Graphics->SetMode(Graphics, i);
            }
            return Graphics;
        }

        /* Track closest match */
        if (totalDiff < bestDiff) {
            bestDiff = totalDiff;
            bestMode = i;
        }
    }

    /* No exact match, use closest */
    if (bestMode != Graphics->Mode->Mode) {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info = NULL;
        UINTN size;
        Graphics->QueryMode(Graphics, bestMode, &size, &info);

        cout->OutputString(cout, L"\rNo exact match. Using closest: ");
        CHAR16* w = InttoString(info->HorizontalResolution);
        CHAR16* h = InttoString(info->VerticalResolution);
        cout->OutputString(cout, w);
        cout->OutputString(cout, L" x ");
        cout->OutputString(cout, h);
        cout->OutputString(cout, L"\n");
        Realloc(w, 0, 0);
        Realloc(h, 0, 0);

        Graphics->SetMode(Graphics, bestMode);
    }

    return Graphics;
}

