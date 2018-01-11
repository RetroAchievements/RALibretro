#include "RA_Interface.h"

#include <winhttp.h>



//	Exposed, shared
//	App-level:
bool	(CCONV *_RA_GameIsActive) (void) = NULL;
void	(CCONV *_RA_CauseUnpause) (void) = NULL;
void	(CCONV *_RA_CausePause) (void) = NULL;
void	(CCONV *_RA_RebuildMenu) (void) = NULL;
void	(CCONV *_RA_ResetEmulation) (void) = NULL;
void	(CCONV *_RA_GetEstimatedGameTitle) (char* sNameOut) = NULL;
void	(CCONV *_RA_LoadROM) (const char* sNameOut) = NULL;


bool RA_GameIsActive()
{
	if( _RA_GameIsActive != NULL )
		return _RA_GameIsActive();
	return false;
}

void RA_CauseUnpause()
{
	if( _RA_CauseUnpause != NULL )
		_RA_CauseUnpause();
}

void RA_CausePause()
{
	if (_RA_CausePause != NULL)
		_RA_CausePause();
}

void RA_RebuildMenu()
{
	if( _RA_RebuildMenu != NULL )
		_RA_RebuildMenu();
}

void RA_ResetEmulation()
{
	if( _RA_ResetEmulation != NULL )
		_RA_ResetEmulation();
}

void RA_LoadROM( const char* sFullPath )
{
	if( _RA_LoadROM != NULL )
		_RA_LoadROM( sFullPath );
}

void RA_GetEstimatedGameTitle( char* sNameOut )
{
	if( _RA_GetEstimatedGameTitle != NULL )
		_RA_GetEstimatedGameTitle( sNameOut );
}


#ifndef RA_EXPORTS

//Note: this is ALL public facing! :S tbd tidy up this bit

//	Expose to app:
 
//	Generic:
const char* (CCONV *_RA_IntegrationVersion) () = nullptr;
int		(CCONV *_RA_InitI) ( HWND hMainWnd, int nConsoleID, const char* sClientVer ) = nullptr;
int		(CCONV *_RA_Shutdown) () = nullptr;
//	Load/Save
bool	(CCONV *_RA_ConfirmLoadNewRom)( bool bQuitting ) = nullptr;
int		(CCONV *_RA_OnLoadNewRom)( const BYTE* pROM, unsigned int nROMSize ) = nullptr;
void	(CCONV *_RA_InstallMemoryBank)( int nBankID, void* pReader, void* pWriter, int nBankSize ) = nullptr;
void	(CCONV *_RA_ClearMemoryBanks)() = nullptr;
void	(CCONV *_RA_OnLoadState)( const char* sFilename ) = nullptr;
void	(CCONV *_RA_OnSaveState)( const char* sFilename ) = nullptr;
//	Achievements:
void	(CCONV *_RA_DoAchievementsFrame)() = nullptr;
//	User:
bool	(CCONV *_RA_UserLoggedIn)() = nullptr;
const char*	(CCONV *_RA_Username)() = nullptr;
void	(CCONV *_RA_AttemptLogin)( bool bBlocking ) = nullptr;
//	Graphics:
void	(CCONV *_RA_InitDirectX) (void) = nullptr;
void	(CCONV *_RA_OnPaint)(HWND hWnd) = nullptr;
//	Tools:
void	(CCONV *_RA_SetPaused)(bool bIsPaused) = nullptr;
HMENU	(CCONV *_RA_CreatePopupMenu)() = nullptr;
void	(CCONV *_RA_UpdateAppTitle) (const char* pMessage) = nullptr;
void	(CCONV *_RA_HandleHTTPResults) (void) = nullptr;
void	(CCONV *_RA_InvokeDialog)(LPARAM nID) = nullptr;
void	(CCONV *_RA_InstallSharedFunctions)( bool(*)(), void(*)(), void(*)(), void(*)(), void(*)(char*), void(*)(), void(*)(const char*) ) = nullptr;
int		(CCONV *_RA_SetConsoleID)(unsigned int nConsoleID) = nullptr;
int		(CCONV *_RA_HardcoreModeIsActive)(void) = nullptr;
void	(CCONV *_RA_EnableHardcoreMode) (void) = nullptr;
void	(CCONV *_RA_DisableHardcoreMode) (void) = nullptr;
int		(CCONV *_RA_HTTPGetRequestExists)(const char* sPageName) = nullptr;

//	Don't expose to app
HINSTANCE g_hRADLL = nullptr;


int		(CCONV *_RA_UpdateOverlay) (ControllerInput* pInput, float fDeltaTime, bool Full_Screen, bool Paused) = nullptr;
int		(CCONV *_RA_UpdatePopups) (ControllerInput* pInput, float fDeltaTime, bool Full_Screen, bool Paused) = nullptr;
void	(CCONV *_RA_RenderOverlay) (HDC hDC, RECT* prcSize) = nullptr;
void	(CCONV *_RA_RenderPopups) (HDC hDC, RECT* prcSize) = nullptr;


//	Helpers:
bool RA_UserLoggedIn()
{
	if( _RA_UserLoggedIn != nullptr )
		return _RA_UserLoggedIn();

	return false;
}

const char* RA_Username()
{
	return _RA_Username ? _RA_Username() : "";
}

void RA_AttemptLogin( bool bBlocking )
{
	if( _RA_AttemptLogin != nullptr )
		_RA_AttemptLogin( bBlocking );
}

void RA_UpdateRenderOverlay( HDC hDC, ControllerInput* pInput, float fDeltaTime, RECT* prcSize, bool Full_Screen, bool Paused )
{
	if( _RA_UpdatePopups != nullptr )
		_RA_UpdatePopups( pInput, fDeltaTime, Full_Screen, Paused );

	if( _RA_RenderPopups != nullptr )
		_RA_RenderPopups( hDC, prcSize );

	//	NB. Render overlay second, on top of popups!
	if( _RA_UpdateOverlay != nullptr )
		_RA_UpdateOverlay( pInput, fDeltaTime, Full_Screen, Paused );

	if( _RA_RenderOverlay != nullptr )
		_RA_RenderOverlay( hDC, prcSize );
}

void RA_OnLoadNewRom( BYTE* pROMData, unsigned int nROMSize )
{
	if( _RA_OnLoadNewRom != nullptr )
		_RA_OnLoadNewRom( pROMData, nROMSize );
}

void RA_ClearMemoryBanks()
{
	if( _RA_ClearMemoryBanks != nullptr )
		_RA_ClearMemoryBanks();
}

void RA_InstallMemoryBank( int nBankID, void* pReader, void* pWriter, int nBankSize )
{
	if( _RA_InstallMemoryBank != nullptr )
		_RA_InstallMemoryBank( nBankID, pReader, pWriter, nBankSize );
}

HMENU RA_CreatePopupMenu()
{
	return ( _RA_CreatePopupMenu != nullptr ) ? _RA_CreatePopupMenu() : nullptr;
}

void RA_UpdateAppTitle( const char* sCustomMsg )
{
	if( _RA_UpdateAppTitle != nullptr )
		_RA_UpdateAppTitle( sCustomMsg );
}

void RA_HandleHTTPResults()
{
	if( _RA_HandleHTTPResults != nullptr )
		_RA_HandleHTTPResults();
}

bool RA_ConfirmLoadNewRom( bool bIsQuitting )
{
	return _RA_ConfirmLoadNewRom ? _RA_ConfirmLoadNewRom( bIsQuitting ) : true;
}

void RA_InitDirectX()
{
	if( _RA_InitDirectX != nullptr )
		_RA_InitDirectX();
}

void RA_OnPaint( HWND hWnd )
{
	if( _RA_OnPaint != nullptr )
		_RA_OnPaint( hWnd );
}

void RA_InvokeDialog( LPARAM nID )
{
	if( _RA_InvokeDialog != nullptr )
		_RA_InvokeDialog( nID );
}

void RA_SetPaused( bool bIsPaused )
{
	if( _RA_SetPaused != nullptr )
		_RA_SetPaused( bIsPaused );
}

void RA_OnLoadState( const char* sFilename )
{
	if( _RA_OnLoadState != nullptr )
		_RA_OnLoadState( sFilename );
}

void RA_OnSaveState( const char* sFilename )
{
	if( _RA_OnSaveState != nullptr )
		_RA_OnSaveState( sFilename );
}

void RA_DoAchievementsFrame()
{
	if( _RA_DoAchievementsFrame != nullptr )
		_RA_DoAchievementsFrame();
}

void RA_SetConsoleID( unsigned int nConsoleID )
{
	if( _RA_SetConsoleID != nullptr )
		_RA_SetConsoleID( nConsoleID );
}

int RA_HardcoreModeIsActive()
{
	return ( _RA_HardcoreModeIsActive != nullptr ) ? _RA_HardcoreModeIsActive() : 0;
}

void RA_EnableHardcoreMode() // Note this will reset the emulator on state change
{
	if (_RA_EnableHardcoreMode != nullptr) {
		_RA_EnableHardcoreMode();
	}
}

void RA_DisableHardcoreMode()
{
	if (_RA_DisableHardcoreMode != nullptr) {
		_RA_DisableHardcoreMode();
	}
}

int RA_HTTPRequestExists( const char* sPageName )
{
	return ( _RA_HTTPGetRequestExists != nullptr ) ? _RA_HTTPGetRequestExists( sPageName ) : 0;
}


BOOL DoBlockingHttpGet( const char* sRequestedPage, char* pBufferOut, const unsigned int /*nBufferOutSize*/, DWORD* pBytesRead )
{
	BOOL bResults = FALSE, bSuccess = FALSE;
	HINTERNET hSession = nullptr, hConnect = nullptr, hRequest = nullptr;

	WCHAR wBuffer[1024];
	size_t nTemp;
	char* sDataDestOffset = &pBufferOut[0];
	DWORD nBytesToRead = 0;
	DWORD nBytesFetched = 0;

	char sClientName[1024];
	snprintf( sClientName, 1024, "Retro Achievements Client" );
	WCHAR wClientNameBuffer[1024];
	//mbstowcs_s( &nTemp, wClientNameBuffer, 1024, sClientName, strlen(sClientName)+1 );
	mbstowcs( wClientNameBuffer, sClientName, strlen(sClientName)+1 );

	// Use WinHttpOpen to obtain a session handle.
	hSession = WinHttpOpen( wClientNameBuffer, 
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME, 
		WINHTTP_NO_PROXY_BYPASS, 0);

	// Specify an HTTP server.
	if( hSession != nullptr )
	{
		hConnect = WinHttpConnect( hSession, L"www.retroachievements.org", INTERNET_DEFAULT_HTTP_PORT, 0);

		// Create an HTTP Request handle.
		if( hConnect != nullptr )
		{
			//mbstowcs_s( &nTemp, wBuffer, 1024, sRequestedPage, strlen(sRequestedPage)+1 );
			mbstowcs( wBuffer, sRequestedPage, strlen(sRequestedPage)+1 );

			hRequest = WinHttpOpenRequest( hConnect, 
				L"GET", 
				wBuffer, 
				NULL, 
				WINHTTP_NO_REFERER, 
				WINHTTP_DEFAULT_ACCEPT_TYPES,
				0);

			// Send a Request.
			if( hRequest != nullptr )
			{
				bResults = WinHttpSendRequest( hRequest, 
					L"Content-Type: application/x-www-form-urlencoded",
					0, 
					WINHTTP_NO_REQUEST_DATA,
					0, 
					0,
					0);

				if( WinHttpReceiveResponse( hRequest, NULL ) )
				{
					nBytesToRead = 0;
					(*pBytesRead) = 0;
					WinHttpQueryDataAvailable( hRequest, &nBytesToRead );

					while( nBytesToRead > 0 )
					{
						char sHttpReadData[8192];
						ZeroMemory( sHttpReadData, 8192*sizeof(char) );

						assert( nBytesToRead <= 8192 );
						if( nBytesToRead <= 8192 )
						{
							nBytesFetched = 0;
							if( WinHttpReadData( hRequest, &sHttpReadData, nBytesToRead, &nBytesFetched ) )
							{
								assert( nBytesToRead == nBytesFetched );

								//Read: parse buffer
								memcpy( sDataDestOffset, sHttpReadData, nBytesFetched );

								sDataDestOffset += nBytesFetched;
								(*pBytesRead) += nBytesFetched;
							}
						}

						bSuccess = TRUE;

						WinHttpQueryDataAvailable( hRequest, &nBytesToRead );
					}
				}
			}
		}
	}


	// Close open handles.
	if (hRequest) WinHttpCloseHandle(hRequest);
	if (hConnect) WinHttpCloseHandle(hConnect);
	if (hSession) WinHttpCloseHandle(hSession);

	return bSuccess;
}

void WriteBufferToFile( const char* sFile, const char* sBuffer, int nBytes )
{
	FILE* pf = fopen( sFile, "wb" );
	if( pf != nullptr )
	{
		fwrite( sBuffer, 1, nBytes, pf );
		fclose( pf );
	}
	else
	{
		MessageBoxA( nullptr, "Problems writing file!", sFile, MB_OK );
	}
}

void FetchIntegrationFromWeb()
{
	const int MAX_SIZE = 2 * 1024 * 1024;
	char* buffer = new char[ MAX_SIZE ];
	if( buffer != nullptr )
	{
		DWORD nBytesRead = 0;
		if( DoBlockingHttpGet( "bin/RA_Integration.dll", buffer, MAX_SIZE, &nBytesRead ) )
			WriteBufferToFile( "RA_Integration.dll", buffer, nBytesRead );
 
		delete[] ( buffer );
		buffer = nullptr;
	}
}

//Returns the last Win32 error, in string format. Returns an empty string if there is no error.
std::string GetLastErrorAsString()
{
    //Get the error message, if any.
    DWORD errorMessageID = ::GetLastError();
    if(errorMessageID == 0)
        return "No error message has been recorded";

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    std::string message(messageBuffer, size);

    //Free the buffer.
    LocalFree(messageBuffer);

    return message;
}

const char* CCONV _RA_InstallIntegration()
{
	SetErrorMode( 0 );
	
#ifdef _DEBUG
	g_hRADLL = LoadLibraryEx( TEXT( "RA_Integration_d.dll" ), nullptr, 0 );
#else
	g_hRADLL = LoadLibrary( TEXT( "RA_Integration.dll" ) );
#endif
	if( g_hRADLL == NULL )
	{
		char buffer[ 1024 ];
		snprintf( buffer, 1024, "LoadLibrary failed: %d : %s\n", ::GetLastError(), GetLastErrorAsString().c_str() );
		MessageBoxA( nullptr, buffer, "Sorry!", MB_OK );

		return "0.000";
	}

	//	Install function pointers one by one
 
	_RA_IntegrationVersion	= (const char*(CCONV *)())								GetProcAddress( g_hRADLL, "_RA_IntegrationVersion" );
	_RA_InitI				= (int(CCONV *)(HWND, int, const char*))				GetProcAddress( g_hRADLL, "_RA_InitI" );
	_RA_Shutdown			= (int(CCONV *)())										GetProcAddress( g_hRADLL, "_RA_Shutdown" );
	_RA_UserLoggedIn		= (bool(CCONV *)())										GetProcAddress( g_hRADLL, "_RA_UserLoggedIn" );
	_RA_Username			= (const char*(CCONV *)())								GetProcAddress( g_hRADLL, "_RA_Username" );
	_RA_AttemptLogin		= (void(CCONV *)(bool))									GetProcAddress( g_hRADLL, "_RA_AttemptLogin" );
	_RA_UpdateOverlay		= (int(CCONV *)(ControllerInput*, float, bool, bool))	GetProcAddress( g_hRADLL, "_RA_UpdateOverlay" );
	_RA_UpdatePopups		= (int(CCONV *)(ControllerInput*, float, bool, bool))	GetProcAddress( g_hRADLL, "_RA_UpdatePopups" );
	_RA_RenderOverlay		= (void(CCONV *)(HDC, RECT*))							GetProcAddress( g_hRADLL, "_RA_RenderOverlay" );
	_RA_RenderPopups		= (void(CCONV *)(HDC, RECT*))							GetProcAddress( g_hRADLL, "_RA_RenderPopups" );
	_RA_OnLoadNewRom		= (int(CCONV *)(const BYTE*, unsigned int))				GetProcAddress( g_hRADLL, "_RA_OnLoadNewRom" );
	_RA_InstallMemoryBank	= (void(CCONV *)(int, void*, void*, int))				GetProcAddress( g_hRADLL, "_RA_InstallMemoryBank" );
	_RA_ClearMemoryBanks	= (void(CCONV *)())										GetProcAddress( g_hRADLL, "_RA_ClearMemoryBanks" );
	_RA_UpdateAppTitle		= (void(CCONV *)(const char*))							GetProcAddress( g_hRADLL, "_RA_UpdateAppTitle" );
	_RA_HandleHTTPResults	= (void(CCONV *)())										GetProcAddress( g_hRADLL, "_RA_HandleHTTPResults" );
	_RA_ConfirmLoadNewRom	= (bool(CCONV *)(bool))									GetProcAddress( g_hRADLL, "_RA_ConfirmLoadNewRom" );
	_RA_CreatePopupMenu		= (HMENU(CCONV *)(void))								GetProcAddress( g_hRADLL, "_RA_CreatePopupMenu" );
	_RA_InitDirectX			= (void(CCONV *)(void))									GetProcAddress( g_hRADLL, "_RA_InitDirectX" );
	_RA_OnPaint				= (void(CCONV *)(HWND))									GetProcAddress( g_hRADLL, "_RA_OnPaint" );
	_RA_InvokeDialog		= (void(CCONV *)(LPARAM))								GetProcAddress( g_hRADLL, "_RA_InvokeDialog" );
	_RA_SetPaused			= (void(CCONV *)(bool))									GetProcAddress( g_hRADLL, "_RA_SetPaused" );
	_RA_OnLoadState			= (void(CCONV *)(const char*))							GetProcAddress( g_hRADLL, "_RA_OnLoadState" );
	_RA_OnSaveState			= (void(CCONV *)(const char*))							GetProcAddress( g_hRADLL, "_RA_OnSaveState" );
	_RA_DoAchievementsFrame = (void(CCONV *)())										GetProcAddress( g_hRADLL, "_RA_DoAchievementsFrame" );
	_RA_SetConsoleID		= (int(CCONV *)(unsigned int))							GetProcAddress( g_hRADLL, "_RA_SetConsoleID" );
	_RA_HardcoreModeIsActive= (int(CCONV *)())										GetProcAddress( g_hRADLL, "_RA_HardcoreModeIsActive" );
	_RA_EnableHardcoreMode	= (void(CCONV *)())										GetProcAddress( g_hRADLL, "_RA_EnableHardcoreMode");
	_RA_DisableHardcoreMode = (void(CCONV *)())										GetProcAddress( g_hRADLL, "_RA_DisableHardcoreMode");
	_RA_HTTPGetRequestExists= (int(CCONV *)(const char*))							GetProcAddress( g_hRADLL, "_RA_HTTPGetRequestExists" );

	_RA_InstallSharedFunctions = ( void(CCONV *)( bool(*)(), void(*)(), void(*)(), void(*)(), void(*)(char*), void(*)(), void(*)(const char*) ) ) GetProcAddress( g_hRADLL, "_RA_InstallSharedFunctionsExt" );

	return _RA_IntegrationVersion ? _RA_IntegrationVersion() : "0.000";
}

//	Console IDs: see enum EmulatorID in header
void RA_Init( HWND hMainHWND, int nConsoleID, const char* sClientVersion )
{
	DWORD nBytesRead = 0;
	char buffer[1024];
	ZeroMemory( buffer, 1024 );
	if( DoBlockingHttpGet( "LatestIntegration.html", buffer, 1024, &nBytesRead ) == FALSE )
	{
		MessageBoxA( NULL, "Cannot access www.retroachievements.org - working offline.", "Warning", MB_OK|MB_ICONEXCLAMATION );
		return;
	}

	const unsigned int nLatestDLLVer = strtol( buffer+2, NULL, 10 );

	BOOL bInstalled = FALSE;
	int nMBReply = IDNO;
	do
	{
		const char* sVerInstalled = _RA_InstallIntegration();
		const unsigned int nVerInstalled = strtol( sVerInstalled+2, NULL, 10 );
		if( nVerInstalled < nLatestDLLVer )
		{
			RA_Shutdown();	//	Unhook the DLL, it's out of date. We may need to overwrite the DLL, so unhook it.%

			char sErrorMsg[2048];
			snprintf( sErrorMsg, 2048, "%s\nLatest Version: 0.%03d\n%s",
				nVerInstalled == 0 ? 
					"Cannot find or load RA_Integration.dll" :
					"A new version of the RetroAchievements Toolset is available!",
				nLatestDLLVer,
				"Automatically update your RetroAchievements Toolset file?" );

			nMBReply = MessageBoxA( NULL, sErrorMsg, "Warning", MB_YESNO );

			if( nMBReply == IDYES )
			{
				FetchIntegrationFromWeb();
			}
		}
		else
		{
			bInstalled = TRUE;
			break;
		}

	} while( nMBReply == IDYES );

	if( bInstalled )
		_RA_InitI( hMainHWND, nConsoleID, sClientVersion );
	else
		RA_Shutdown();

}

void RA_InstallSharedFunctions( bool(*fpIsActive)(void), void(*fpCauseUnpause)(void), void(*fpCausePause)(void), void(*fpRebuildMenu)(void), void(*fpEstimateTitle)(char*), void(*fpResetEmulation)(void), void(*fpLoadROM)(const char*) )
{
	_RA_GameIsActive			= fpIsActive;
	_RA_CauseUnpause			= fpCauseUnpause;
	_RA_CausePause				= fpCausePause;
	_RA_RebuildMenu				= fpRebuildMenu;
	_RA_GetEstimatedGameTitle	= fpEstimateTitle;
	_RA_ResetEmulation			= fpResetEmulation;
	_RA_LoadROM					= fpLoadROM;

	//	Also install *within* DLL! FFS
	if( _RA_InstallSharedFunctions != NULL )
		_RA_InstallSharedFunctions( fpIsActive, fpCauseUnpause, fpCausePause, fpRebuildMenu, fpEstimateTitle, fpResetEmulation, fpLoadROM );
}

void RA_Shutdown()
{
	//	Call shutdown on toolchain
	if( _RA_Shutdown != NULL )
		_RA_Shutdown();

	//	Clear func ptrs
	_RA_IntegrationVersion		= nullptr;
	_RA_InitI					= nullptr;
	_RA_Shutdown				= nullptr;
	_RA_UserLoggedIn			= nullptr;
	_RA_Username				= nullptr;
	_RA_UpdateOverlay			= nullptr;
	_RA_UpdatePopups			= nullptr;
	_RA_RenderOverlay			= nullptr;
	_RA_RenderPopups			= nullptr;
	_RA_OnLoadNewRom			= nullptr;
	_RA_InstallMemoryBank		= nullptr;
	_RA_ClearMemoryBanks		= nullptr;
	_RA_UpdateAppTitle			= nullptr;
	_RA_HandleHTTPResults		= nullptr;
	_RA_ConfirmLoadNewRom		= nullptr;
	_RA_CreatePopupMenu			= nullptr;
	_RA_InitDirectX				= nullptr;
	_RA_OnPaint					= nullptr;
	_RA_InvokeDialog			= nullptr;
	_RA_SetPaused				= nullptr;
	_RA_OnLoadState				= nullptr;
	_RA_OnSaveState				= nullptr;
	_RA_DoAchievementsFrame		= nullptr;
	_RA_InstallSharedFunctions	= nullptr;

	_RA_GameIsActive			= nullptr;
	_RA_CauseUnpause			= nullptr;
	_RA_CausePause				= nullptr;
	_RA_RebuildMenu				= nullptr;
	_RA_GetEstimatedGameTitle	= nullptr;
	_RA_ResetEmulation			= nullptr;
	_RA_LoadROM					= nullptr;
	_RA_SetConsoleID			= nullptr;
	_RA_HardcoreModeIsActive	= nullptr;
	_RA_EnableHardcoreMode		= nullptr;
	_RA_DisableHardcoreMode		= nullptr;
	
	_RA_HTTPGetRequestExists	= nullptr;
	_RA_AttemptLogin			= nullptr;

	//	Uninstall DLL
	FreeLibrary( g_hRADLL );
}


#endif //RA_EXPORTS
