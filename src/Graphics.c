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
    ImageData *Pimg,                // Foreground Sprite (Downscaled 8x8)
    ImageData *NPimg,               // Destination (Exploded 64x64)
    UINTN factor,
    zOrder order,                   // The Layer of the CURRENT sprite
    BOOLEAN skipCur,                // Skip the current layer (Eraser mode)
    EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics,
    UINTN screenX, UINTN screenY
) {
    // 1. Set dimensions for the Destination
    NPimg->Height = Pimg->Height * factor;
    NPimg->Width = Pimg->Width * factor;

    // --- PHASE 1: FETCH BASE BACKGROUND (LAYER 0) ---
    if (curLayers[0].Img != NULL) {
        
        UINTN patchTotalSize = patchBuffer.Width * patchBuffer.Height; 
        UINTN spriteSmallSize = Pimg->Width * Pimg->Height;            
        
        UINT32* safeMemoryZone = &patchBuffer.Data[patchTotalSize - spriteSmallSize];

        ImageData bgTmp = {
            .Width = Pimg->Width,     
            .Height = Pimg->Height,   
            .Data = safeMemoryZone,   
            .isPixel = FALSE,         // <--- FIX IS HERE (Was TRUE)
            .isAlpha = FALSE
        };

        // Capture 8x8 from Background Layer into 'safeMemoryZone'
        // Since isPixel is FALSE, SaveIndirectImage treats it as a raw, packed buffer.
        SaveIndirectImage(curLayers[0].Img, &bgTmp, screenX, screenY);

        // Explode from 'safeMemoryZone' (8x8) into 'NPimg->Data' (64x64)
        ExplodeAUnAwarePixelImage(&bgTmp, NPimg, factor);
    } 
    else {
        // Fallback: Raw screen capture
        SaveDirectImage(Graphics, NPimg, screenX, screenY);
    }

    // --- PHASE 2: COMPOSITE INTERMEDIATE LAYERS ---
    // Loop from Layer 1 up to the layer *below* our current sprite
    for (UINTN layer = 1; layer < order; layer++) {
        
        if (curLayers[layer].Img == NULL) continue;

        UINTN gridX = screenX / 8;
        UINTN gridY = screenY / 8;

        for (UINTN i = 0; i < Pimg->Height; i++) {
            for (UINTN j = 0; j < Pimg->Width; j++) {
                
                // Fetch pixel from 160x96 buffer
                UINT32 layerPixel = curLayers[layer].Img->Data[(gridY + i) * 160 + (gridX + j)];

                // If this layer has a visible pixel here, it covers the background
                if (BinaryAlpha(layerPixel)) {
                    for (UINTN v = 0; v < factor; v++) {
                        UINTN destIdxBase = ((i * factor + v) * NPimg->Width) + (j * factor);
                        for (UINTN h = 0; h < factor; h++) {
                            NPimg->Data[destIdxBase + h] = layerPixel;
                        }
                    }
                }
            }
        }
    }

    // If we are just erasing (restoring background), stop here.
    if (skipCur) {
        NPimg->isPixel = FALSE; 
        return;
    }

    // --- PHASE 3: DRAW CURRENT SPRITE (STENCIL) ---
    for (UINTN i = 0; i < Pimg->Height; i++) {
        for (UINTN j = 0; j < Pimg->Width; j++) {
            UINT32 fgPixel = Pimg->Data[i * Pimg->Width + j];

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



VOID SaveIndirectImage(ImageData *saveImg, ImageData *bufferImg, UINTN imgX, UINTN imgY)
{
    if (saveImg == NULL || bufferImg == NULL || saveImg->Data == NULL || bufferImg->Data == NULL) {
        return;
    }
    if (saveImg->Width == 0 || saveImg->Height == 0 || bufferImg->Width == 0 || bufferImg->Height == 0) {
        return;
    }

    // Factors are ONLY used to calculate the starting Offset (X/Y), not the width of data to copy
    UINTN srcFactor = saveImg->isPixel ? 8 : 1;
    UINTN dstFactor = bufferImg->isPixel ? 8 : 1;

    const UINTN bytes_per_pixel = sizeof(UINT32);

    // Case A: saveImg is the Large Background (capture a patch into bufferImg)
    if (saveImg->Width >= bufferImg->Width && saveImg->Height >= bufferImg->Height) {

        UINTN srcX = imgX / srcFactor;
        UINTN srcY = imgY / srcFactor;

        // Use the buffer's ACTUAL width, do not divide by factor again
        UINTN copyWidth  = bufferImg->Width;
        UINTN copyHeight = bufferImg->Height;

        // Clip against source boundaries
        if (srcX >= saveImg->Width || srcY >= saveImg->Height) {
            goto DONE;
        }

        if (srcX + copyWidth > saveImg->Width) {
            copyWidth = saveImg->Width - srcX;
        }
        if (srcY + copyHeight > saveImg->Height) {
            copyHeight = saveImg->Height - srcY;
        }

        if (copyWidth == 0 || copyHeight == 0) {
            goto DONE;
        }

        for (UINTN row = 0; row < copyHeight; row++) {
            UINTN srcOffset = ((srcY + row) * saveImg->Width) + srcX;
            UINTN dstOffset = row * bufferImg->Width; 

            // Copy the memory. copyWidth is now the correct number of pixels (e.g., 160).
            gBS->CopyMem(
                &bufferImg->Data[dstOffset],
                &saveImg->Data[srcOffset],
                copyWidth * bytes_per_pixel
            );
        }
    }
    // Case B: bufferImg is the Large Canvas (paste saveImg into bufferImg)
    else {

        UINTN dstX = imgX / dstFactor;
        UINTN dstY = imgY / dstFactor;

        // Use the source's ACTUAL width
        UINTN copyWidth  = saveImg->Width;
        UINTN copyHeight = saveImg->Height;

        // Clip against destination boundaries
        if (dstX >= bufferImg->Width || dstY >= bufferImg->Height) {
            goto DONE;
        }

        if (dstX + copyWidth > bufferImg->Width) {
            copyWidth = bufferImg->Width - dstX;
        }
        if (dstY + copyHeight > bufferImg->Height) {
            copyHeight = bufferImg->Height - dstY;
        }

        if (copyWidth == 0 || copyHeight == 0) {
            goto DONE;
        }

        for (UINTN row = 0; row < copyHeight; row++) {
            UINTN srcOffset = row * saveImg->Width; 
            UINTN dstOffset = ((dstY + row) * bufferImg->Width) + dstX;

            gBS->CopyMem(
                &bufferImg->Data[dstOffset],
                &saveImg->Data[srcOffset],
                copyWidth * bytes_per_pixel
            );
        }
    }

DONE:
    // Preserve pixel/alpha mode from source image
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

        if (i < 3) {
            curSprites[i].nextImg = (SpriteLayers*)&curSprites[i + 1];
        } else {
            curSprites[i].nextImg = NULL;
        }

        if (i > 0) {
            curSprites[i].prevImg = &curSprites[i - 1];
        } else {
            curSprites[i].prevImg = NULL;
        }
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
        curLayers[i].layer = i;
    }
}


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


VOID DrawScreenUpdates(EFI_GRAPHICS_OUTPUT_PROTOCOL *Graphics, ImageData *DrawImg, zOrder order, UINTN screenX,UINTN screenY){
    if(DrawImg == NULL) return;
    /* Image data will always be pixelated */
    /* push the image to our layer buffers before drawing */
    SaveIndirectImage(DrawImg, curLayers[order].Img, screenX, screenY);
    ImageData *SourceToBlt = DrawImg; // Default to drawing the original data
    if(DrawImg->isPixel){
        if(DrawImg->isAlpha){
            ExplodeAAwarePixelImage(
                DrawImg, 
                &patchBuffer, 
                8,
                order,
                FALSE,
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

VOID ClearSpriteFromLayer(UINTN order, UINTN screenX, UINTN screenY, UINTN width, UINTN height) {
    // 1. Convert screen coords to Grid coords (160x96 space)
    if (order >= 4) return;
    UINTN gridX = screenX / 8;
    UINTN gridY = screenY / 8;
    
    // 2. Get the width of the collision buffer (160)
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

VOID RestoreScreenUpdates(EFI_GRAPHICS_OUTPUT_PROTOCOL *Graphics, ImageData *DrawImg, zOrder order, UINTN screenX,UINTN screenY){

    if(DrawImg == NULL) return;
    ClearSpriteFromLayer(order, screenX, screenY, DrawImg->Width, DrawImg->Height);
    ExplodeAAwarePixelImage(
        DrawImg, 
        &patchBuffer, 
        8,
        order,
        TRUE,
        Graphics,
        screenX,
        screenY
    );
    DrawDirectImage(Graphics, &patchBuffer, screenX, screenY);
}

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
