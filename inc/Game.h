#include "efi.h"

typedef enum{
    LAUNCH,
    MENU,
    PLAYING,
    PAUSED
}GameState;

typedef struct{
    GameState state;

}Game;

VOID Launch();
VOID Menu();
VOID Playing();
VOID Paused();
