#include "main.h"
#include "efidef.h"
#include "efierr.h"
#include "images.h"

Screen curScreen = {
    .ScreenHeight = 768,
    .ScreenWidth = 1280,
    .redraw = FALSE,
    .isRunning = FALSE
};

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable) {

    Globalize(ImageHandle, SystemTable);
    /* Reset the input and output buffers to a known stat*/
    cin->Reset(cin, FALSE);
    cout->Reset(cout, FALSE);
    cerr->Reset(cerr, FALSE);

    connect_all_controllers();

    /* Clear Screen */
    cout->ClearScreen(cout);

    /* Green on Black Text*/
    cout->SetAttribute(
        cout,
        EFI_TEXT_ATTR(EFI_GREEN, EFI_BLACK)
    );

    EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics = InitGraphics(curScreen.ScreenWidth,curScreen.ScreenHeight);
    EFI_SIMPLE_POINTER_PROTOCOL* activeMouse = InitMouse();

    Cursor curCursor = {
        .X = curScreen.ScreenWidth / 2,
        .Y = curScreen.ScreenHeight / 2,
        .moved = FALSE,
        .Lclicked = FALSE,
        .Rclicked = FALSE,
        .Over = {
            .Width = Pointer.Width*8,  // Actual screen width (8 clean pixels * 8)
            .Height = Pointer.Height*8, // Actual screen height
            // Allocate for 64x64 ACTUAL pixels
            .Data = Realloc(NULL, 0, (8*Pointer.Width) * (8*Pointer.Height) * sizeof(UINT32)), 
        },
        .normalimg = Pointer, // Your "Clean" 8x8 image data
        .clickedimg = ClickedPointer,
    };
    EFI_STATUS Status;

    /* Draw the background */
    DrawAlphaAwareImage(Graphics, &BgImage, 0, 0, 0);
    DrawAlphaAwareImage(Graphics, &IntroLayerOne, 0, curScreen.ScreenHeight-IntroLayerOne.Height*8, 1);
    /* Save the underlying bits of the cursor */
    Graphics->Blt(
        Graphics, 
        (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)curCursor.Over.Data, 
        EfiBltVideoToBltBuffer, 
        curCursor.X,
        curCursor.Y,
        0,
        0,
        curCursor.Over.Width, 
        curCursor.Over.Height, 
        0
    );

    if (activeMouse == NULL) {
        cout->OutputString(cout, L"\rNo mouse available for interactive test!\n");
        cout->OutputString(cout, L"\rPress any key to exit!\n");

        UINTN index;
        EFI_EVENT keyEvent = cin->WaitForKey;
        EFI_INPUT_KEY key;
        gBS->WaitForEvent(1, &keyEvent, &index);
        cin->ReadKeyStroke(cin, &key);
        return EFI_NOT_FOUND;
    }

    EFI_EVENT timerEvent;
    Status = gBS->CreateEvent(
        EVT_TIMER | EVT_NOTIFY_SIGNAL,
        TPL_CALLBACK,           /* Priority level */
        ScreenUpdate,           /* No notification function */
        NULL,                   /* No context */
        &timerEvent             /* Output: event handle */
    );
    
    if (EFI_ERROR(Status)) {
        cout->OutputString(cout, L"Failed to create timer event!\n");
        return Status;
    }
    /* Set up event array for waiting on mouse or keyboard input */
    UINTN index;
    EFI_EVENT Events[3];
    Events[0] = activeMouse->WaitForInput;
    Events[1] = cin->WaitForKey;
    Events[2] = timerEvent;
    EFI_INPUT_KEY key;

    /* Reset the active mouse */
    Status = activeMouse->Reset(activeMouse, TRUE);
    if (EFI_ERROR(Status)) {
        activeMouse->Reset(activeMouse, FALSE);
    }

    INTN X = curCursor.X, Y = curCursor.Y;

    EFI_SIMPLE_POINTER_STATE State;
    curScreen.isRunning = TRUE;
    /* 30 Frames per second */
    gBS->SetTimer(
        Events[2],
        TimerPeriodic,
        333333
    );
    /* Main Event Loop */
    while(curScreen.isRunning){
        /* Wait for the timer and mouse input or keyboard input */
        gBS->WaitForEvent(3,Events,&index);

        if(index == 0){
             /* Get the current mouse state */
            Status = activeMouse->GetState(activeMouse, &State);
            X += State.RelativeMovementX/2048;
            Y += State.RelativeMovementY/2048;
            // Clamp values
            if (X < 0) X = 0;
            if (Y < 0) Y = 0;
            if (X >= (INTN)(curScreen.ScreenWidth - (curCursor.normalimg.Width * 8))) 
                X = curScreen.ScreenWidth - (curCursor.normalimg.Width * 8);
            if (Y >= (INTN)(curScreen.ScreenHeight - (curCursor.normalimg.Height * 8))) 
                Y = curScreen.ScreenHeight - (curCursor.normalimg.Height * 8);
            if(State.LeftButton) curCursor.Lclicked = TRUE;
            if(State.RightButton) curCursor.Rclicked = TRUE;
            curCursor.moved = TRUE;

        }

        if (curScreen.redraw && curCursor.moved) {
            DoCursor(Graphics, &curCursor, X, Y);
            curScreen.redraw = FALSE;
        }

        /* Keyboard Event Triggered */
        if (index == 1) {

            Status = cin->ReadKeyStroke(cin, &key);
            if(EFI_ERROR(Status)){
                break;
            }
            /* Shutdown if ESC is pressed */
            else if(key.ScanCode == SCAN_ESC){
                gRS->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, NULL);
                break;
            }
        }
        /* If no input received skip */
        curScreen.redraw = FALSE;
    }
    return EFI_SUCCESS;
}
