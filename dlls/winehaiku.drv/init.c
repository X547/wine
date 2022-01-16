/*
 * Haiku graphics driver initialisation functions
 *
 * Copyright 1996 Alexandre Julliard
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
#include "haikudrv.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(haikudrv);


#define GDI_HOOK(name) .dc_funcs.p##name = HAIKUDRV_##name
#define GDI_WINE_HOOK(name) .dc_funcs.name = HAIKUDRV_##name
#define USER_HOOK(name) .p##name = HAIKUDRV_##name
const struct user_driver_funcs haikudrv_funcs =
{
    GDI_HOOK(CreateCompatibleDC),
    GDI_HOOK(CreateDC),
    GDI_HOOK(DeleteDC),
    GDI_HOOK(GetDeviceCaps),
//  GDI_HOOK(GetDeviceGammaRamp),
//  GDI_HOOK(SetDeviceGammaRamp),
//  GDI_WINE_HOOK(wine_get_wgl_driver),
    GDI_WINE_HOOK(wine_get_vulkan_driver),
    .dc_funcs.priority = GDI_PRIORITY_GRAPHICS_DRV,

    /* keyboard functions */
    USER_HOOK(ActivateKeyboardLayout),
    USER_HOOK(Beep),
    USER_HOOK(GetKeyNameText),
    USER_HOOK(MapVirtualKeyEx),
    USER_HOOK(ToUnicodeEx),
    USER_HOOK(VkKeyScanEx),

    /* cursor/icon functions */
    USER_HOOK(DestroyCursorIcon),
    USER_HOOK(SetCursor),
    USER_HOOK(GetCursorPos),
    USER_HOOK(SetCursorPos),
    USER_HOOK(ClipCursor),

    /* clipboard functions */
    USER_HOOK(UpdateClipboard),

    /* display modes */
    USER_HOOK(ChangeDisplaySettingsEx),
    USER_HOOK(EnumDisplaySettingsEx),
    USER_HOOK(UpdateDisplayDevices),

    /* windowing functions */
    USER_HOOK(CreateDesktopWindow),
    USER_HOOK(CreateWindow),
    USER_HOOK(DestroyWindow),
    USER_HOOK(FlashWindowEx),
//  USER_HOOK(GetDC),
//  USER_HOOK(MsgWaitForMultipleObjectsEx),
//  USER_HOOK(ReleaseDC),
//  USER_HOOK(ScrollDC),
    USER_HOOK(SetCapture),
    USER_HOOK(SetFocus),
    USER_HOOK(SetLayeredWindowAttributes),
    USER_HOOK(SetParent),
    USER_HOOK(SetWindowIcon),
    USER_HOOK(SetWindowRgn),
    USER_HOOK(SetWindowStyle),
    USER_HOOK(SetWindowText),
    USER_HOOK(ShowWindow),
    USER_HOOK(SysCommand),
    USER_HOOK(UpdateLayeredWindow),
    USER_HOOK(WindowMessage),
    USER_HOOK(WindowPosChanging),
    USER_HOOK(WindowPosChanged),

    /* system parameters */
    USER_HOOK(SystemParametersInfo),

    /* thread management */
    USER_HOOK(ThreadDetach),
};
#undef GDI_HOOK
#undef GDI_WINE_HOOK
#undef USER_HOOK

void init_user_driver(void)
{
    __wine_set_user_driver(&haikudrv_funcs, WINE_GDI_DRIVER_VERSION);
}
