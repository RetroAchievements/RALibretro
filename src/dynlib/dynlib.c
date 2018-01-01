#include "dynlib.h"

#ifdef _WIN32

#include <stdio.h>

const char* dynlib_error( void )
{
  static char msg[ 512 ];
  
  DWORD err = GetLastError();
  
  DWORD res = FormatMessage(
    FORMAT_MESSAGE_FROM_SYSTEM,
    NULL,
    err,
    MAKELANGID( LANG_ENGLISH, SUBLANG_DEFAULT ),
    msg,
    sizeof( msg ) - 1,
    NULL
  );

  if ( res == 0 )
  {
    snprintf( msg, sizeof( msg ) - 1, "Error %lu", err );
    msg[ sizeof( msg ) - 1 ] = 0;
  }
  
  return msg;
}

#endif
