#include "efibind.h"
#include "efidef.h"
#include <efi.h>

#define SPRITES_PER_LAYER 4

#define LOGICAL_SCREEN_W 160
#define LOGICAL_SCREEN_H 96
typedef struct{
    UINTN ScreenWidth;
    UINTN ScreenHeight;
    BOOLEAN redraw;
    BOOLEAN isRunning;
}Screen;

typedef struct{
    UINTN Width;        // Image Width
    UINTN Height;       // Image Height
    UINT32* Data;       // Image Data
    BOOLEAN isPixel;    // Is the Image Pixelated or not
    BOOLEAN isAlpha;
}ImageData;

typedef struct{
    INTN X;
    INTN Y;
    BOOLEAN Lclicked;
    BOOLEAN Rclicked;
    BOOLEAN reDraw;
}Cursor;

typedef struct{
    Cursor cursor;
    ImageData normalimg;
    ImageData clickedimg;
}Mouse;

typedef enum{
    ACTIVE,     // Draw Once Forget
    STALE,      // Drawn to be replaced
    PERSIST,    // Already Drawn but dont replace yet
    PERMANENT,  // Drawn Never to replace till a full frame redraw
}patchState;

typedef enum{
    FIRST = 0,
    SECOND = 1,
    THIRD = 2,
    FOUTH = 3,
}zOrder;

typedef struct{
    ImageData *Img;         // The image to draw
    UINTN X;                // X coords to draw at
    UINTN Y;                // Y coords to draw at
    patchState imageState;  // How long the image should exist
    UINTN Nframes;          // Number of frames the image must PERSIST for
    UINTN Cframes;          // Current Number of frames the image has persisted for
    BOOLEAN isDrawn;        // If the image has been drawn or not
}patchImg;

typedef struct SpriteLayers SpriteLayers;

struct SpriteLayers{
    patchImg ImgBuffer[SPRITES_PER_LAYER];
    SpriteLayers *nextImg;
    SpriteLayers *prevImg;
    zOrder Layer;
};

typedef struct{
    ImageData* Img;
    zOrder layer;
}zBuffers;

extern Screen curScreen;
extern ImageData patchBuffer;
extern SpriteLayers curSprites[4];
extern zBuffers curLayers[4];
VOID DrawAAwarePixelImage(
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, 
    ImageData *img, 
    UINT32 x, 
    UINT32 y
);

VOID DrawAUnAwarePixelImage(
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, 
    ImageData *img, 
    UINT32 x, 
    UINT32 y
);

VOID DoCursor(
    EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics,
    Mouse* cur, 
    Cursor* curCurs
);

VOID ScreenUpdate();

VOID SaveDirectImage(
    EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics,
    ImageData* img, 
    UINTN screenX, 
    UINTN screenY
);

VOID DrawDirectImage(
    EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics, 
    ImageData* img, 
    UINTN imgX, 
    UINTN imgY
);

VOID GetMouseUpdates(
    EFI_SIMPLE_POINTER_PROTOCOL* Mouse, 
    Cursor* cursPos
);

VOID setupScreenQue(
);

VOID SaveIndirectImage(
    ImageData *saveImg, 
    ImageData *bufferImg, 
    UINTN imgX, 
    UINTN imgY
);

patchImg* RequestPatchFromPool(
    zOrder order
);

VOID DoScreenUpdates(
    EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics
);

VOID DrawScreenUpdates(
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Graphics, 
    ImageData *DrawImg, 
    zOrder Layer,
    UINTN screenX,
    UINTN screenY
);

VOID ExplodeAAwarePixelImage(
    ImageData *Pimg,                // Foreground (Downscaled)
    ImageData *NPimg,               // Scratchpad (Exploded)
    UINTN factor,
    zOrder order,
    BOOLEAN skipCur,
    EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics,
    UINTN screenX, UINTN screenY    
);

VOID ExplodeAUnAwarePixelImage(
    ImageData *Pimg, 
    ImageData *NPimg, 
    UINTN factor
);

VOID RestoreScreenUpdates(EFI_GRAPHICS_OUTPUT_PROTOCOL *Graphics, ImageData *DrawImg, zOrder order, UINTN screenX,UINTN screenY);
