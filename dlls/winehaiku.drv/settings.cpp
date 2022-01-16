/*
 * Wine X11drv display settings functions
 *
 * Copyright 2003 Alexander James Pasadyn
 * Copyright 2020 Zhiyi Zhang for CodeWeavers
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

#define NONAMELESSUNION
#define NONAMELESSSTRUCT

extern "C" {
#include "haikudrv.h"

#include "windef.h"
#include "winreg.h"
#include "wingdi.h"
#include "wine/debug.h"
#include "wine/heap.h"
#include "wine/unicode.h"
}

WINE_DEFAULT_DEBUG_CHANNEL(x11settings);


/***********************************************************************
 *		EnumDisplaySettingsEx  (HAIKUDRV.@)
 *
 */
BOOL CDECL HAIKUDRV_EnumDisplaySettingsEx( LPCWSTR name, DWORD n, LPDEVMODEW devmode, DWORD flags)
{
    switch (n) {
    	case ENUM_REGISTRY_SETTINGS:
    	case ENUM_CURRENT_SETTINGS:
    	case 0: {
    		devmode->u1.s2.dmPosition.x = 0;
    		devmode->u1.s2.dmPosition.y = 0;
    		devmode->dmPelsWidth = 1920;
    		devmode->dmPelsHeight = 1080;
    		devmode->dmDisplayFrequency = 60;
    		devmode->dmBitsPerPel = 32;
    		devmode->u1.s2.dmDisplayOrientation = 0;
    		return TRUE;
    	}
    	default:;
    }
    return FALSE;
}

/***********************************************************************
 *		ChangeDisplaySettingsEx  (HAIKUDRV.@)
 *
 */
LONG CDECL HAIKUDRV_ChangeDisplaySettingsEx( LPCWSTR devname, LPDEVMODEW devmode,
                                           HWND hwnd, DWORD flags, LPVOID lpvoid )
{
    return 0;
}
