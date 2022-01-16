/*
 * X11 tablet driver
 *
 * Copyright 2003 CodeWeavers (Aric Stewart)
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

#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <dlfcn.h>

#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "haikudrv.h"
#include "wine/unicode.h"
#include "wine/debug.h"
#include "wintab.h"

WINE_DEFAULT_DEBUG_CHANNEL(wintab32);


/***********************************************************************
 *             HAIKUDRV_LoadTabletInfo (HAIKUDRV.@)
 */
BOOL CDECL HAIKUDRV_LoadTabletInfo(HWND hwnddefault)
{
    return FALSE;
}

/***********************************************************************
 *		HAIKUDRV_AttachEventQueueToTablet (HAIKUDRV.@)
 */
int CDECL HAIKUDRV_AttachEventQueueToTablet(HWND hOwner)
{
    return 0;
}

/***********************************************************************
 *		HAIKUDRV_GetCurrentPacket (HAIKUDRV.@)
 */
int CDECL HAIKUDRV_GetCurrentPacket(/*LPWTPACKET*/void* packet)
{
    return 0;
}

/***********************************************************************
 *		HAIKUDRV_WTInfoW (HAIKUDRV.@)
 */
UINT CDECL HAIKUDRV_WTInfoW(UINT wCategory, UINT nIndex, LPVOID lpOutput)
{
    return 0;
}
