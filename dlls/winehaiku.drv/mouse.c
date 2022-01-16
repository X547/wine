/*
 * X11 mouse driver
 *
 * Copyright 1998 Ulrich Weigand
 * Copyright 2007 Henri Verbeet
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

#include <math.h>
#include <dlfcn.h>
#include <stdarg.h>


#define NONAMELESSUNION
#define OEMRESOURCE
#include "windef.h"
#include "winbase.h"
#include "winreg.h"

#include "haikudrv.h"
#include "wine/server.h"
#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(cursor);


/***********************************************************************
 *		DestroyCursorIcon (HAIKUDRV.@)
 */
void CDECL HAIKUDRV_DestroyCursorIcon( HCURSOR handle )
{
}

/***********************************************************************
 *		SetCursor (HAIKUDRV.@)
 */
void CDECL HAIKUDRV_SetCursor( HCURSOR handle )
{
}

/***********************************************************************
 *		SetCursorPos (HAIKUDRV.@)
 */
BOOL CDECL HAIKUDRV_SetCursorPos( INT x, INT y )
{
    return TRUE;
}

/***********************************************************************
 *		GetCursorPos (HAIKUDRV.@)
 */
BOOL CDECL HAIKUDRV_GetCursorPos(LPPOINT pos)
{
    return TRUE;
}

/***********************************************************************
 *		ClipCursor (HAIKUDRV.@)
 */
BOOL CDECL HAIKUDRV_ClipCursor( LPCRECT clip )
{
    return TRUE;
}
