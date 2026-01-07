#include "Graphics.h"
#include "Helpers.h"
#include "efibind.h"
#include "efidef.h"
#include "efiprot.h"


/* Draw rectangle */
void DrawRect(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, UINT32 x, UINT32 y, UINT32 color) {
    gop->Blt(
        gop,
        (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)&color,     // Source: single color
        EfiBltVideoFill,                            // Operation: fill with color
        0, 0,                                       // Source X, Y (ignored for fill)
        x, y,                                       // Destination X, Y on screen
        8, 8,                                       // Width, Height (8x8 block)
        0                                           // Delta (ignored for fill)
    );
}

BOOLEAN BinaryAlpha(UINT32 Pixel){
    if((Pixel >> 24) == 0xff){
        return TRUE;
    }
    else return FALSE;
}

/*
==================================================
Draw a given Alpha Aware Pixelated image to screen
==================================================
*/
VOID DrawAAwarePixelImage(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, ImageData *img, UINT32 x, UINT32 y) {
    if (gop == NULL || img == NULL || img->Data == NULL) {
        return;
    }

    for (UINT32 imgY = 0; imgY < img->Height; imgY++) {

        /* Multiply by 8 to scale the position back up for the screen */
        UINT32 screenY = y + (imgY * 8);

        if (screenY >= curScreen.ScreenHeight) break;

        for (UINT32 imgX = 0; imgX < img->Width; imgX++) {

            UINT32 screenX = x + (imgX * 8);

            if (screenX >= curScreen.ScreenWidth) continue;

            /* Since it's a standard array now, index normally */
            UINT32 pixelIndex = imgY * img->Width + imgX;

            /* Draw an 8x8 block for this single pixel data */
            if(BinaryAlpha(img->Data[pixelIndex])){

                DrawRect(gop, screenX, screenY, img->Data[pixelIndex]);
            }
            else continue;
        }
    }
}

VOID ExplodeAUnAwarePixelImage(ImageData *Pimg, ImageData *NPimg, UINTN factor) {
    NPimg->Height = Pimg->Height * factor;
    NPimg->Width = Pimg->Width * factor;

    for (UINTN i = 0; i < Pimg->Height; i++) {
        /* Fill the "Master Row" for this logical row */
        UINTN destRowStart = (i * factor) * NPimg->Width;

        for (UINTN j = 0; j < Pimg->Width; j++) {
            UINT32 pixel = Pimg->Data[i * Pimg->Width + j];

            /* Repeat the pixel 'factor' times horizontally */
            for (UINTN k = 0; k < factor; k++) {
                NPimg->Data[destRowStart + (j * factor) + k] = pixel;
            }
        }

        /* The Master Row is now complete. Copy it to the next (factor - 1) rows. */
        for (UINTN l = 1; l < factor; l++) {
            UINTN nextRowStart = destRowStart + (l * NPimg->Width);

            gBS->CopyMem(
                &NPimg->Data[nextRowStart],    // Destination: The row below
                &NPimg->Data[destRowStart],    // Source: The master row we just built
                NPimg->Width * sizeof(UINT32)  // Size: One full exploded row
            );
        }
    }
    NPimg->isPixel = FALSE;
}


VOID DrawDirectImage(EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics, ImageData* img, UINTN imgX, UINTN imgY){

    Graphics->Blt(
        Graphics, 
        (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)img->Data, 
        EfiBltBufferToVideo, 
        0, 
        0, 
        imgX, 
        imgY,
        img->Width, 
        img->Height, 
        0
    );

}


VOID ExplodeAAwarePixelImage(
    ImageData *Pimg,                // Foreground (Downscaled)
    ImageData *NPimg,               // Scratchpad (Exploded)
    UINTN factor,
    BOOLEAN useBg,
    EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics,
    UINTN screenX, UINTN screenY    
) {
    NPimg->Height = Pimg->Height * factor;
    NPimg->Width = Pimg->Width * factor;

    /* PRE-FILL THE SCRATCHPAD (BACKGROUND) */
    if (!useBg) {
        /* Capture from Live Screen */
        SaveDirectImage(Graphics, NPimg, screenX, screenY);
    } 
    else {
        /* curBgImg is our source, NPimg is the target buffer */
        SaveIndirectImage(&curBgImg, NPimg, screenX, screenY);

        /* Crucial Check: If the background we just pulled is ALSO pixelated explode it as well*/
        if (NPimg->isPixel) {
            /* To avoid recursion, we use a simple unaware explosion */
            ExplodeAUnAwarePixelImage(NPimg, NPimg, factor);
        }
    }

    /* OVERWRITE FOREGROUND (STENCIL) */
    for (UINTN i = 0; i < Pimg->Height; i++) {
        for (UINTN j = 0; j < Pimg->Width; j++) {
            UINT32 fgPixel = Pimg->Data[i * Pimg->Width + j];

            /* Only overwrite if the Foreground pixel is NOT Alpha-transparent */
            if (BinaryAlpha(fgPixel)) {
                for (UINTN v = 0; v < factor; v++) {
                    UINTN destIdxBase = ((i * factor + v) * NPimg->Width) + (j * factor);
                    for (UINTN h = 0; h < factor; h++) {
                        NPimg->Data[destIdxBase + h] = fgPixel;
                    }
                }
            }
        }
    }

    NPimg->isPixel = FALSE; 
}

/*
====================================================
Draw a given Alpha UnAware Pixelated image to screen
====================================================
*/
VOID DrawAUnAwarePixelImage(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, ImageData *img, UINT32 x, UINT32 y) {
    if (gop == NULL || img == NULL || img->Data == NULL) {
        return;
    }

    for (UINT32 imgY = 0; imgY < img->Height; imgY++) {

        /* Multiply by 8 to scale the position back up for the screen */
        UINT32 screenY = y + (imgY * 8);

        if (screenY >= curScreen.ScreenHeight) break;

        for (UINT32 imgX = 0; imgX < img->Width; imgX++) {

            UINT32 screenX = x + (imgX * 8);

            if (screenX >= curScreen.ScreenWidth) continue;

            /* Since it's a standard array now, index normally */
            UINT32 pixelIndex = imgY * img->Width + imgX;

            /* Draw an 8x8 block for this single pixel data */
            DrawRect(gop, screenX, screenY, img->Data[pixelIndex]);
        }
    }
}


VOID DoCursor(EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics, Mouse *cur, Cursor *curCurs) {
    if (!curCurs->reDraw || Graphics == NULL) return;

    /* RESTORE using 64x64 screen area */
    DrawScreenUpdates(Graphics, &cur->Over, FALSE,cur->cursor.X, cur->cursor.Y);

    /* SAVE using 64x64 screen area */
    SaveDirectImage(Graphics, &cur->Over, curCurs->X, curCurs->Y);

    /* DRAW the cursor using your 8x8 clean data (DrawImage scales it to 64x64) */
    if(curCurs->Lclicked || curCurs->Rclicked){
        DrawScreenUpdates(Graphics, &cur->clickedimg, FALSE, curCurs->X, curCurs->Y);
        cur->cursor.Lclicked = curCurs->Lclicked;
        cur->cursor.Rclicked = curCurs->Rclicked;
    }
    else {

        DrawScreenUpdates(Graphics, &cur->normalimg, FALSE, curCurs->X, curCurs->Y);
    }

    curCurs->reDraw = FALSE;
    curCurs->Lclicked = FALSE;
    curCurs->Rclicked = FALSE;
    cur->cursor.X = curCurs->X;
    cur->cursor.Y = curCurs->Y;
}


VOID EFIAPI ScreenUpdate(EFI_EVENT Event, VOID *Context) {
    curScreen.redraw = TRUE;
}


VOID SaveDirectImage(EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics,ImageData* img, UINTN screenX, UINTN screenY){
    Graphics->Blt(
        Graphics, 
        (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)img->Data,
        EfiBltVideoToBltBuffer,
        screenX,
        screenY,
        0,
        0,
        img->Height,
        img->Width,
        0
    );

}


VOID SaveIndirectImage(ImageData *saveImg, ImageData *bufferImg, UINTN imgX, UINTN imgY){
    UINTN sourceX = imgX / 8;
    UINTN sourceY = imgY / 8;
    for (UINTN row = 0; row < bufferImg->Height; row++) {
        /* Calculate the row offset in the full-size background */
        UINTN bgOffset = ((sourceY+ row) * bufferImg->Width) + sourceX;
        /* Calculate the row offset in the small 64x64 patch */
        UINTN patchOffset = row * bufferImg->Width;

        /* Perform a high-speed memory copy */
        gBS->CopyMem(
            &bufferImg->Data[patchOffset], 
            &saveImg->Data[bgOffset], 
            bufferImg->Width * sizeof(UINT32)
        );
    }
    bufferImg->isPixel = saveImg->isPixel;
    bufferImg->isAlpha = saveImg->isAlpha;
}


VOID GetMouseUpdates(EFI_SIMPLE_POINTER_PROTOCOL *Mouse, Cursor *cursPos){
    EFI_SIMPLE_POINTER_STATE State;
    EFI_STATUS Status;
    Status = Mouse->GetState(Mouse, &State);
    if(Status == EFI_NOT_READY){
        cursPos->reDraw = FALSE;
        cursPos->Lclicked = FALSE;
        cursPos->Rclicked = FALSE;
    }
    cursPos->X += State.RelativeMovementX/2048;
    cursPos->Y += State.RelativeMovementY/2048;
    /* Clamp values */
    if (cursPos->X < 0) cursPos->X = 0;
    if (cursPos->Y < 0) cursPos->Y = 0;
    if (cursPos->X >= (INTN)(curScreen.ScreenWidth - 64)) 
        cursPos->X = curScreen.ScreenWidth - 64;
    if (cursPos->Y >= (INTN)(curScreen.ScreenHeight - 64)) 
        cursPos->Y = curScreen.ScreenHeight - 64;
    if(State.LeftButton) cursPos->Lclicked = TRUE;
    if(State.RightButton) cursPos->Rclicked = TRUE;
    cursPos->reDraw = TRUE;
}

VOID setupScreenQue(patchImg** pQue) {
    /* Allocate all 8 slots at once */
    *pQue = Realloc(NULL, 0, 16 * sizeof(patchImg));

    if (*pQue == NULL) return;

    for (UINTN i = 0; i < 16; i++) {
        /* Access using the arrow on the base pointer, then index */
        (*pQue)[i].imageState = STALE; 
        (*pQue)[i].Img = NULL;
        (*pQue)[i].replacedImg = NULL;
        (*pQue)[i].X = 0;
        (*pQue)[i].Y = 0;
    }
}


patchImg* RequestPatchFromPool(patchImg *pQue){
    patchImg* found = NULL;
    for(UINTN i = 0; i < 16; i++){
        /* Find the first inactive buffer and push the image */
        if(pQue[i].imageState == STALE && !pQue[i].isDrawn){
            found = &pQue[i];
            return found;
        }
        /* If a Permanent image has been already drawn give up the pointer */
        else if (pQue[i].imageState == PERMANENT && pQue[i].isDrawn){
            found = &pQue[i];
            return found;
        }

    }
    return NULL;
}


VOID DrawScreenUpdates(EFI_GRAPHICS_OUTPUT_PROTOCOL *Graphics, ImageData *DrawImg, BOOLEAN useBg, UINTN screenX,UINTN screenY){
    if(DrawImg == NULL) return;
    ImageData *SourceToBlt = DrawImg; // Default to drawing the original data
    if(DrawImg->isPixel){
        if(DrawImg->isAlpha){
            ExplodeAAwarePixelImage(
                DrawImg, 
                &patchBuffer, 
                8,
                useBg,
                Graphics,
                screenX,
                screenY
            );

        }
        else{
            ExplodeAUnAwarePixelImage(DrawImg,&patchBuffer, 8);

        }
        SourceToBlt = &patchBuffer;
    }
    DrawDirectImage(Graphics,SourceToBlt,screenX,screenY);
}


VOID SaveScreenUpdates(EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics, patchImg *pQue){
    switch(pQue->imageState){
        case ACTIVE:
        case PERSIST:{
            if(pQue->useBg){
                SaveIndirectImage(&curBgImg,pQue->replacedImg,pQue->X,pQue->Y);
            }
            else{
                SaveDirectImage(Graphics, pQue->replacedImg, pQue->X, pQue->Y);
            }
            break;
        }
        case STALE:
        case PERMANENT:
            break;

    }
}


VOID DoScreenUpdates(EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics, patchImg *pQue){
    for(UINTN i = 0; i < 16; i++){
        SaveScreenUpdates(Graphics,&pQue[i]);
        /* Break if no more images left to draw */
        if(pQue[i].imageState == STALE && !pQue[i].isDrawn) break;
        switch(pQue[i].imageState){
            /* Draw the image */
            case ACTIVE:{
                DrawScreenUpdates(Graphics,pQue[i].Img,pQue[i].useBg,pQue[i].X,pQue[i].Y);
                pQue[i].imageState = STALE;
                pQue[i].isDrawn = TRUE;
                break;
            }
            /* Replace the image */
            case STALE:{
                if(pQue[i].isDrawn){
                    DrawScreenUpdates(Graphics,pQue[i].replacedImg,FALSE,pQue[i].X,pQue[i].Y);
                    pQue[i].isDrawn = FALSE;
                }
                break;
            }
            /* Let the image Persist for a number of frames */
            case PERSIST:{
                /* If the image was not drawn draw it*/
                if(pQue[i].isDrawn){
                    pQue[i].Cframes++;
                    if(pQue[i].Cframes >= pQue[i].Nframes) pQue[i].imageState = STALE;
                }
                /* Count the frames for which artifact must persist */
                else{
                    DrawScreenUpdates(Graphics,pQue[i].Img,pQue[i].useBg,pQue[i].X,pQue[i].Y);
                    pQue[i].isDrawn = TRUE;
                }
                break;
            }
            /* Permanent Image if it is drawn already do nothing */
            case PERMANENT:{
                if(!pQue[i].isDrawn){
                    DrawScreenUpdates(Graphics,pQue[i].Img,pQue[i].useBg,pQue[i].X,pQue[i].Y);
                    pQue[i].isDrawn = TRUE;
                }
                break;
            }
        }
    }
}
