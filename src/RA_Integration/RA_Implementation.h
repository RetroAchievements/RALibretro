//  RetroAchievements Integration files:
//  Example implementation file: copy .h/.cpp to your emulator and modify functionality.
//  www.retroachievements.org

//  Return whether a game has been loaded. Should return FALSE if
//   no ROM is loaded, or a ROM has been unloaded.
extern bool GameIsActive();

//  Perform whatever action is required to unpause emulation.
extern void CauseUnpause();
//  Perform whatever action is required to Pause emulation.
extern void CausePause();

//  Perform whatever function in the case of needing to rebuild the menu.
extern void RebuildMenu();

//  sNameOut points to a 64 character buffer.
//  sNameOut should have copied into it the estimated game title 
//   for the ROM, if one can be inferred from the ROM.
extern void GetEstimatedGameTitle(char* sNameOut);

extern void ResetEmulation();

//  Called BY the toolset to tell the emulator to load a particular ROM.
extern void LoadROM(const char* sFullPath);

//  Installs these shared functions into the DLL
extern void RA_InitShared();


//extras
