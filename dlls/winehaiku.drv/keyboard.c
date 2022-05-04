/*
 * X11 keyboard driver
 *
 * Copyright 1993 Bob Amstadt
 * Copyright 1996 Albrecht Kleine
 * Copyright 1997 David Faure
 * Copyright 1998 Morten Welinder
 * Copyright 1998 Ulrich Weigand
 * Copyright 1999 Ove Kåven
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

#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#define NONAMELESSUNION

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "winreg.h"
#include "winnls.h"
#include "ime.h"
#include "haikudrv.h"
#include "wine/server.h"
#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(keyboard);
WINE_DECLARE_DEBUG_CHANNEL(key);


/***********************************************************************
 *		ActivateKeyboardLayout (HAIKUDRV.@)
 */
BOOL HAIKUDRV_ActivateKeyboardLayout(HKL hkl, UINT flags)
{
    return TRUE;
}


/***********************************************************************
 *		VkKeyScanEx (HAIKUDRV.@)
 *
 * Note: Windows ignores HKL parameter and uses current active layout instead
 */
SHORT HAIKUDRV_VkKeyScanEx(WCHAR wChar, HKL hkl)
{
    //FIXME("(%u): stub\n", wChar);
    return 0;
}

/***********************************************************************
 *		MapVirtualKeyEx (HAIKUDRV.@)
 */
UINT HAIKUDRV_MapVirtualKeyEx(UINT wCode, UINT wMapType, HKL hkl)
{
    //FIXME("(%u): stub\n", wCode);
    return 0;
}

/***********************************************************************
 *		GetKeyNameText (HAIKUDRV.@)
 */
INT HAIKUDRV_GetKeyNameText(LONG lParam, LPWSTR lpBuffer, INT nSize)
{
  return 0;
}

/***********************************************************************
 *		Beep (HAIKUDRV.@)
 */
void HAIKUDRV_Beep(void)
{
}
