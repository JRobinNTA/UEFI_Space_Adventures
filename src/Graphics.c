#include "Graphics.h"
#include "Helpers.h"
#include "efibind.h"
#include "efidef.h"
#include "efiprot.h"


/*
========================
Draw rectangle to screen
========================
*/
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


/*
================================
Helper to implement transparency
================================
*/
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


/*
=============================================================
Explode a given Alpha UnAware Pixelated image to given buffer
=============================================================
*/
VOID ExplodeAUnAwarePixelImage(ImageData *Pimg, ImageData *NPimg ) {
    UINTN factor = 8;
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


/*
=====================================
Draw a given image directly to screen
=====================================
*/
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


/*
=================================================
Explode a given alpha aware pixel image to buffer
=================================================
*/
VOID ExplodeAAwarePixelImage(
    ImageData *Pimg,            // Logical sprite (e.g. 8x8)
    ImageData *NPimg,           // Screen-space output (e.g. 64x64)
    zOrder order,               // The layer the sprite belongs to
    BOOLEAN skipSprite,         // If TRUE, don't draw Pimg (used for restoring background)
    UINTN screenX,              // Actual screen COORDS
    UINTN screenY
) {
    UINTN factor = 8;
    if (!Pimg || !NPimg || !Pimg->Data ) return;

    UINTN gridX = screenX / factor;
    UINTN gridY = screenY / factor;
    
    /* Instead of exploding each layer by layer */
    /* first we compose the image fully and then explode */

    /* save the background fully opaque */
    scratchBuffer.Height = Pimg->Height;
    scratchBuffer.Width = Pimg->Width;
    scratchBuffer.isAlpha = TRUE;
    scratchBuffer.isPixel = TRUE;

    SaveIndirectImage(curLayers[0].Img, &scratchBuffer, screenX/8, screenY/8);
    for(UINTN i = 1; i < 4; i++){

        for(UINTN j = 0; j < Pimg->Height;j++){

            UINTN srcY = gridY + j;
            for(UINTN k = 0; k < Pimg->Width;k++){

                UINTN srcX = gridX + k;

                UINT32 pixel;
                /* if we reached the sprite layer pull the pixels from the sprite itself */
                if(i == order && !skipSprite){
                    pixel = Pimg->Data[j*Pimg->Width + k];
                }
                else{
                    pixel = curLayers[i].Img->Data[srcY * curLayers[i].Img->Width + srcX];
                }

                /* overwrite only opaque pixels */
                if(BinaryAlpha(pixel)){
                    scratchBuffer.Data[j*scratchBuffer.Width + k] = pixel;
                }
            }
        }
    }
    ExplodeAUnAwarePixelImage(&scratchBuffer,NPimg);
    NPimg->isPixel = FALSE; // Ready for Blt
    NPimg->isAlpha = TRUE;
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


/*
=====================
Do the cursor updates
=====================
*/
VOID DoCursor(EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics, Mouse *cur, Cursor *curCurs) {
    if (!curCurs->reDraw || Graphics == NULL) return;

    RestoreScreenUpdates(Graphics, &cur->normalimg, 3, cur->cursor.X, cur->cursor.Y);


    if(curCurs->Lclicked || curCurs->Rclicked){
        DrawScreenUpdates(Graphics, &cur->clickedimg, 3, curCurs->X, curCurs->Y);
        cur->cursor.Lclicked = curCurs->Lclicked;
        cur->cursor.Rclicked = curCurs->Rclicked;
    }
    else {

        DrawScreenUpdates(Graphics, &cur->normalimg, 3, curCurs->X, curCurs->Y);
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


/*
======================================
Save an image directly from the screen
======================================
*/
VOID SaveDirectImage(EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics,ImageData* img, UINTN screenX, UINTN screenY){
    Graphics->Blt(
        Graphics, 
        (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)img->Data,
        EfiBltVideoToBltBuffer,
        screenX,
        screenY,
        0,
        0,
        img->Width,
        img->Height,
        0
    );

}


/*
======================================
Save a given image from a given buffer
======================================
*/
VOID SaveIndirectImage(
    ImageData *saveImg, 
    ImageData *bufferImg, 
    UINTN imgX, // The coords are image coords not bloody screen coords
    UINTN imgY
){
    if (!saveImg || !bufferImg ) return;

    /* Remember imgX and imgY are image coords so no need to check for scaling here */

    /* If saveImg is larger capture a patch from saveImg at imgX and imgY to bufferImg */
    if (saveImg->Width >= bufferImg->Width && saveImg->Height >= bufferImg->Height) {
        for(UINTN i = 0; i < bufferImg->Height; i++){
            UINTN srcOffset = (imgY + i) * saveImg->Width + imgX;
            UINTN dstOffset = i * bufferImg->Width;
            gBS->CopyMem(
                &bufferImg->Data[dstOffset],
                &saveImg->Data[srcOffset],
                bufferImg->Width * sizeof(UINT32)
            );

        }

    }

    /* if bufferImg is large capture saveImg fully to bufferImg at imgX and imgY */
    else {
        for(UINTN i = 0; i < saveImg->Height; i++){
            UINTN srcOffset = i * saveImg->Width;
            UINTN dstOffset = ((imgY + i) * bufferImg->Width) + imgX;
            gBS->CopyMem(
                &bufferImg->Data[dstOffset],
                &saveImg->Data[srcOffset],
                saveImg->Width * sizeof(UINT32)
            );
        }

    }

    /* Preserve pixel/alpha mode from source image */
    bufferImg->isPixel = saveImg->isPixel;
    bufferImg->isAlpha = saveImg->isAlpha;
}


/*
===============================================
Update the cursPos object with the current data
===============================================
*/
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
    cursPos->X = (cursPos->X / 8) * 8;
    cursPos->Y = (cursPos->Y / 8) * 8;
    if(State.LeftButton) cursPos->Lclicked = TRUE;
    if(State.RightButton) cursPos->Rclicked = TRUE;
    cursPos->reDraw = TRUE;
}


/*
========================
Initialize the screenQue
========================
*/
VOID setupScreenQue() {
    for (UINTN i = 0; i < 4; i++) {

        for (UINTN j = 0; j < SPRITES_PER_LAYER; j++) {
            curSprites[i].ImgBuffer[j].Cframes = 0;
            curSprites[i].ImgBuffer[j].Img = NULL;
            curSprites[i].ImgBuffer[j].Nframes = 0;
            curSprites[i].ImgBuffer[j].X = 0;
            curSprites[i].ImgBuffer[j].Y = 0;
            curSprites[i].ImgBuffer[j].imageState = STALE;
            curSprites[i].ImgBuffer[j].isDrawn = FALSE;
        }

        curSprites[i].Layer = i;

        curLayers[i].Img = Realloc(NULL, 0, sizeof(ImageData));
        if (curLayers[i].Img != NULL) {
            // 2. Allocate the raw PIXEL DATA inside the struct
            curLayers[i].Img->Data = Realloc(NULL, 0, curScreen.ScreenWidth/8 * curScreen.ScreenHeight/8 * sizeof(UINT32));

            // 3. Initialize the struct properties
            curLayers[i].Img->Width = curScreen.ScreenWidth/8;
            curLayers[i].Img->Height = curScreen.ScreenHeight/8;
            curLayers[i].Img->isPixel = TRUE;  // Important for scaling logic!
            curLayers[i].Img->isAlpha = TRUE;

            // 4. Zero out the pixel buffer
            if (curLayers[i].Img->Data != NULL) {
                gBS->SetMem(curLayers[i].Img->Data, curScreen.ScreenWidth/8 * curScreen.ScreenHeight/8 * sizeof(UINT32), 0);
            }
        }
    }
}


/*
================================================
Request for an sprite patch from a specific layer
================================================
*/
patchImg* RequestPatchFromPool(zOrder order){
    for(UINTN i = 0; i < SPRITES_PER_LAYER; i++){
        patchImg *curImg = &curSprites[order].ImgBuffer[i];
        if(curImg->imageState == STALE && !curImg->isDrawn){
            return curImg;
        }
        if(curImg->imageState == PERMANENT && curImg->isDrawn){
            return curImg;
        }
    }
    return NULL;
}


/*
=======================
Draw the screen updates
=======================
*/
VOID DrawScreenUpdates(
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Graphics, 
    ImageData *DrawImg, 
    zOrder order, 
    UINTN screenX,
    UINTN screenY
){
    if(DrawImg == NULL) return;
    /* Image data will always be pixelated */
    /* push the image to our layer buffers before drawing */
    SaveIndirectImage(DrawImg, curLayers[order].Img, screenX/8, screenY/8);
    ImageData *SourceToBlt = DrawImg; // Default to drawing the original data
    if(DrawImg->isPixel){
        if(DrawImg->isAlpha){
            ExplodeAAwarePixelImage(
                DrawImg, 
                &patchBuffer, 
                order,
                FALSE,
                // Graphics,
                screenX,
                screenY
            );

        }
        else{
            ExplodeAUnAwarePixelImage(DrawImg,&patchBuffer);

        }
        SourceToBlt = &patchBuffer;
    }
    DrawDirectImage(Graphics,SourceToBlt,screenX,screenY);
}


/*
======================
Clear away the sprites
======================
*/
VOID ClearSpriteFromLayer(UINTN order, UINTN screenX, UINTN screenY, UINTN width, UINTN height) {
    if (order >= 4) return;
    UINTN gridX = screenX / 8;
    UINTN gridY = screenY / 8;
    
    UINTN bufferWidth = curScreen.ScreenWidth / 8; 

    // 3. Loop through the height of the sprite
    for (UINTN row = 0; row < height; row++) {
        
        // Calculate the starting index for THIS specific row
        UINTN offset = ((gridY + row) * bufferWidth) + gridX;

        // Zero out ONLY the width of the sprite for this row
        gBS->SetMem(
            &curLayers[order].Img->Data[offset], // Address of start of row
            width * sizeof(UINT32),        // Length (in bytes!)
            0                              // Value
        );
    }
}


/*
=======================
Restore the last sprite
=======================
*/
VOID RestoreScreenUpdates(EFI_GRAPHICS_OUTPUT_PROTOCOL *Graphics, ImageData *DrawImg, zOrder order, UINTN screenX,UINTN screenY){

    if(DrawImg == NULL) return;
    ClearSpriteFromLayer(order, screenX, screenY, DrawImg->Width, DrawImg->Height);
    ExplodeAAwarePixelImage(
        DrawImg, 
        &patchBuffer, 
        order,
        TRUE,
        screenX,
        screenY
    );
    DrawDirectImage(Graphics, &patchBuffer, screenX, screenY);
}


/*
======================
Dow the screen updates
======================
*/
VOID DoScreenUpdates(EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics){
    for(UINTN i = 0; i < 4; i++){

        for(UINTN j = 0; j < SPRITES_PER_LAYER; j++){

            patchImg *curImg = &curSprites[i].ImgBuffer[j];
            switch(curImg->imageState){
                /* Draw the image */
                case ACTIVE:{
                    DrawScreenUpdates(Graphics,curImg->Img,curSprites[i].Layer,curImg->X,curImg->Y);
                    curImg->imageState = STALE;
                    curImg->isDrawn = TRUE;
                    break;
                }
                /* Replace the image */
                case STALE:{
                    if(curImg->isDrawn){
                        RestoreScreenUpdates(Graphics,curImg->Img,curSprites[i].Layer,curImg->X,curImg->Y);
                        curImg->isDrawn = FALSE;
                    }
                    break;
                }
                /* Let the image Persist for a number of frames */
                case PERSIST:{
                    /* If the image was not drawn draw it*/
                    if(curImg->isDrawn){
                        curImg->Cframes++;
                        if(curImg->Cframes >= curImg->Nframes){
                            curImg->imageState = STALE;
                            curImg->Cframes = 0;
                            curImg->Nframes = 0;
                        }
                    }
                    /* Count the frames for which artifact must persist */
                    else{
                        DrawScreenUpdates(Graphics,curImg->Img,curSprites[i].Layer,curImg->X,curImg->Y);
                        curImg->isDrawn = TRUE;
                    }
                    break;
                }
                /* Permanent Image if it is drawn already do nothing */
                case PERMANENT:{
                    if(!curImg->isDrawn){
                        DrawScreenUpdates(Graphics,curImg->Img,curSprites[i].Layer,curImg->X,curImg->Y);
                        curImg->isDrawn = TRUE;
                    }
                    break;
                }
            }
        }
    }
}
