/*
 * The IME for interfacing with XIM
 *
 * Copyright 2008 CodeWeavers, Aric Stewart
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

/*
 * Notes:
 *  The normal flow for IMM/IME Processing is as follows.
 * 1) The Keyboard Driver generates key messages which are first passed to
 *    the IMM and then to IME via ImeProcessKey. If the IME returns 0  then
 *    it does not want the key and the keyboard driver then generates the
 *    WM_KEYUP/WM_KEYDOWN messages.  However if the IME is going to process the
 *    key it returns non-zero.
 * 2) If the IME is going to process the key then the IMM calls ImeToAsciiEx to
 *    process the key.  the IME modifies the HIMC structure to reflect the
 *    current state and generates any messages it needs the IMM to process.
 * 3) IMM checks the messages and send them to the application in question. From
 *    here the IMM level deals with if the application is IME aware or not.
 *
 *  This flow does not work well for the X11 driver and XIM.
 *   (It works fine for Mac)
 *  As such we will have to reroute step 1.  Instead the haikudrv driver will
 *  generate an XIM events and call directly into this IME implementation.
 *  As such we will have to use the alternative ImmGenerateMessage path to be
 *  generate the messages that we want the IMM layer to send to the application.
 */

#include "config.h"

#include <stdarg.h>
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "winerror.h"
#include "wine/debug.h"
#include "imm.h"
#include "ddk/imm.h"
#include "winnls.h"
#include "haikudrv.h"

WINE_DEFAULT_DEBUG_CHANNEL(imm);


BOOL WINAPI ImeInquire(LPIMEINFO lpIMEInfo, LPWSTR lpszUIClass,
                       LPCWSTR lpszOption)
{
    return TRUE;
}

BOOL WINAPI ImeConfigure(HKL hKL,HWND hWnd, DWORD dwMode, LPVOID lpData)
{
    FIXME("(%p, %p, %d, %p): stub\n", hKL, hWnd, dwMode, lpData);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

DWORD WINAPI ImeConversionList(HIMC hIMC, LPCWSTR lpSource,
                LPCANDIDATELIST lpCandList, DWORD dwBufLen, UINT uFlag)

{
    FIXME("(%p, %s, %p, %d, %d): stub\n", hIMC, debugstr_w(lpSource),
                                          lpCandList, dwBufLen, uFlag);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

BOOL WINAPI ImeDestroy(UINT uForce)
{
    return TRUE;
}

LRESULT WINAPI ImeEscape(HIMC hIMC, UINT uSubFunc, LPVOID lpData)
{
    FIXME("(%p, %d, %p): stub\n", hIMC, uSubFunc, lpData);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

BOOL WINAPI ImeProcessKey(HIMC hIMC, UINT vKey, LPARAM lKeyData, const LPBYTE lpbKeyState)
{
    /* See the comment at the head of this file */
    TRACE("We do no processing via this route\n");
    return FALSE;
}

BOOL WINAPI ImeSelect(HIMC hIMC, BOOL fSelect)
{
    return TRUE;
}

BOOL WINAPI ImeSetActiveContext(HIMC hIMC,BOOL fFlag)
{
    static int once;

    if (!once++)
        FIXME("(%p, %x): stub\n", hIMC, fFlag);
    return TRUE;
}

UINT WINAPI ImeToAsciiEx (UINT uVKey, UINT uScanCode, const LPBYTE lpbKeyState,
                          LPDWORD lpdwTransKey, UINT fuState, HIMC hIMC)
{
    /* See the comment at the head of this file */
    TRACE("We do no processing via this route\n");
    return 0;
}

BOOL WINAPI NotifyIME(HIMC hIMC, DWORD dwAction, DWORD dwIndex, DWORD dwValue)
{
    return FALSE;
}

BOOL WINAPI ImeRegisterWord(LPCWSTR lpszReading, DWORD dwStyle,
                            LPCWSTR lpszRegister)
{
    FIXME("(%s, %d, %s): stub\n", debugstr_w(lpszReading), dwStyle,
                                  debugstr_w(lpszRegister));
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

BOOL WINAPI ImeUnregisterWord(LPCWSTR lpszReading, DWORD dwStyle,
                              LPCWSTR lpszUnregister)
{
    FIXME("(%s, %d, %s): stub\n", debugstr_w(lpszReading), dwStyle,
                                  debugstr_w(lpszUnregister));
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

UINT WINAPI ImeGetRegisterWordStyle(UINT nItem, LPSTYLEBUFW lpStyleBuf)
{
    FIXME("(%d, %p): stub\n", nItem, lpStyleBuf);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

UINT WINAPI ImeEnumRegisterWord(REGISTERWORDENUMPROCW lpfnEnumProc,
                                LPCWSTR lpszReading, DWORD dwStyle,
                                LPCWSTR lpszRegister, LPVOID lpData)
{
    FIXME("(%p, %s, %d, %s, %p): stub\n", lpfnEnumProc,
            debugstr_w(lpszReading), dwStyle, debugstr_w(lpszRegister),
            lpData);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

BOOL WINAPI ImeSetCompositionString(HIMC hIMC, DWORD dwIndex, LPCVOID lpComp,
                                    DWORD dwCompLen, LPCVOID lpRead,
                                    DWORD dwReadLen)
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

DWORD WINAPI ImeGetImeMenuItems(HIMC hIMC,  DWORD dwFlags,  DWORD dwType,
            LPIMEMENUITEMINFOW lpImeParentMenu, LPIMEMENUITEMINFOW lpImeMenu,
            DWORD dwSize)
{
    FIXME("(%p, %x %x %p %p %x): stub\n", hIMC, dwFlags, dwType,
                                lpImeParentMenu, lpImeMenu, dwSize);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}
