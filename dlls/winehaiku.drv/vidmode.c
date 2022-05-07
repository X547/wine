/*
 * DirectDraw XVidMode interface
 *
 * Copyright 2001 TransGaming Technologies, Inc.
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

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <dlfcn.h>

#define NONAMELESSSTRUCT
#define NONAMELESSUNION

#include "haikudrv.h"

#include "windef.h"
#include "wingdi.h"
#include "wine/debug.h"
#include "wine/heap.h"

WINE_DEFAULT_DEBUG_CHANNEL(vidmode);


/***********************************************************************
 *		GetDeviceGammaRamp (HAIKUDRV.@)
 */
BOOL CDECL HAIKUDRV_GetDeviceGammaRamp(PHYSDEV dev, LPVOID ramp)
{
  return FALSE;
}

/***********************************************************************
 *		SetDeviceGammaRamp (HAIKUDRV.@)
 */
BOOL CDECL HAIKUDRV_SetDeviceGammaRamp(PHYSDEV dev, LPVOID ramp)
{
  return FALSE;
}
