#include "main.h"

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable) {

    /* Reset the input and output buffers to a known stat*/
    SystemTable->ConIn->Reset(SystemTable->ConIn, FALSE);
    SystemTable->ConOut->Reset(SystemTable->ConOut,FALSE);

    connect_all_controllers(SystemTable);
    /* Clear Screen */
    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);

    /* Green on Black Text*/
    SystemTable->ConOut->SetAttribute(
        SystemTable->ConOut,
        EFI_TEXT_ATTR(EFI_GREEN, EFI_BLACK)
    );
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Initializing Mouse!\n");


    EFI_STATUS Status;
    EFI_GUID ptrGuid = EFI_SIMPLE_POINTER_PROTOCOL_GUID;
    EFI_HANDLE *Ptrhandlebuffer = NULL;
    UINTN Ptrhandlecount = 0;

    /* Get all the devices supporting SPP protocol */
    Status = SystemTable->BootServices->LocateHandleBuffer(
        /* Search by protocol GUID */
        ByProtocol,
        /* GUID to search for */
        &ptrGuid,
        NULL,
        &Ptrhandlecount,
        &Ptrhandlebuffer
    );

    if(EFI_ERROR(Status) || Ptrhandlecount == 0){
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\rNo Mouse devices found\n");
    }
    else {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\rNumber of mouse found :");
        CHAR16* count = InttoString(SystemTable, Ptrhandlecount);
        SystemTable->ConOut->OutputString(SystemTable->ConOut, count);
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\n");
        Realloc(SystemTable, count, 0, 0);

        for(UINTN i = 0; i < Ptrhandlecount; i++){
            EFI_SIMPLE_POINTER_PROTOCOL *Mouse = NULL;
            Status = SystemTable->BootServices->HandleProtocol(
                Ptrhandlebuffer[i],   // The handle to query
                &ptrGuid,              // The protocol GUID
                (VOID**)&Mouse      // OUTPUT: Protocol interface pointer
            );

            if (EFI_ERROR(Status)) {
                CHAR16* indexStr = InttoString(SystemTable, i);
                SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\r  Device ");
                SystemTable->ConOut->OutputString(SystemTable->ConOut, indexStr);
                SystemTable->ConOut->OutputString(SystemTable->ConOut, L":\n");
                SystemTable->ConOut->OutputString(SystemTable->ConOut, L":\r      Failed to open protocol\n");
                Realloc(SystemTable, indexStr, 0, 0);
                continue;
            }
            // Display device index
            CHAR16* indexStr = InttoString(SystemTable, i);
            SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\r  Device ");
            SystemTable->ConOut->OutputString(SystemTable->ConOut, indexStr);
            SystemTable->ConOut->OutputString(SystemTable->ConOut, L":\n");
            Realloc(SystemTable, indexStr, 0, 0);

            // Try to reset the device with extended verification
            Status = Mouse->Reset(Mouse, TRUE);
            if (EFI_ERROR(Status)) {
                SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\r     Extended reset failed, trying simple reset...\n");
                // If extended reset fails, try simple reset
                Status = Mouse->Reset(Mouse, FALSE);
                if (EFI_ERROR(Status)) {
                    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\r     Simple reset also failed!\n");
                } else {
                    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\r     Simple reset succeeded\n");
                }
            } else {
                SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\r     Extended reset succeeded\n");
            }
            SystemTable->BootServices->FreePool(Ptrhandlebuffer);
        }
    }

    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\rStarting Interactive Test\n");
    
    // Try to get the first Simple Pointer Protocol device for interactive testing
    EFI_SIMPLE_POINTER_PROTOCOL *activeMouse = NULL;
    EFI_GUID sppGuid2 = EFI_SIMPLE_POINTER_PROTOCOL_GUID;
    
    Status = SystemTable->BootServices->LocateProtocol(
        &sppGuid2,
        NULL,
        (VOID **)&activeMouse
    );

    if (EFI_ERROR(Status)) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\rNo mouse available for interactive test!\n");
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\rPress any key to exit!\n");

        UINTN index;
        EFI_EVENT keyEvent = SystemTable->ConIn->WaitForKey;
        EFI_INPUT_KEY key;
        SystemTable->BootServices->WaitForEvent(1, &keyEvent, &index);
        SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &key);
        return EFI_NOT_FOUND;
    }

    // Set up event array for waiting on mouse or keyboard input
    UINTN index;
    EFI_EVENT Events[2];
    Events[0] = activeMouse->WaitForInput;  // Mouse input event
    Events[1] = SystemTable->ConIn->WaitForKey;  // Keyboard input event

    EFI_INPUT_KEY key;

    // Reset the active mouse
    Status = activeMouse->Reset(activeMouse, TRUE);
    if (EFI_ERROR(Status)) {
        activeMouse->Reset(activeMouse, FALSE);
    }

    INTN X = 0, Y = 0;
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\rMove the mouse to see position updates\n");
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\rPress any key to exit!\n\n");

    EFI_SIMPLE_POINTER_STATE State;
    
    // Main event loop
    while (TRUE) {
        // Wait for either mouse input or keyboard input
        SystemTable->BootServices->WaitForEvent(2, Events, &index);
        
        if (index == 0) {  // Mouse event triggered
            // Get the current mouse state
            Status = activeMouse->GetState(activeMouse, &State);
            
            if (!EFI_ERROR(Status)) {
                // Accumulate relative movements
                X += State.RelativeMovementX;
                Y += State.RelativeMovementY;
                
                // Display current position and button states
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
                
                // Free allocated strings
                Realloc(SystemTable, sX, 0, 0);
                Realloc(SystemTable, sY, 0, 0);
            }
            else if (Status == EFI_NOT_READY) {
                // Mouse has no new data available
                SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\rMouse Not Ready");
            }
        }
        
        if (index == 1) {  // Keyboard event triggered
            // Read the keystroke to clear the event
            SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &key);
            break;  // Exit the loop
        }
    }

    return EFI_SUCCESS;
}
