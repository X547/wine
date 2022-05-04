/*
 * X11 driver definitions
 *
 * Copyright 1996 Alexandre Julliard
 * Copyright 1999 Patrik Stridvall
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

#ifndef __WINE_HAIKUDRV_H
#define __WINE_HAIKUDRV_H

#ifndef __WINE_CONFIG_H
# error You must include config.h to use this header
#endif

#include <limits.h>

#undef Status  /* avoid conflict with wintrnl.h */
typedef int Status;

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "wine/gdi_driver.h"
#include "wine/list.h"

#define MAX_DASHLEN 16

#define WINE_XDND_VERSION 5

static inline void reset_bounds( RECT *bounds )
{
    bounds->left = bounds->top = INT_MAX;
    bounds->right = bounds->bottom = INT_MIN;
}

static inline void add_bounds_rect( RECT *bounds, const RECT *rect )
{
    if (rect->left >= rect->right || rect->top >= rect->bottom) return;
    bounds->left   = min( bounds->left, rect->left );
    bounds->top    = min( bounds->top, rect->top );
    bounds->right  = max( bounds->right, rect->right );
    bounds->bottom = max( bounds->bottom, rect->bottom );
}

/* Wine driver X11 functions */

extern BOOL CDECL HAIKUDRV_CreateDC(PHYSDEV *pdev, LPCWSTR device, LPCWSTR output, const DEVMODEW* initData);
extern BOOL CDECL HAIKUDRV_CreateCompatibleDC(PHYSDEV orig, PHYSDEV *pdev);
extern BOOL CDECL HAIKUDRV_DeleteDC(PHYSDEV dev);
extern INT CDECL HAIKUDRV_GetDeviceCaps(PHYSDEV dev, INT cap);
extern const struct vulkan_funcs *CDECL HAIKUDRV_wine_get_vulkan_driver(UINT version);

extern BOOL HAIKUDRV_ActivateKeyboardLayout( HKL hkl, UINT flags ) DECLSPEC_HIDDEN;
extern void HAIKUDRV_Beep(void) DECLSPEC_HIDDEN;
extern INT HAIKUDRV_GetKeyNameText( LONG lparam, LPWSTR buffer, INT size ) DECLSPEC_HIDDEN;
extern UINT HAIKUDRV_MapVirtualKeyEx( UINT code, UINT map_type, HKL hkl ) DECLSPEC_HIDDEN;
extern INT HAIKUDRV_ToUnicodeEx( UINT virtKey, UINT scanCode, const BYTE *lpKeyState,
                                     LPWSTR bufW, int bufW_size, UINT flags, HKL hkl ) DECLSPEC_HIDDEN;
extern SHORT HAIKUDRV_VkKeyScanEx( WCHAR wChar, HKL hkl ) DECLSPEC_HIDDEN;
extern void HAIKUDRV_DestroyCursorIcon( HCURSOR handle ) DECLSPEC_HIDDEN;
extern void HAIKUDRV_SetCursor( HCURSOR handle ) DECLSPEC_HIDDEN;
extern BOOL HAIKUDRV_SetCursorPos( INT x, INT y ) DECLSPEC_HIDDEN;
extern BOOL HAIKUDRV_GetCursorPos( LPPOINT pos ) DECLSPEC_HIDDEN;
extern BOOL HAIKUDRV_ClipCursor( LPCRECT clip ) DECLSPEC_HIDDEN;
extern LONG HAIKUDRV_ChangeDisplaySettingsEx( LPCWSTR devname, LPDEVMODEW devmode,
                                                  HWND hwnd, DWORD flags, LPVOID lpvoid ) DECLSPEC_HIDDEN;
extern BOOL HAIKUDRV_EnumDisplaySettingsEx( LPCWSTR name, DWORD n, LPDEVMODEW devmode,
                                                DWORD flags ) DECLSPEC_HIDDEN;
extern void HAIKUDRV_UpdateDisplayDevices( const struct gdi_device_manager *device_manager,
                                               BOOL force, void *param ) DECLSPEC_HIDDEN;
extern BOOL HAIKUDRV_CreateDesktopWindow( HWND hwnd ) DECLSPEC_HIDDEN;
extern BOOL HAIKUDRV_CreateWindow( HWND hwnd ) DECLSPEC_HIDDEN;
extern void HAIKUDRV_DestroyWindow( HWND hwnd ) DECLSPEC_HIDDEN;
extern void HAIKUDRV_FlashWindowEx( PFLASHWINFO pfinfo ) DECLSPEC_HIDDEN;
extern void HAIKUDRV_GetDC( HDC hdc, HWND hwnd, HWND top, const RECT *win_rect,
                                const RECT *top_rect, DWORD flags ) DECLSPEC_HIDDEN;
extern void HAIKUDRV_ReleaseDC( HWND hwnd, HDC hdc ) DECLSPEC_HIDDEN;
extern BOOL HAIKUDRV_ScrollDC( HDC hdc, INT dx, INT dy, HRGN update ) DECLSPEC_HIDDEN;
extern void HAIKUDRV_SetCapture( HWND hwnd, UINT flags ) DECLSPEC_HIDDEN;
extern void HAIKUDRV_SetLayeredWindowAttributes( HWND hwnd, COLORREF key, BYTE alpha,
                                                     DWORD flags ) DECLSPEC_HIDDEN;
extern void HAIKUDRV_SetParent( HWND hwnd, HWND parent, HWND old_parent ) DECLSPEC_HIDDEN;
extern void HAIKUDRV_SetWindowIcon( HWND hwnd, UINT type, HICON icon ) DECLSPEC_HIDDEN;
extern void HAIKUDRV_SetWindowRgn( HWND hwnd, HRGN hrgn, BOOL redraw ) DECLSPEC_HIDDEN;
extern void HAIKUDRV_SetWindowStyle( HWND hwnd, INT offset, STYLESTRUCT *style ) DECLSPEC_HIDDEN;
extern void HAIKUDRV_SetWindowText( HWND hwnd, LPCWSTR text ) DECLSPEC_HIDDEN;
extern UINT HAIKUDRV_ShowWindow( HWND hwnd, INT cmd, RECT *rect, UINT swp ) DECLSPEC_HIDDEN;
extern LRESULT HAIKUDRV_SysCommand( HWND hwnd, WPARAM wparam, LPARAM lparam ) DECLSPEC_HIDDEN;
extern void HAIKUDRV_UpdateClipboard(void) DECLSPEC_HIDDEN;
extern BOOL HAIKUDRV_UpdateLayeredWindow( HWND hwnd, const UPDATELAYEREDWINDOWINFO *info,
                                              const RECT *window_rect ) DECLSPEC_HIDDEN;
extern LRESULT HAIKUDRV_WindowMessage( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp ) DECLSPEC_HIDDEN;
extern BOOL HAIKUDRV_WindowPosChanging( HWND hwnd, HWND insert_after, UINT swp_flags,
                                            const RECT *window_rect, const RECT *client_rect, RECT *visible_rect,
                                            struct window_surface **surface ) DECLSPEC_HIDDEN;
extern void HAIKUDRV_WindowPosChanged( HWND hwnd, HWND insert_after, UINT swp_flags,
                                           const RECT *rectWindow, const RECT *rectClient,
                                           const RECT *visible_rect, const RECT *valid_rects,
                                           struct window_surface *surface ) DECLSPEC_HIDDEN;
extern BOOL HAIKUDRV_SystemParametersInfo( UINT action, UINT int_param, void *ptr_param,
                                               UINT flags ) DECLSPEC_HIDDEN;
extern void HAIKUDRV_ThreadDetach(void) DECLSPEC_HIDDEN;

/* X11 driver internal functions */

extern RGNDATA *HAIKUDRV_GetRegionData( HRGN hrgn, HDC hdc_lptodp ) DECLSPEC_HIDDEN;

/* IME support */
extern void IME_SetOpenStatus(BOOL fOpen) DECLSPEC_HIDDEN;
extern void IME_SetCompositionStatus(BOOL fOpen) DECLSPEC_HIDDEN;
extern INT IME_GetCursorPos(void) DECLSPEC_HIDDEN;
extern void IME_SetCursorPos(DWORD pos) DECLSPEC_HIDDEN;
extern void IME_UpdateAssociation(HWND focus) DECLSPEC_HIDDEN;
extern BOOL IME_SetCompositionString(DWORD dwIndex, LPCVOID lpComp,
                                     DWORD dwCompLen, LPCVOID lpRead,
                                     DWORD dwReadLen) DECLSPEC_HIDDEN;
extern void IME_SetResultString(LPWSTR lpResult, DWORD dwResultlen) DECLSPEC_HIDDEN;


/**************************************************************************
 * X11 USER driver
 */

struct haikudrv_thread_data;

extern struct haikudrv_thread_data *haikudrv_init_thread_data(void) DECLSPEC_HIDDEN;
extern DWORD thread_data_tls_index DECLSPEC_HIDDEN;

static inline struct haikudrv_thread_data *haikudrv_thread_data(void)
{
    DWORD err = GetLastError();  /* TlsGetValue always resets last error */
    struct haikudrv_thread_data *data = (struct haikudrv_thread_data*)TlsGetValue( thread_data_tls_index );
    SetLastError( err );
    return data;
}

extern BOOL clipping_cursor DECLSPEC_HIDDEN;
extern BOOL keyboard_grabbed DECLSPEC_HIDDEN;
extern unsigned int screen_bpp DECLSPEC_HIDDEN;
extern BOOL use_xkb DECLSPEC_HIDDEN;
extern BOOL usexrandr DECLSPEC_HIDDEN;
extern BOOL usexvidmode DECLSPEC_HIDDEN;
extern BOOL ximInComposeMode DECLSPEC_HIDDEN;
extern BOOL use_take_focus DECLSPEC_HIDDEN;
extern BOOL use_primary_selection DECLSPEC_HIDDEN;
extern BOOL use_system_cursors DECLSPEC_HIDDEN;
extern BOOL show_systray DECLSPEC_HIDDEN;
extern BOOL grab_pointer DECLSPEC_HIDDEN;
extern BOOL grab_fullscreen DECLSPEC_HIDDEN;
extern BOOL usexcomposite DECLSPEC_HIDDEN;
extern BOOL managed_mode DECLSPEC_HIDDEN;
extern BOOL decorated_mode DECLSPEC_HIDDEN;
extern BOOL private_color_map DECLSPEC_HIDDEN;
extern int primary_monitor DECLSPEC_HIDDEN;
extern int copy_default_colors DECLSPEC_HIDDEN;
extern int alloc_system_colors DECLSPEC_HIDDEN;
extern int xrender_error_base DECLSPEC_HIDDEN;
extern HMODULE haikudrv_module DECLSPEC_HIDDEN;
extern char *process_name DECLSPEC_HIDDEN;

/* X11 event driver */


static inline void mirror_rect( const RECT *window_rect, RECT *rect )
{
    int width = window_rect->right - window_rect->left;
    int tmp = rect->left;
    rect->left = width - rect->right;
    rect->right = width - tmp;
}

extern void HAIKUDRV_InitClipboard(void) DECLSPEC_HIDDEN;
extern void HAIKUDRV_SetFocus( HWND hwnd ) DECLSPEC_HIDDEN;
extern DWORD HAIKUDRV_MsgWaitForMultipleObjectsEx( DWORD count, const HANDLE *handles, DWORD timeout,
                                                       DWORD mask, DWORD flags ) DECLSPEC_HIDDEN;

#define DEPTH_COUNT 3
extern const unsigned int *depths DECLSPEC_HIDDEN;

/* Required functions for changing and enumerating display settings */
struct haikudrv_settings_handler
{
    /* A name to tell what host driver is used */
    const char *name;

    /* Higher priority can override handlers with a lower priority */
    UINT priority;

    /* get_id() will be called to map a device name, e.g., \\.\DISPLAY1 to a driver specific id.
     * Following functions use this id to identify the device.
     *
     * Return FALSE if the device cannot be found and TRUE on success */
    BOOL (*get_id)(const WCHAR *device_name, ULONG_PTR *id);

    /* get_modes() will be called to get a list of supported modes of the device of id in modes
     * with respect to flags, which could be 0, EDS_RAWMODE or EDS_ROTATEDMODE. If the implementation
     * uses dmDriverExtra then every DEVMODEW in the list must have the same dmDriverExtra value
     *
     * Following fields in DEVMODE must be valid:
     * dmSize, dmDriverExtra, dmFields, dmDisplayOrientation, dmBitsPerPel, dmPelsWidth, dmPelsHeight,
     * dmDisplayFlags and dmDisplayFrequency
     *
     * Return FALSE on failure with parameters unchanged and error code set. Return TRUE on success */
    BOOL (*get_modes)(ULONG_PTR id, DWORD flags, DEVMODEW **modes, UINT *mode_count);

    /* free_modes() will be called to free the mode list returned from get_modes() */
    void (*free_modes)(DEVMODEW *modes);

    /* get_current_mode() will be called to get the current display mode of the device of id
     *
     * Following fields in DEVMODE must be valid:
     * dmFields, dmDisplayOrientation, dmBitsPerPel, dmPelsWidth, dmPelsHeight, dmDisplayFlags,
     * dmDisplayFrequency and dmPosition
     *
     * Return FALSE on failure with parameters unchanged and error code set. Return TRUE on success */
    BOOL (*get_current_mode)(ULONG_PTR id, DEVMODEW *mode);

    /* set_current_mode() will be called to change the display mode of the display device of id.
     * mode must be a valid mode from get_modes() with optional fields, such as dmPosition set.
     *
     * Return DISP_CHANGE_*, same as ChangeDisplaySettingsExW() return values */
    LONG (*set_current_mode)(ULONG_PTR id, DEVMODEW *mode);
};

extern void HAIKUDRV_Settings_SetHandler(const struct haikudrv_settings_handler *handler) DECLSPEC_HIDDEN;

extern void HAIKUDRV_resize_desktop(BOOL) DECLSPEC_HIDDEN;
void HAIKUDRV_Settings_Init(void) DECLSPEC_HIDDEN;

void HAIKUDRV_XF86VM_Init(void) DECLSPEC_HIDDEN;
void HAIKUDRV_XRandR_Init(void) DECLSPEC_HIDDEN;
void init_user_driver(void) DECLSPEC_HIDDEN;

/* X11 display device handler. Used to initialize display device registry data */

/* Required functions for display device registry initialization */
struct haikudrv_display_device_handler
{
    /* A name to tell what host driver is used */
    const char *name;

    /* Higher priority can override handlers with lower priority */
    INT priority;

    /* get_gpus will be called to get a list of GPUs. First GPU has to be where the primary adapter is.
     *
     * Return FALSE on failure with parameters unchanged */
    BOOL (*get_gpus)(struct gdi_gpu **gpus, int *count);

    /* get_adapters will be called to get a list of adapters in EnumDisplayDevices context under a GPU.
     * The first adapter has to be primary if GPU is primary.
     *
     * Return FALSE on failure with parameters unchanged */
    BOOL (*get_adapters)(ULONG_PTR gpu_id, struct gdi_adapter **adapters, int *count);

    /* get_monitors will be called to get a list of monitors in EnumDisplayDevices context under an adapter.
     * The first monitor has to be primary if adapter is primary.
     *
     * Return FALSE on failure with parameters unchanged */
    BOOL (*get_monitors)(ULONG_PTR adapter_id, struct gdi_monitor **monitors, int *count);

    /* free_gpus will be called to free a GPU list from get_gpus */
    void (*free_gpus)(struct gdi_gpu *gpus);

    /* free_adapters will be called to free an adapter list from get_adapters */
    void (*free_adapters)(struct gdi_adapter *adapters);

    /* free_monitors will be called to free a monitor list from get_monitors */
    void (*free_monitors)(struct gdi_monitor *monitors, int count);

    /* register_event_handlers will be called to register event handlers.
     * This function pointer is optional and can be NULL when driver doesn't support it */
    void (*register_event_handlers)(void);
};

extern HANDLE get_display_device_init_mutex(void) DECLSPEC_HIDDEN;
extern BOOL get_host_primary_gpu(struct gdi_gpu *gpu) DECLSPEC_HIDDEN;
extern void release_display_device_init_mutex(HANDLE) DECLSPEC_HIDDEN;
extern void HAIKUDRV_DisplayDevices_SetHandler(const struct haikudrv_display_device_handler *handler) DECLSPEC_HIDDEN;
extern void HAIKUDRV_DisplayDevices_Init(BOOL force) DECLSPEC_HIDDEN;
extern void HAIKUDRV_DisplayDevices_RegisterEventHandlers(void) DECLSPEC_HIDDEN;
extern void HAIKUDRV_DisplayDevices_Update(BOOL) DECLSPEC_HIDDEN;
/* Display device handler used in virtual desktop mode */
extern struct haikudrv_display_device_handler desktop_handler DECLSPEC_HIDDEN;


extern const struct user_driver_funcs haikudrv_funcs;


extern BOOL HaikuStartApplication(void);

#endif  /* __WINE_HAIKUDRV_H */
