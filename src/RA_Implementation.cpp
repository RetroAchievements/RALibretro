#include "RA_Integration/RA_Implementation/RA_Implementation.h"
#include "RA_Interface.h"
#include <windows.h>


extern HWND g_mainWindow;

bool isGameActive();
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
}


void ResetEmulation()
{
  reset();
}


void LoadROM(const char* sFullPath)
{
  loadROM(sFullPath);
}


//  Installs these shared functions into the DLL
void RA_InitShared()
{
  RA_InstallSharedFunctions(&GameIsActive, &CauseUnpause, &CausePause, &RebuildMenu, &GetEstimatedGameTitle, &ResetEmulation, &LoadROM);
}
