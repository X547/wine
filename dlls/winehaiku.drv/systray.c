/*
 * X11 system tray management
 *
 * Copyright (C) 2004 Mike Hearn, for CodeWeavers
 * Copyright (C) 2005 Robert Shearman
 * Copyright (C) 2008 Alexandre Julliard
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
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#define NONAMELESSUNION
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "commctrl.h"
#include "shellapi.h"

#include "haikudrv.h"
#include "wine/list.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(systray);


/***********************************************************************
 *              wine_notify_icon   (HAIKUDRV.@)
 *
 * Driver-side implementation of Shell_NotifyIcon.
 */
int CDECL wine_notify_icon( DWORD msg, NOTIFYICONDATAW *data )
{
    BOOL ret = FALSE;

    switch (msg)
    {
    case NIM_ADD:
        break;
    case NIM_DELETE:
        break;
    case NIM_MODIFY:
        break;
    case NIM_SETVERSION:
        break;
    case 0xdead:  /* Wine extension: owner window has died */
        break;
    default:
        FIXME( "unhandled tray message: %u\n", msg );
        break;
    }
    return ret;
}
