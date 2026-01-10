#include "efierr.h"
#include "Helpers.h"
#include "Images.h"
#include "Game.h"

Screen curScreen = {
    .ScreenHeight = 768,
    .ScreenWidth = 1280,
    .redraw = FALSE,
    .isRunning = FALSE
};

Game curGame = {
    .state = LAUNCH,
};

ImageData patchBuffer;

SpriteLayers curSprites[4];

zBuffers curLayers[4];

UINT32 scratchData[LOGICAL_SCREEN_H*LOGICAL_SCREEN_W];
ImageData scratchBuffer;

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
    patchBuffer.Data = Realloc(NULL,0,sizeof(UINT32)*curScreen.ScreenHeight*curScreen.ScreenWidth);
    patchBuffer.Height = curScreen.ScreenHeight;
    patchBuffer.Width = curScreen.ScreenWidth;
    EFI_SIMPLE_POINTER_PROTOCOL* activeMouse = InitMouse();

    Mouse curMouse = {
        .cursor.X = curScreen.ScreenWidth / 2,
        .cursor.Y = curScreen.ScreenHeight / 2,
        .cursor.reDraw = FALSE,
        .cursor.Lclicked = FALSE,
        .cursor.Rclicked = FALSE,
        .normalimg = Pointer, // Your "Clean" 8x8 image data
        .clickedimg = ClickedPointer,
    };
    EFI_STATUS Status;

    /* Draw the background */
    // patchImg *pQue;
    setupScreenQue();
    scratchBuffer.Data = scratchData;
    patchImg *ImgRocket = RequestPatchFromPool(3);
    ImgRocket->Img = &Rocket;
    ImgRocket->X = 0;
    ImgRocket->Y = 0;
    ImgRocket->imageState = PERSIST;
    ImgRocket->Nframes = 500;
    ImgRocket->isDrawn = FALSE;

    patchImg *backG = RequestPatchFromPool(0);
    backG->imageState = PERMANENT;
    backG->Img = &BgImage;
    backG->X = 0;
    backG->Y = 0;
    backG->isDrawn = FALSE;

    patchImg *frontL = RequestPatchFromPool(1);
    frontL->imageState = PERMANENT;
    frontL->Img = &IntroLayerOne;
    frontL->X = 0;
    frontL->Y = curScreen.ScreenHeight-IntroLayerOne.Height*8;
    frontL->isDrawn = FALSE;

    DoScreenUpdates(Graphics);
    // DrawAAwarePixelImage(Graphics, &IntroLayerOne, 0, curScreen.ScreenHeight-IntroLayerOne.Height*8);
    /* Save the underlying bits of the cursor */
    // SaveDirectImage(Graphics, &curMouse.Over, curMouse.cursor.X, curMouse.cursor.Y);
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

    Cursor cursPos ={
        .Lclicked = curMouse.cursor.Lclicked,
        .Rclicked = curMouse.cursor.Rclicked,
        .X = curMouse.cursor.X,
        .Y = curMouse.cursor.Y,
        .reDraw = curMouse.cursor.reDraw,
    };

    /* 30 Frames per second */
    gBS->SetTimer(
        Events[2],
        TimerPeriodic,
        333333
    );
    /* Main Event Loop */
    curScreen.isRunning = TRUE;
    while(curScreen.isRunning){
        /* Wait for the timer and mouse input or keyboard input */
        gBS->WaitForEvent(3,Events,&index);
        if(index == 0){
             /* Get the current mouse state */
            GetMouseUpdates(activeMouse, &cursPos);

        }

        if (curScreen.redraw && cursPos.reDraw) {
            DoCursor(Graphics, &curMouse, &cursPos);
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
