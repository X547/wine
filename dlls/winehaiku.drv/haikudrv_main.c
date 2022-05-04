/*
 * HAIKUDRV initialization code
 *
 * Copyright 1998 Patrik Stridvall
 * Copyright 2000 Alexandre Julliard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winreg.h"

#include "haikudrv.h"
#include "wine/server.h"
#include "wine/unicode.h"
#include "wine/debug.h"
#include "wine/list.h"
#include "wine/heap.h"

WINE_DEFAULT_DEBUG_CHANNEL(haikudrv);
WINE_DECLARE_DEBUG_CHANNEL(synchronous);
WINE_DECLARE_DEBUG_CHANNEL(winediag);

DWORD thread_data_tls_index = TLS_OUT_OF_INDEXES;


/***********************************************************************
 *           HAIKUDRV process initialisation routine
 */
static BOOL process_attach(void)
{
    if (!HaikuStartApplication()) return FALSE;
    if ((thread_data_tls_index = TlsAlloc()) == TLS_OUT_OF_INDEXES) return FALSE;
    init_user_driver();
    return TRUE;
}


/***********************************************************************
 *           HAIKUDRV initialisation routine
 */
BOOL WINAPI DllMain( HINSTANCE hinst, DWORD reason, LPVOID reserved )
{
    BOOL ret = TRUE;

    switch(reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls( hinst );
        ret = process_attach();
        break;
    }
    return ret;
}


/***********************************************************************
 *              SystemParametersInfo (HAIKUDRV.@)
 */
BOOL HAIKUDRV_SystemParametersInfo( UINT action, UINT int_param, void *ptr_param, UINT flags )
{
    return FALSE;  /* let user32 handle it */
}

/**********************************************************************
 *           HAIKUDRV_D3DKMTSetVidPnSourceOwner
 */
NTSTATUS HAIKUDRV_D3DKMTSetVidPnSourceOwner( const D3DKMT_SETVIDPNSOURCEOWNER *desc )
{
    NTSTATUS status = STATUS_SUCCESS;
    return status;
}

/**********************************************************************
 *           HAIKUDRV_D3DKMTCheckVidPnExclusiveOwnership
 */
NTSTATUS HAIKUDRV_D3DKMTCheckVidPnExclusiveOwnership( const D3DKMT_CHECKVIDPNEXCLUSIVEOWNERSHIP *desc )
{
    return STATUS_SUCCESS;
}
