/*
 * HAIKUDRV desktop window handling
 *
 * Copyright 2001 Alexandre Julliard
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

#define NONAMELESSSTRUCT
#define NONAMELESSUNION

#include "haikudrv.h"

/* avoid conflict with field names in included win32 headers */
#undef Status
#include "wine/debug.h"
#include "wine/heap.h"

WINE_DEFAULT_DEBUG_CHANNEL(haikudrv);


/***********************************************************************
 *		HAIKUDRV_create_desktop
 *
 * Create the X11 desktop window for the desktop mode.
 */
BOOL CDECL HAIKUDRV_create_desktop( UINT width, UINT height )
{
    return FALSE;
}
