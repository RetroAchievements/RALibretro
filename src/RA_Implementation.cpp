#include "RA_Interface.h"

#include <windows.h>

#include "Git.h"

extern HWND g_mainWindow;
int g_activeGame;

bool isGameActive();
void getGameName(char name[], size_t len);
void pause();
void resume();
void reset();
void loadROM(const char* path);


// returns -1 if not found
int GetMenuItemIndex(HMENU hMenu, const char* ItemName)
{
  int index = 0;
  char buf[256];

  while (index < GetMenuItemCount(hMenu))
  {
    if (GetMenuString(hMenu, index, buf, sizeof(buf) - 1, MF_BYPOSITION) && !strcmp(ItemName, buf))
    {
      return index;
    }

    index++;
  }

  return -1;
}


//  Return whether a game has been loaded. Should return FALSE if
//   no ROM is loaded, or a ROM has been unloaded.
bool GameIsActive()
{
  return isGameActive();
}


void CauseUnpause()
{
  resume();
}


//  Perform whatever action is required to Pause emulation.
void CausePause()
{
  pause();
}


//  Perform whatever function in the case of needing to rebuild the menu.
void RebuildMenu()
{
  //Meka uses Allegro window (Not Win32)
  //So going to use the Console window from message.c to attach the RA Menu to because using SetMenu distorts the window
  //We actually need to create the entire menu in the first place to do this (Console doesn't have one)

  HMENU mainMenu = GetMenu(g_mainWindow);
  if (!mainMenu) return;
  
  // get file menu index
  int index = GetMenuItemIndex(mainMenu, "&RetroAchievements");
  if (index >= 0)
    DeleteMenu(mainMenu, index, MF_BYPOSITION);

  //  ##RA embed RA
  AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)RA_CreatePopupMenu(), TEXT("&RetroAchievements"));
  InvalidateRect(g_mainWindow, NULL, TRUE);
  DrawMenuBar(g_mainWindow);
}


//  sNameOut points to a 64 character buffer.
//  sNameOut should have copied into it the estimated game title 
//   for the ROM, if one can be inferred from the ROM.
void GetEstimatedGameTitle(char* sNameOut)
{
    getGameName(sNameOut, 64);
}


void ResetEmulation()
{
  reset();
}


void LoadROM(const char* sFullPath)
{
  loadROM(sFullPath);
}

void RA_Init(HWND hWnd)
{
  // initialize the DLL
  RA_Init(hWnd, RA_Libretro, git::getReleaseVersion());

  // provide callbacks to the DLL
  RA_InstallSharedFunctions(NULL, &CauseUnpause, &CausePause, &RebuildMenu, &GetEstimatedGameTitle, &ResetEmulation, &LoadROM);

  // add a placeholder menu item and start the login process - menu will be updated when login completes
  RebuildMenu();
  RA_AttemptLogin(false);

  // ensure titlebar text matches expected format
  RA_UpdateAppTitle("");
}

void RA_ActivateDisc(unsigned char* pExe, size_t nExeSize)
{
  int loadedGame = RA_IdentifyRom(pExe, nExeSize);
  if (loadedGame != g_activeGame)
  {
    g_activeGame = loadedGame;
    RA_ActivateGame(loadedGame);
  }
}

void RA_DeactivateDisc()
{
  g_activeGame = 0;
}
