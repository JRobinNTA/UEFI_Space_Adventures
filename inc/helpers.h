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
    UINTN X;
    UINTN Y;
    BOOLEAN moved;
    BOOLEAN Lclicked;
    BOOLEAN Rclicked;
    ImageData Over;
    ImageData normalimg;
    ImageData clickedimg;
}Cursor;

extern Screen curScreen;

VOID* Realloc(void* Oldptr, UINTN Oldsize, UINTN Newsize);
CHAR16* InttoString(INTN val);
VOID connect_all_controllers();
void Globalize(EFI_HANDLE handle, EFI_SYSTEM_TABLE *systable);
EFI_SIMPLE_POINTER_PROTOCOL* InitMouse();
EFI_GRAPHICS_OUTPUT_PROTOCOL* InitGraphics(UINTN targetWidth, UINTN targetHeight);
VOID DrawAlphaAwareImage(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, const ImageData *img, UINT32 x, UINT32 y, BOOLEAN Alpha);
VOID DoCursor(EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics,Cursor* cur, INTN curX, INTN curY);
VOID ScreenUpdate();
VOID SaveDirectImage(EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics,ImageData* img, UINTN screenX, UINTN screenY);
VOID DrawDirectImage(EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics, ImageData* img, UINTN imgX, UINTN imgY);
