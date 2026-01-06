#include "efibind.h"
#include "efidef.h"
#include <efi.h>

extern SIMPLE_TEXT_OUTPUT_INTERFACE *cout;
extern SIMPLE_INPUT_INTERFACE *cin;
extern SIMPLE_TEXT_OUTPUT_INTERFACE *cerr;  // Stderr can be set to a serial output or other non-display device.
extern EFI_SYSTEM_TABLE *gST;
extern EFI_BOOT_SERVICES *gBS;
extern EFI_RUNTIME_SERVICES *gRS;
extern EFI_HANDLE image;

typedef struct{
    UINTN Width;
    UINTN Height;
    UINT32* Data;
}ImageData;

typedef struct{
    UINTN ScreenWidth;
    UINTN ScreenHeight;
    BOOLEAN redraw;
    BOOLEAN isRunning;
}Screen;

typedef struct{
    INTN X;
    INTN Y;
    BOOLEAN Lclicked;
    BOOLEAN Rclicked;
    BOOLEAN isMoved;
}Cursor;

typedef struct{
    Cursor cursor;
    ImageData Over;
    ImageData normalimg;
    ImageData clickedimg;
}Mouse;

typedef enum{
    ACTIVE,     /* Draw Once Forget*/
    STALE,      /* Drawn to be replaced */
    PERSIST,    /* Already Drawn but dont replace yet*/
    PERMANENT,  /* Drawn Never to replace till a full frame redraw*/
}patchState;

typedef enum{
    FFAWARE,    /* Paint the full frame alpha aware*/
    FFUAWARE,   /* Pain the full frame alpha unaware*/
    PFAWARE,    /* Paint Part of the frame alpha aware */
    PFUAWARE,   /* Paint Part of the frame alpha unaware */
}drawType;

typedef struct{
    ImageData *replacedImg;
    ImageData *Img;
    UINTN X;
    UINTN Y;
    patchState imageState;
    UINTN Nframes;
    UINTN Cframes;
    drawType imgtype;
    BOOLEAN isDrawn;
}patchImg;

typedef enum{
    LAUNCH,
    MENU,
    PLAYING,
    PAUSED
}GameState;

typedef struct{
    GameState state;

}Game;

extern Screen curScreen;

VOID* Realloc(void* Oldptr, UINTN Oldsize, UINTN Newsize);
CHAR16* InttoString(INTN val);
VOID connect_all_controllers();
void Globalize(EFI_HANDLE handle, EFI_SYSTEM_TABLE *systable);
EFI_SIMPLE_POINTER_PROTOCOL* InitMouse();
EFI_GRAPHICS_OUTPUT_PROTOCOL* InitGraphics(UINTN targetWidth, UINTN targetHeight);
VOID DrawAlphaAwareImage(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, ImageData *img, UINT32 x, UINT32 y);
VOID DoCursor(EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics,Mouse* cur, Cursor* curCurs);
VOID ScreenUpdate();
VOID SaveDirectImage(EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics,ImageData* img, UINTN screenX, UINTN screenY);
VOID DrawDirectImage(EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics, ImageData* img, UINTN imgX, UINTN imgY);
VOID GetMouseUpdates(EFI_SIMPLE_POINTER_PROTOCOL* Mouse, Cursor* cursPos);
VOID setupScreenQue(patchImg** pQue);
VOID SaveIndirectImage(ImageData *saveImg, ImageData *bufferImg, UINTN imgX, UINTN imgY);
patchImg* RequestPatchFromPool(patchImg *pQue);
VOID DoScreenUpdates(EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics, patchImg *pQue);
