/*
 * Window related functions
 *
 * Copyright 1993, 1994, 1995, 1996, 2001 Alexandre Julliard
 * Copyright 1993 David Metcalfe
 * Copyright 1995, 1996 Alex Korobka
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

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#define thread_info haiku_thread_info

#include <MessageQueue.h>
#include <Application.h>
#include <Window.h>
#include <View.h>
#include <Bitmap.h>
#include <Screen.h>
#include <OS.h>
#include <Autolock.h>
#include <locks.h>

#undef thread_info

#include <map>


/* avoid conflict with field names in included win32 headers */
#undef Status
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "wine/unicode.h"

extern "C" {
#include "haikudrv.h"
#include "wine/debug.h"
#include "wine/server.h"
}

#undef SendMessage

WINE_DEFAULT_DEBUG_CHANNEL(haikudrv);

static HWND sCaptureWnd = NULL;

enum {
	windowResizedMsg = 1,
};


//#pragma mark -

struct haikudrv_thread_data {
};


static WORD sVkFromHaikuKeycodes[256];
static WORD sScanFromHaikuKeycodes[256];
static uint8 sHaikuFromVkKeycodes[256];
static key_map *sHaikuKeymap = NULL; // TODO: call free() on exit
static char *sHaikuKeymapChars = NULL; // TODO: call free() on exit


static void WriteSet(uint32 val)
{
	printf("{");
	bool first = true;
	for (uint32 i = 0; i < 32; i++) {
		if ((1 << i) & val) {
			if (first) first = false; else printf(", ");
			printf("%" B_PRIu32, i);
		}
	}
	printf("}");
}


static void FromHaikuKeyCode(WORD &vkey, WORD &scan, int32 key)
{
	switch (key) {
		case 0x01: vkey = VK_ESCAPE; scan = 0x01; break;
		case 0x02: vkey = VK_F1; scan = 0x3b; break;
		case 0x03: vkey = VK_F2; scan = 0x3c; break;
		case 0x04: vkey = VK_F3; scan = 0x3d; break;
		case 0x05: vkey = VK_F4; scan = 0x3e; break;
		case 0x06: vkey = VK_F5; scan = 0x3f; break;
		case 0x07: vkey = VK_F6; scan = 0x40; break;
		case 0x08: vkey = VK_F7; scan = 0x41; break;
		case 0x09: vkey = VK_F8; scan = 0x42; break;
		case 0x0a: vkey = VK_F9; scan = 0x43; break;
		case 0x0b: vkey = VK_F10; scan = 0x44; break;
		case 0x0c: vkey = VK_F11; scan = 0x57; break;
		case 0x0d: vkey = VK_F12; scan = 0x58; break;
		case 0x0e: vkey = VK_SNAPSHOT; scan = 0; break;
		case 0x0f: vkey = VK_SCROLL; scan = 0; break;

		case 0x12: vkey = '1'; scan = 0x02; break;
		case 0x13: vkey = '2'; scan = 0x03; break;
		case 0x14: vkey = '3'; scan = 0x04; break;
		case 0x15: vkey = '4'; scan = 0x05; break;
		case 0x16: vkey = '5'; scan = 0x06; break;
		case 0x17: vkey = '6'; scan = 0x07; break;
		case 0x18: vkey = '7'; scan = 0x08; break;
		case 0x19: vkey = '8'; scan = 0x09; break;
		case 0x1a: vkey = '9'; scan = 0x0a; break;
		case 0x1b: vkey = '0'; scan = 0x0b; break;
		case 0x1c: vkey = VK_OEM_MINUS; scan = 0x0c; break;
		case 0x1d: vkey = VK_OEM_7; scan = 0x0d; break;
		case 0x1e: vkey = VK_BACK; scan = 0x0e; break;
		case 0x1f: vkey = VK_INSERT; scan = 0; break;

		case 0x26: vkey = VK_TAB; scan = 0x0f; break;
		case 0x27: vkey = 'Q'; scan = 0x10; break;
		case 0x28: vkey = 'W'; scan = 0x11; break;
		case 0x29: vkey = 'E'; scan = 0x12; break;
		case 0x2a: vkey = 'R'; scan = 0x13; break;
		case 0x2b: vkey = 'T'; scan = 0x14; break;
		case 0x2c: vkey = 'Y'; scan = 0x15; break;
		case 0x2d: vkey = 'U'; scan = 0x16; break;
		case 0x2e: vkey = 'I'; scan = 0x17; break;
		case 0x2f: vkey = 'O'; scan = 0x18; break;
		case 0x30: vkey = 'P'; scan = 0x19; break;	
		case 0x31: vkey = VK_OEM_3; scan = 0; break;
		case 0x32: vkey = VK_OEM_4; scan = 0; break;
		case 0x33: vkey = VK_OEM_6; scan = 0; break;
		case 0x34: vkey = VK_DELETE; scan = 0; break;
	
		case 0x3b: vkey = VK_CAPITAL; scan = 0x3a; break;	
		case 0x3c: vkey = 'A'; scan = 0x1e; break;
		case 0x3d: vkey = 'S'; scan = 0x1f; break;
		case 0x3e: vkey = 'D'; scan = 0x20; break;
		case 0x3f: vkey = 'F'; scan = 0x21; break;
		case 0x40: vkey = 'G'; scan = 0x22; break;
		case 0x41: vkey = 'H'; scan = 0x23; break;
		case 0x42: vkey = 'J'; scan = 0x24; break;
		case 0x43: vkey = 'K'; scan = 0x25; break;
		case 0x44: vkey = 'L'; scan = 0x26; break;
		case 0x45: vkey = VK_OEM_PLUS; scan = 0x27; break;
		case 0x46: vkey = VK_OEM_1; scan = 0x28; break;
		case 0x47: vkey = VK_RETURN; scan = 0x1c; break;

		case 0x4b: vkey = VK_LSHIFT; scan = 0x2a; break;	
		case 0x4c: vkey = 'Z'; scan = 0x2c; break;
		case 0x4d: vkey = 'X'; scan = 0x2d; break;
		case 0x4e: vkey = 'C'; scan = 0x2e; break;
		case 0x4f: vkey = 'V'; scan = 0x2f; break;
		case 0x50: vkey = 'B'; scan = 0x30; break;
		case 0x51: vkey = 'N'; scan = 0x31; break;
		case 0x52: vkey = 'M'; scan = 0x32; break;	
		case 0x53: vkey = VK_OEM_COMMA; scan = 0x33; break;
		case 0x54: vkey = VK_OEM_PERIOD; scan = 0x34; break;
		case 0x55: vkey = VK_OEM_2; scan = 0x35; break;
		case 0x56: vkey = VK_RSHIFT; scan = 0x36; break;	
		case 0x57: vkey = VK_UP; scan = 0; break;

		case 0x5c: vkey = VK_LCONTROL; scan = 0; break;
		case 0x5d: vkey = VK_LMENU; scan = 0x38; break;
		case 0x5e: vkey = VK_SPACE; scan = 0x39; break;

		case 0x61: vkey = VK_LEFT; scan = 0; break;
		case 0x62: vkey = VK_DOWN; scan = 0; break;
		case 0x63: vkey = VK_RIGHT; scan = 0; break;

		case 0x66: vkey = VK_LWIN; scan = 0; break;
		case 0x68: vkey = VK_APPS; scan = 0; break;

		case 0x6a: vkey = VK_OEM_5; scan = 0; break;
		case 0x6b: vkey = VK_OEM_102; scan = 0; break;

		default: vkey = 0; scan = 0;
	}
}

static void InitKeymap()
{
	for (int i = 0; i < 256; i++) {
		FromHaikuKeyCode(sVkFromHaikuKeycodes[i], sScanFromHaikuKeycodes[i], i);
	}
	for (int i = 0; i < 256; i++) {
		sHaikuFromVkKeycodes[sVkFromHaikuKeycodes[i]] = i;
	}

	get_key_map(&sHaikuKeymap, &sHaikuKeymapChars);
}

static uint32 HaikuModifiersFromKeyState(const BYTE *lpKeyState)
{
	uint32 modifiers = 0;
	if (lpKeyState[VK_MENU   ] & 0x80) modifiers |= B_COMMAND_KEY;
	if (lpKeyState[VK_SHIFT  ] & 0x80) modifiers |= B_SHIFT_KEY;
	if (lpKeyState[VK_CONTROL] & 0x80) modifiers |= B_CONTROL_KEY;
	if (lpKeyState[VK_LWIN   ] & 0x80) modifiers |= B_OPTION_KEY;
	if (lpKeyState[VK_NUMLOCK] & 0x01) modifiers |= B_NUM_LOCK;
	if (lpKeyState[VK_CAPITAL] & 0x01) modifiers |= B_CAPS_LOCK;
	if (lpKeyState[VK_SCROLL ] & 0x01) modifiers |= B_SCROLL_LOCK;
	return modifiers;
}

static void MapKey(char *&chars, size_t &len, int32 key, uint32 modifiers)
{
	char *ch;
	switch (modifiers & (B_SHIFT_KEY | B_CONTROL_KEY | B_OPTION_KEY | B_CAPS_LOCK)) {
		case B_OPTION_KEY | B_CAPS_LOCK | B_SHIFT_KEY: ch = sHaikuKeymapChars + sHaikuKeymap->option_caps_shift_map[key]; break;
		case B_OPTION_KEY | B_CAPS_LOCK:               ch = sHaikuKeymapChars + sHaikuKeymap->option_caps_map[key];       break;
		case B_OPTION_KEY | B_SHIFT_KEY:               ch = sHaikuKeymapChars + sHaikuKeymap->option_shift_map[key];      break;
		case B_OPTION_KEY:                             ch = sHaikuKeymapChars + sHaikuKeymap->option_map[key];            break;
		case B_CAPS_LOCK  | B_SHIFT_KEY:               ch = sHaikuKeymapChars + sHaikuKeymap->caps_shift_map[key];        break;
		case B_CAPS_LOCK:                              ch = sHaikuKeymapChars + sHaikuKeymap->caps_map[key];              break;
		case B_SHIFT_KEY:                              ch = sHaikuKeymapChars + sHaikuKeymap->shift_map[key];             break;
		default:
			if (modifiers & B_CONTROL_KEY)               ch = sHaikuKeymapChars + sHaikuKeymap->control_map[key];
			else                                         ch = sHaikuKeymapChars + sHaikuKeymap->normal_map[key];
	}
	len = *ch;
	chars = ch + 1;
}


//#pragma mark -

class WineView: public BView {
private:
	HWND fHwnd;
	struct haikudrv_thread_data *fData;
	BBitmap *fBitmap;
	uint32 fOldMouseBtns;

public:
	WineView(HWND hwnd, struct haikudrv_thread_data *data, BRect frame, const char *name);
	void UpdateBitmap(BBitmap *bitmap, BRect dirty);
	void MessageReceived(BMessage *msg);

};

class WineWindow: public BWindow {
private:
	WineView *fView;
	HWND fHwnd;
	struct haikudrv_thread_data *fData;
	
public:
	mutex fCreateMutex;
	window_surface *fSurface;
	bool fSendResizeEvents;
	bool fInDestroy;
	RECT fNonClient;
	//HANDLE fEvent;

	WineWindow(HWND hwnd, struct haikudrv_thread_data *data, BRect frame);
	virtual ~WineWindow();
	
	bool QuitRequested() override;

	void FrameMoved(BPoint newPosition) override;
	void FrameResized(float newWidth, float newHeight) override;

	HWND Handle() {return fHwnd;}
	WineView *View() {return fView;}
};

class WineApplication: public BApplication {
public:
	WineApplication();
};


WineView::WineView(HWND hwnd, struct haikudrv_thread_data *data, BRect frame, const char *name):
	BView(frame, name, B_FOLLOW_NONE, B_SUBPIXEL_PRECISE),
	fHwnd(hwnd),
	fData(data),
	fBitmap(NULL),
	fOldMouseBtns(0)
{
}

void WineView::UpdateBitmap(BBitmap *bitmap, BRect dirty)
{
	if (bitmap != fBitmap) {
		fBitmap = bitmap;
		SetViewBitmap(fBitmap, B_FOLLOW_LEFT_TOP, 0);
	}
	Invalidate(dirty);
}

void WineView::MessageReceived(BMessage *msg)
{
	if (sCaptureWnd != NULL && sCaptureWnd != fHwnd) {
		return BView::MessageReceived(msg);
	}
	switch (msg->what) {
		case B_KEY_DOWN:
		case B_KEY_UP:
		case B_UNMAPPED_KEY_DOWN:
		case B_UNMAPPED_KEY_UP: {
			HWND hwnd = fHwnd;
			//msg->FindPointer("hwnd", (void**)&hwnd);
			int32 key;
			msg->FindInt32("key", &key);

	    INPUT input;
	    input.type           = INPUT_KEYBOARD;
	    input.ki.dwFlags     = (msg->what == B_KEY_UP || msg->what == B_UNMAPPED_KEY_UP) ? KEYEVENTF_KEYUP : 0;
	    input.ki.time        = GetTickCount();
	    input.ki.dwExtraInfo = 0;
			FromHaikuKeyCode(input.ki.wVk, input.ki.wScan, key);

			if (input.ki.wVk != 0) {
				UnlockLooper();
				__wine_send_input(hwnd, &input, NULL);
				LockLooper();
			}
			return;
		}
		case B_MOUSE_DOWN:
		case B_MOUSE_UP:
		case B_MOUSE_MOVED: {
			HWND hwnd = fHwnd;
			//msg->FindPointer("hwnd", (void**)&hwnd);
			BPoint where;
			uint32 btns;
			msg->FindPoint("screen_where", &where);
			msg->FindInt32("buttons", (int32*)&btns);
			//msg->FindInt32("old_buttons", (int32*)&oldBtns);
			uint32 oldBtns = (msg->what == B_MOUSE_MOVED) ? btns : fOldMouseBtns;
			fOldMouseBtns = btns;
			uint32 btnsDown = btns & (~oldBtns);
			uint32 btnsUp = oldBtns & (~btns);

			INPUT input;
			input.type = INPUT_MOUSE;
			input.mi.dx          = (int)where.x;
			input.mi.dy          = (int)where.y;
			input.mi.mouseData   = 0;
			input.mi.dwFlags     = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
			input.mi.time        = GetTickCount();
			input.mi.dwExtraInfo = 0;

			if ((1 << 0) & btnsDown) input.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
			if ((1 << 1) & btnsDown) input.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
			if ((1 << 2) & btnsDown) input.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
			if ((1 << 0) & btnsUp)   input.mi.dwFlags |= MOUSEEVENTF_LEFTUP;
			if ((1 << 1) & btnsUp)   input.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;
			if ((1 << 2) & btnsUp)   input.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;

			UnlockLooper();
			__wine_send_input(hwnd, &input, NULL);
			LockLooper();
			return;
		}
		case B_MOUSE_WHEEL_CHANGED: {
			HWND hwnd;
			msg->FindPointer("hwnd", (void**)&hwnd);
			float dx, dy;
			if (msg->FindFloat("be:wheel_delta_x", &dx) < B_OK) dx = 0;
			if (msg->FindFloat("be:wheel_delta_y", &dy) < B_OK) dy = 0;
			int dxInt = int(-dx*WHEEL_DELTA);
			int dyInt = int(-dy*WHEEL_DELTA);
			DWORD time = GetTickCount();
			if (dxInt != 0) {
				INPUT input;
				input.type = INPUT_MOUSE;
				input.mi.dx          = 0;
				input.mi.dy          = 0;
				input.mi.mouseData   = dxInt;
				input.mi.dwFlags     = MOUSEEVENTF_HWHEEL;
				input.mi.time        = time;
				input.mi.dwExtraInfo = 0;
				UnlockLooper();
				__wine_send_input(hwnd, &input, NULL);
				LockLooper();
			}
			if (dyInt != 0) {
				INPUT input;
				input.type = INPUT_MOUSE;
				input.mi.dx          = 0;
				input.mi.dy          = 0;
				input.mi.mouseData   = dyInt;
				input.mi.dwFlags     = MOUSEEVENTF_WHEEL;
				input.mi.time        = time;
				input.mi.dwExtraInfo = 0;
				UnlockLooper();
				__wine_send_input(hwnd, &input, NULL);
				LockLooper();
			}
			return;
		}
	}
/*
	switch (msg->what) {
		case B_KEY_DOWN:
		case B_KEY_UP:
		case B_UNMAPPED_KEY_DOWN:
		case B_UNMAPPED_KEY_UP: {
			msg->AddPointer("hwnd", fHwnd);
			be_app_messenger.SendMessage(msg);
			return;
		}
		case B_MOUSE_DOWN:
		case B_MOUSE_UP:
		case B_MOUSE_MOVED: {
			msg->AddPointer("hwnd", fHwnd);
			msg->AddInt32("old_buttons", (int32)fOldMouseBtns);
			msg->FindInt32("buttons", (int32*)&fOldMouseBtns);
			be_app_messenger.SendMessage(msg);
			return;
		}
		case B_MOUSE_WHEEL_CHANGED: {
			msg->AddPointer("hwnd", fHwnd);				
			be_app_messenger.SendMessage(msg);
			return;
		}
	}
*/
	BView::MessageReceived(msg);
}


WineWindow::WineWindow(HWND hwnd, struct haikudrv_thread_data *data, BRect frame):
	BWindow(frame, "TestApp", B_NO_BORDER_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL, B_ASYNCHRONOUS_CONTROLS),
	fHwnd(hwnd),
	fData(data),
	fCreateMutex(MUTEX_INITIALIZER("create window")),
	fSurface(NULL),
	fSendResizeEvents(false),
	fInDestroy(false),
	fNonClient({0, 0, 0, 0})
{
	fView = new WineView(fHwnd, fData, Frame().OffsetToCopy(B_ORIGIN), "view");
	fView->SetResizingMode(B_FOLLOW_ALL);
	AddChild(fView, NULL);
	fView->MakeFocus();
}

WineWindow::~WineWindow()
{}

bool WineWindow::QuitRequested()
{
	if (fInDestroy) return true;
	Unlock();
	HWND hwnd = fHwnd;
   if (IsWindowEnabled(hwnd))
   {
       HMENU hSysMenu;

       if (GetClassLongW(hwnd, GCL_STYLE) & CS_NOCLOSE) {
         Lock();
       	return false;
       }
       hSysMenu = GetSystemMenu(hwnd, FALSE);
       if (hSysMenu)
       {
           UINT state = GetMenuState(hSysMenu, SC_CLOSE, MF_BYCOMMAND);
           if (state == 0xFFFFFFFF || (state & (MF_DISABLED | MF_GRAYED))) {
               Lock();
               return false;
           }
       }
#if 0
       if (GetActiveWindow() != hwnd)
       {
           LRESULT ma = SendMessageW( hwnd, WM_MOUSEACTIVATE,
                                      (WPARAM)GetAncestor( hwnd, GA_ROOT ),
                                      MAKELPARAM( HTCLOSE, WM_NCLBUTTONDOWN ) );
           switch(ma)
           {
               case MA_NOACTIVATEANDEAT:
               case MA_ACTIVATEANDEAT:
                   Lock();
                   return false;
               case MA_NOACTIVATE:
                   break;
               case MA_ACTIVATE:
               case 0:
                   SetActiveWindow(hwnd);
                   break;
               default:
                   WARN( "unknown WM_MOUSEACTIVATE code %d\n", (int) ma );
                   break;
           }
       }
#endif
       PostMessageW( hwnd, WM_SYSCOMMAND, SC_CLOSE, 0 );
   }
	Lock();
	return false;
}

void WineWindow::FrameMoved(BPoint newPosition)
{
	if (!fSendResizeEvents) return;
	Unlock();
	SetWindowPos(fHwnd, 0, (int)newPosition.x - fNonClient.left, (int)newPosition.y - fNonClient.top, 0, 0, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE);
	Lock();
}

void WineWindow::FrameResized(float newWidth, float newHeight)
{
	if (!fSendResizeEvents) return;
	Unlock();
	SetWindowPos(fHwnd, 0, 0, 0, (int)newWidth + 1 + fNonClient.left + fNonClient.right, (int)newHeight + 1 + fNonClient.top + fNonClient.bottom, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
	Lock();
}


WineApplication::WineApplication(): BApplication("application/x-vnd.Test-Wine")
{
	InitKeymap();
}

//#pragma mark -


DWORD sAppThread = 0;
std::map<HWND, WineWindow*> *sWindows;

static UINT CALLBACK AppThread(void *arg)
{
	rename_thread(find_thread(NULL), "application");
	be_app->Lock();
	be_app->Run();
	return 0;
}

BOOL HaikuStartApplication()
{
	if (be_app != NULL) {
		ERR("be_app != NULL");
		_exit(1);
	}
	
	sWindows = new std::map<HWND, WineWindow*>();

	new WineApplication();
	be_app->Unlock();
	CloseHandle(CreateThread(NULL, 0, AppThread, NULL, 0, &sAppThread));
	return TRUE;
}

static UINT CALLBACK WindowThread(void *arg)
{
	rename_thread(find_thread(NULL), "window");
	auto wnd = (WineWindow*)arg;
	wnd->Lock();
	mutex_unlock(&wnd->fCreateMutex);
	wnd->Loop();
	return 0;
}

static WineWindow* HaikuThisWindow(HWND hwnd, bool create = true)
{
	BAutolock lock(be_app);
	auto it = sWindows->find(hwnd);
	if (it != sWindows->end())
		return it->second;
	
	if (!create)
		return NULL;

	//struct haikudrv_thread_data *data = haikudrv_init_thread_data();
	WineWindow *window = new WineWindow(hwnd, NULL, BRect());
	(*sWindows)[hwnd] = window;
	mutex_lock(&window->fCreateMutex);
	window->Unlock();
	CloseHandle(CreateThread(NULL, 0, WindowThread, window, 0, &sAppThread));
	mutex_lock(&window->fCreateMutex);
	return window;
}


//#pragma mark - window_surface

struct haikudrv_window_surface: public window_surface {
    RECT                  bounds;
    void                 *bits;
    BBitmap              *bitmap;
    WineWindow           *window;
    CRITICAL_SECTION      crit;
    BITMAPINFO            info;   /* variable size, must be last */
};

static struct haikudrv_window_surface *get_x11_surface(struct window_surface *surface)
{
    return (struct haikudrv_window_surface *)surface;
}

static inline int get_dib_info_size(const BITMAPINFO *info, UINT coloruse)
{
	return FIELD_OFFSET( BITMAPINFO, bmiColors[0] );
/*
	if (info->bmiHeader.biCompression == BI_BITFIELDS)
		return sizeof(BITMAPINFOHEADER) + 3 * sizeof(DWORD);
	if (coloruse == DIB_PAL_COLORS)
		return sizeof(BITMAPINFOHEADER) + info->bmiHeader.biClrUsed * sizeof(WORD);
	if (!info->bmiHeader.biClrUsed && info->bmiHeader.biBitCount <= 8)
		return FIELD_OFFSET( BITMAPINFO, bmiColors[1 << info->bmiHeader.biBitCount] );
	return FIELD_OFFSET( BITMAPINFO, bmiColors[info->bmiHeader.biClrUsed] );
*/
}


static void CDECL haikudrv_surface_lock(struct window_surface *window_surface)
{
	struct haikudrv_window_surface *surface = get_x11_surface(window_surface);
	EnterCriticalSection(&surface->crit);
}

static void CDECL haikudrv_surface_unlock(struct window_surface *window_surface)
{
	struct haikudrv_window_surface *surface = get_x11_surface(window_surface);
	LeaveCriticalSection(&surface->crit);
}

static void *CDECL haikudrv_surface_get_bitmap_info(struct window_surface *window_surface, BITMAPINFO *info)
{
	struct haikudrv_window_surface *surface = get_x11_surface( window_surface );
	
	memcpy( info, &surface->info, get_dib_info_size(&surface->info, DIB_RGB_COLORS));
	return surface->bits;
}

static RECT *CDECL haikudrv_surface_get_bounds(struct window_surface *window_surface)
{
	struct haikudrv_window_surface *surface = get_x11_surface(window_surface);
	return &surface->bounds;
}

static void CDECL haikudrv_surface_set_region(struct window_surface *window_surface, HRGN region)
{
	struct haikudrv_window_surface *surface = get_x11_surface(window_surface);
	
	window_surface->funcs->lock(window_surface);
	// ...
	window_surface->funcs->unlock(window_surface);
}

static void CDECL haikudrv_surface_flush(struct window_surface *window_surface)
{
	struct haikudrv_window_surface *surface = get_x11_surface(window_surface);
	
	window_surface->funcs->lock(window_surface);
	//FIXME("(): stub\n");
	RECT dirty;
	SetRect(&dirty, 0, 0, surface->rect.right - surface->rect.left, surface->rect.bottom - surface->rect.top);
	if (IntersectRect( &dirty, &dirty, &surface->bounds )) {
		surface->window->Lock();
		BRect dirtyRect(dirty.left, dirty.top, dirty.right - 1, dirty.bottom - 1);
		surface->window->View()->UpdateBitmap(surface->bitmap, dirtyRect);
		surface->window->Unlock();
	}
	reset_bounds(&surface->bounds);
	window_surface->funcs->unlock(window_surface);
}

static void CDECL haikudrv_surface_destroy(struct window_surface *window_surface)
{
	struct haikudrv_window_surface *surface = get_x11_surface( window_surface );
	
	delete surface->bitmap; surface->bitmap = NULL;
	surface->crit.DebugInfo->Spare[0] = 0;
	DeleteCriticalSection(&surface->crit);
	HeapFree(GetProcessHeap(), 0, surface);
}

static const struct window_surface_funcs haikudrv_surface_funcs = {
	haikudrv_surface_lock,
	haikudrv_surface_unlock,
	haikudrv_surface_get_bitmap_info,
	haikudrv_surface_get_bounds,
	haikudrv_surface_set_region,
	haikudrv_surface_flush,
	haikudrv_surface_destroy
};

struct window_surface *create_surface(WineWindow *window, const RECT *rect, COLORREF color_key, BOOL use_alpha)
{
	struct haikudrv_window_surface *surface;
	int width = rect->right - rect->left, height = rect->bottom - rect->top;

	surface = (haikudrv_window_surface*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, FIELD_OFFSET(struct haikudrv_window_surface, info.bmiColors[0]));
	if (!surface) return NULL;

	surface->window = window;

	surface->info.bmiHeader.biSize        = sizeof(surface->info.bmiHeader);
	surface->info.bmiHeader.biWidth       = width;
	surface->info.bmiHeader.biHeight      = -height; /* top-down */
	surface->info.bmiHeader.biPlanes      = 1;
	surface->info.bmiHeader.biBitCount    = 32;
	surface->info.bmiHeader.biSizeImage   = 4*width*height;
	surface->info.bmiHeader.biCompression = BI_RGB;
	surface->info.bmiHeader.biClrUsed = 0;

	InitializeCriticalSection( &surface->crit );
	surface->crit.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": surface");

	surface->funcs = &haikudrv_surface_funcs;
	surface->rect  = *rect;
	surface->ref   = 1;
	reset_bounds(&surface->bounds);

	surface->bitmap = new BBitmap(BRect(0, 0, width - 1, height - 1), B_RGBA32);
	if (!surface->bitmap) {
		goto failed;
	}
	surface->bits = surface->bitmap->Bits();

	return surface;

failed:
	haikudrv_surface_destroy(surface);
	return NULL;
}


//#pragma mark -

/***********************************************************************
 *           ThreadDetach (HAIKUDRV.@)
 */
void CDECL HAIKUDRV_ThreadDetach(void)
{
#if 0
    struct haikudrv_thread_data *data = (struct haikudrv_thread_data*)TlsGetValue( thread_data_tls_index );

    if (data)
    {
        delete data;
        /* clear data in case we get re-entered from user32 before the thread is truly dead */
        TlsSetValue( thread_data_tls_index, NULL );
    }
#endif
}


/***********************************************************************
 *           HAIKUDRV thread initialisation routine
 */
struct haikudrv_thread_data *haikudrv_init_thread_data(void)
{
    struct haikudrv_thread_data *data = haikudrv_thread_data();

    if (data) return data;

    if (!(data = new(std::nothrow) struct haikudrv_thread_data()))
    {
        ERR( "could not create data\n" );
        ExitProcess(1);
    }

    TlsSetValue( thread_data_tls_index, data );

    return data;
}

//#pragma mark -

struct has_popup_result
{
    HWND hwnd;
    BOOL found;
};

static BOOL is_managed( HWND hwnd )
{
    WineWindow *wnd = HaikuThisWindow(hwnd, false);
    return wnd != NULL;
}

static BOOL CALLBACK has_managed_popup( HWND hwnd, LPARAM lparam )
{
    struct has_popup_result *result = (struct has_popup_result *)lparam;

    if (hwnd == result->hwnd) return FALSE;  /* popups are always above owner */
    if (GetWindow( hwnd, GW_OWNER ) != result->hwnd) return TRUE;
    result->found = is_managed( hwnd );
    return !result->found;
}

static BOOL has_owned_popups( HWND hwnd )
{
    struct has_popup_result result;

    result.hwnd = hwnd;
    result.found = FALSE;
    EnumWindows( has_managed_popup, (LPARAM)&result );
    return result.found;
}

static BOOL is_window_managed( HWND hwnd, UINT swp_flags, const RECT *window_rect )
{
    DWORD style, ex_style;

    /* child windows are not managed */
    style = GetWindowLongW( hwnd, GWL_STYLE );
    if ((style & (WS_CHILD|WS_POPUP)) == WS_CHILD) return FALSE;
    return TRUE;

    /* activated windows are managed */
    if (!(swp_flags & (SWP_NOACTIVATE|SWP_HIDEWINDOW))) return TRUE;
    if (hwnd == GetActiveWindow()) return TRUE;
    /* windows with caption are managed */
    if ((style & WS_CAPTION) == WS_CAPTION) return TRUE;
    /* windows with thick frame are managed */
    if (style & WS_THICKFRAME) return TRUE;
    if (style & WS_POPUP) {
        HMONITOR hmon;
        MONITORINFO mi;

        /* popup with sysmenu == caption are managed */
        if (style & WS_SYSMENU) return TRUE;
        /* full-screen popup windows are managed */
        hmon = MonitorFromWindow( hwnd, MONITOR_DEFAULTTOPRIMARY );
        mi.cbSize = sizeof( mi );
        GetMonitorInfoW( hmon, &mi );
        if (window_rect->left <= mi.rcWork.left && window_rect->right >= mi.rcWork.right &&
            window_rect->top <= mi.rcWork.top && window_rect->bottom >= mi.rcWork.bottom)
            return TRUE;
    }
    /* application windows are managed */
    ex_style = GetWindowLongW( hwnd, GWL_EXSTYLE );
    if (ex_style & WS_EX_APPWINDOW) return TRUE;
    /* windows that own popups are managed */
    if (has_owned_popups( hwnd )) return TRUE;
    /* default: not managed */
    return FALSE;
}



/**********************************************************************
 *              is_owned_by
 */
static BOOL is_owned_by(HWND hwnd, HWND maybe_owner)
{
    while (1)
    {
        HWND hwnd2 = GetWindow(hwnd, GW_OWNER);
        if (!hwnd2)
            hwnd2 = GetAncestor(hwnd, GA_ROOT);
        if (!hwnd2 || hwnd2 == hwnd)
            break;
        if (hwnd2 == maybe_owner)
            return TRUE;
        hwnd = hwnd2;
    }

    return FALSE;
}


/**********************************************************************
 *              is_all_the_way_front
 */
static BOOL is_all_the_way_front(HWND hwnd)
{
    BOOL topmost = (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    HWND prev = hwnd;

    while ((prev = GetWindow(prev, GW_HWNDPREV)))
    {
        if (!topmost && (GetWindowLongW(prev, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0)
            return TRUE;
        if (!is_owned_by(prev, hwnd))
            return FALSE;
    }

    return TRUE;
}

static inline BOOL get_surface_rect( const RECT *visible_rect, RECT *surface_rect )
{
    *surface_rect = (RECT){0, 0, 4096, 4096};

    if (!IntersectRect( surface_rect, surface_rect, visible_rect )) return FALSE;
    OffsetRect( surface_rect, -visible_rect->left, -visible_rect->top );
    surface_rect->left &= ~31;
    surface_rect->top  &= ~31;
    surface_rect->right  = max( surface_rect->left + 32, (surface_rect->right + 31) & ~31 );
    surface_rect->bottom = max( surface_rect->top + 32, (surface_rect->bottom + 31) & ~31 );
    return TRUE;
}

static RECT get_virtual_screen_rect()
{
	BRect frame = BScreen().Frame();
	return RECT{
		.left   = (int)frame.left,
		.top    = (int)frame.top,
		.right  = (int)frame.right + 1,
		.bottom = (int)frame.bottom + 1,
	};
}

static void GetHaikuWindowFlags(
	uint32 &flags,
	window_look &look,
	window_feel &feel,
	WineWindow *wnd,
	DWORD style, DWORD ex_style,
	const RECT *window_rect,
	const RECT *client_rect
)
{
    flags = B_NOT_CLOSABLE | B_NOT_MINIMIZABLE | B_NOT_ZOOMABLE | B_NOT_RESIZABLE |
    	B_ASYNCHRONOUS_CONTROLS | B_WILL_ACCEPT_FIRST_CLICK;
    look = B_NO_BORDER_WINDOW_LOOK;
    feel = B_NORMAL_WINDOW_FEEL;

    //if (disable_window_decorations) return;
    if (IsRectEmpty(window_rect)) return;
    if (EqualRect(window_rect, client_rect)) return;

    if ((style & WS_CAPTION) == WS_CAPTION && !(ex_style & WS_EX_LAYERED))
    {
        look = B_MODAL_WINDOW_LOOK;
        if (true/*!data->shaped*/)
        {
            look = B_TITLED_WINDOW_LOOK;
            if (style & WS_SYSMENU) flags &= ~B_NOT_CLOSABLE;
            if (style & WS_MINIMIZEBOX) flags &= ~B_NOT_MINIMIZABLE;
            if (style & WS_MAXIMIZEBOX) flags &= ~B_NOT_ZOOMABLE;
            if (ex_style & WS_EX_TOOLWINDOW) look = B_FLOATING_WINDOW_LOOK;
        }
    }
    if (style & WS_THICKFRAME)
    {
        if (look == B_NO_BORDER_WINDOW_LOOK) look = B_MODAL_WINDOW_LOOK;
        if (true/*!data->shaped*/) flags &= ~B_NOT_RESIZABLE;
    }
    else if (ex_style & WS_EX_DLGMODALFRAME) {if (look == B_NO_BORDER_WINDOW_LOOK) look = B_MODAL_WINDOW_LOOK;}
    else if ((style & (WS_DLGFRAME|WS_BORDER)) == WS_DLGFRAME) {if (look == B_NO_BORDER_WINDOW_LOOK) look = B_MODAL_WINDOW_LOOK;}
}


/*****************************************************************
 *		SetWindowText   (HAIKUDRV.@)
 */
void CDECL HAIKUDRV_SetWindowText( HWND hwnd, LPCWSTR text )
{
	UINT count;
	char *utf8_buffer;
	count = WideCharToMultiByte(CP_UTF8, 0, text, strlenW(text), NULL, 0, NULL, NULL);
	if (!(utf8_buffer = (char*)HeapAlloc( GetProcessHeap(), 0, count + 1 ))) {
		return;
	}
	WideCharToMultiByte(CP_UTF8, 0, text, strlenW(text), utf8_buffer, count, NULL, NULL);
	utf8_buffer[count] = 0;

	//FIXME("(%ld, \"%s\"): stub\n", (long)hwnd, utf8_buffer);

	WineWindow *wnd = HaikuThisWindow(hwnd, false);
	if (wnd == NULL) return;
	wnd->Lock();
	wnd->SetTitle(utf8_buffer);
	wnd->Unlock();

	HeapFree( GetProcessHeap(), 0, utf8_buffer );
}


/***********************************************************************
 *		SetWindowStyle   (HAIKUDRV.@)
 *
 * Update the X state of a window to reflect a style change
 */
void CDECL HAIKUDRV_SetWindowStyle( HWND hwnd, INT offset, STYLESTRUCT *style )
{
	FIXME("(%ld): stub\n", (long)hwnd);
}


/***********************************************************************
 *		DestroyWindow   (HAIKUDRV.@)
 */
void CDECL HAIKUDRV_DestroyWindow( HWND hwnd )
{
	//FIXME("(%ld): stub\n", (long)hwnd);
	WineWindow *wnd = HaikuThisWindow(hwnd, false);
	if (wnd == NULL) return;
	wnd->Lock();
	wnd->fInDestroy = true;
	BMessenger(wnd).SendMessage(B_QUIT_REQUESTED);
	wnd->Unlock();
}


/**********************************************************************
 *		CreateDesktopWindow   (HAIKUDRV.@)
 */
BOOL CDECL HAIKUDRV_CreateDesktopWindow( HWND hwnd )
{
	//FIXME("(%ld): stub\n", (long)hwnd);
    RECT oldRect;

    /* retrieve the real size of the desktop */
    SERVER_START_REQ( get_window_rectangles )
    {
        req->handle = wine_server_user_handle( hwnd );
        req->relative = COORDS_CLIENT;
        wine_server_call( req );
        oldRect.left   = reply->window.left;
        oldRect.top    = reply->window.top;
        oldRect.right  = reply->window.right;
        oldRect.bottom = reply->window.bottom;
    }
    SERVER_END_REQ;

    RECT rect = get_virtual_screen_rect();
    ERR("desktop rect: (%d, %d, %d, %d)\n", rect.left, rect.top, rect.right, rect.bottom);
    if (!EqualRect(&oldRect, &rect))
    {

        SERVER_START_REQ( set_window_pos )
        {
            req->handle        = wine_server_user_handle( hwnd );
            req->previous      = 0;
            req->swp_flags     = SWP_NOZORDER;
            req->window.left   = rect.left;
            req->window.top    = rect.top;
            req->window.right  = rect.right;
            req->window.bottom = rect.bottom;
            req->client        = req->window;
            wine_server_call( req );
        }
        SERVER_END_REQ;
    }
    return TRUE;
}


/**********************************************************************
 *		CreateWindow   (HAIKUDRV.@)
 */
BOOL CDECL HAIKUDRV_CreateWindow( HWND hwnd )
{
	FIXME("(%ld): stub\n", (long)hwnd);
	return TRUE;
}


/***********************************************************************
 *		HAIKUDRV_GetDC   (HAIKUDRV.@)
 */
void CDECL HAIKUDRV_GetDC( HDC hdc, HWND hwnd, HWND top, const RECT *win_rect,
                         const RECT *top_rect, DWORD flags )
{
	//FIXME("(%ld): stub\n", (long)hwnd);
}


/***********************************************************************
 *		HAIKUDRV_ReleaseDC  (HAIKUDRV.@)
 */
void CDECL HAIKUDRV_ReleaseDC( HWND hwnd, HDC hdc )
{
	//FIXME("(%ld): stub\n", (long)hwnd);
}


/*************************************************************************
 *		ScrollDC   (HAIKUDRV.@)
 */
BOOL CDECL HAIKUDRV_ScrollDC( HDC hdc, INT dx, INT dy, HRGN update )
{
	FIXME("(%ld): stub\n", (long)hdc);
	return FALSE;
}


/***********************************************************************
 *		SetCapture  (HAIKUDRV.@)
 */
void CDECL HAIKUDRV_SetCapture( HWND hwnd, UINT flags )
{
	FIXME("(%ld, %x): stub\n", (long)hwnd, flags);
	return;
	if (hwnd == sCaptureWnd) return;
	WineWindow* prevWnd = HaikuThisWindow(sCaptureWnd, false);
	if (prevWnd != NULL) {
		prevWnd->Lock();
		prevWnd->View()->SetEventMask(0);
		prevWnd->Unlock();
	}
	WineWindow* nextWnd = HaikuThisWindow(hwnd, false);
	if (nextWnd != NULL) {
		nextWnd->Lock();
		nextWnd->View()->SetEventMask(B_POINTER_EVENTS | B_KEYBOARD_EVENTS);
		nextWnd->Unlock();
	}
}


/*****************************************************************
 *		SetParent   (HAIKUDRV.@)
 */
void CDECL HAIKUDRV_SetParent( HWND hwnd, HWND parent, HWND old_parent )
{
	FIXME("(%ld): stub\n", (long)hwnd);
}


/***********************************************************************
 *		WindowPosChanging   (HAIKUDRV.@)
 */
BOOL CDECL HAIKUDRV_WindowPosChanging( HWND hwnd, HWND insert_after, UINT swp_flags,
                                     const RECT *window_rect, const RECT *client_rect, RECT *visible_rect,
                                     struct window_surface **surface )
{
	//FIXME("(%ld): stub\n", (long)hwnd);
	ERR("(%ld)\n", (long)hwnd);
	
	if (hwnd == GetDesktopWindow()) return TRUE;
	if (!is_window_managed( hwnd, swp_flags, window_rect )) return TRUE;
	
	ERR("window_rect: (%d, %d, %d, %d)\n", window_rect->left, window_rect->top, window_rect->right, window_rect->bottom);
	ERR("client_rect: (%d, %d, %d, %d)\n", client_rect->left, client_rect->top, client_rect->right, client_rect->bottom);
	
	DWORD style = GetWindowLongW(hwnd, GWL_STYLE);
	DWORD exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
	RECT adjRect = *client_rect;
	AdjustWindowRectEx(&adjRect, style, FALSE, exStyle);
	adjRect = {
		client_rect->left - adjRect.left,
		client_rect->top - adjRect.top,
		adjRect.right - client_rect->right,
		adjRect.bottom - client_rect->bottom
	};
	*visible_rect = {
		window_rect->left + adjRect.left,
		window_rect->top + adjRect.top,
		window_rect->right - adjRect.right,
		window_rect->bottom - adjRect.bottom,
	};

	RECT surface_rect = {
		0, 0,
		visible_rect->right - visible_rect->left,
		visible_rect->bottom - visible_rect->top,
	};
	//if (!get_surface_rect( visible_rect, &surface_rect )) return FALSE;

	WineWindow *window = HaikuThisWindow(hwnd);

	if (window->fSurface) {
		if (EqualRect( &window->fSurface->rect, &surface_rect )) {
			/* existing surface is good enough */
			window_surface_add_ref( window->fSurface );
			*surface = window->fSurface;
			return TRUE;
		}
	}

	*surface = create_surface(window, &surface_rect, 0, FALSE);
	return TRUE;
}


/***********************************************************************
 *		WindowPosChanged   (HAIKUDRV.@)
 */
void CDECL HAIKUDRV_WindowPosChanged( HWND hwnd, HWND insert_after, UINT swp_flags,
                                    const RECT *rectWindow, const RECT *rectClient,
                                    const RECT *visible_rect, const RECT *valid_rects,
                                    struct window_surface *surface )
{
	ERR("(%ld)\n", (long)hwnd);
  WineWindow *window = HaikuThisWindow(hwnd, false);
  if (window == NULL) return;

 	ERR("(%p, swp_flags: %x)\n", hwnd, swp_flags);

  DWORD style = GetWindowLongW(hwnd, GWL_STYLE);
  DWORD exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);

  window->Lock();
  window->fNonClient = {
		visible_rect->left - rectWindow->left,
		visible_rect->top  - rectWindow->top,
		rectWindow->right  - visible_rect->right,
		rectWindow->bottom - visible_rect->bottom
  };
  window->MoveTo(visible_rect->left, visible_rect->top);
  window->ResizeTo(visible_rect->right - visible_rect->left - 1, visible_rect->bottom - visible_rect->top - 1);
  
  uint32 newFlags;
  window_look newLook;
  window_feel newFeel;
  GetHaikuWindowFlags(newFlags, newLook, newFeel, window, style, exStyle, rectWindow, rectClient);
  if (window->Flags() != newFlags) window->SetFlags(newFlags);
  if (window->Look() != newLook) window->SetLook(newLook);
  if (window->Feel() != newFeel) window->SetFeel(newFeel);

  bool newVisible = (style & WS_VISIBLE) != 0;
  if (newVisible != !window->IsHidden()) {
  	if (newVisible) {
  		if (SWP_NOACTIVATE & swp_flags) window->SetFlags(window->Flags() | B_AVOID_FOCUS);
	  	window->Show();
  		if (SWP_NOACTIVATE & swp_flags) window->SetFlags(window->Flags() & ~B_AVOID_FOCUS);
	  	window->fSendResizeEvents = true;
  	} else {
	  	window->Hide();
	  	window->fSendResizeEvents = false;
  	}
  } else {
	  if (!window->IsHidden() && !(SWP_NOACTIVATE & swp_flags)) {
	  	ERR("(%p): Activate\n", hwnd);
	  	window->Activate();
	  }
  }
  window->Unlock();

	//FIXME("(%ld): stub\n", (long)hwnd);
}

/***********************************************************************
 *           ShowWindow   (HAIKUDRV.@)
 */
UINT CDECL HAIKUDRV_ShowWindow( HWND hwnd, INT cmd, RECT *rect, UINT swp )
{
	FIXME("(%ld): stub\n", (long)hwnd);
	return swp;
}


/**********************************************************************
 *		SetWindowIcon (HAIKUDRV.@)
 *
 * hIcon or hIconSm has changed (or is being initialised for the
 * first time). Complete the X11 driver-specific initialisation
 * and set the window hints.
 */
void CDECL HAIKUDRV_SetWindowIcon( HWND hwnd, UINT type, HICON icon )
{
	FIXME("(%ld): stub\n", (long)hwnd);
}


/***********************************************************************
 *		SetWindowRgn  (HAIKUDRV.@)
 *
 * Assign specified region to window (for non-rectangular windows)
 */
void CDECL HAIKUDRV_SetWindowRgn( HWND hwnd, HRGN hrgn, BOOL redraw )
{
	FIXME("(%ld): stub\n", (long)hwnd);
}


/***********************************************************************
 *		SetLayeredWindowAttributes  (HAIKUDRV.@)
 *
 * Set transparency attributes for a layered window.
 */
void CDECL HAIKUDRV_SetLayeredWindowAttributes( HWND hwnd, COLORREF key, BYTE alpha, DWORD flags )
{
	FIXME("(%ld): stub\n", (long)hwnd);
}


/*****************************************************************************
 *              UpdateLayeredWindow  (HAIKUDRV.@)
 */
BOOL CDECL HAIKUDRV_UpdateLayeredWindow( HWND hwnd, const UPDATELAYEREDWINDOWINFO *info,
                                       const RECT *window_rect )
{
	FIXME("(%ld): stub\n", (long)hwnd);
	return TRUE;
}

/**********************************************************************
 *           HAIKUDRV_WindowMessage   (HAIKUDRV.@)
 */
LRESULT CDECL HAIKUDRV_WindowMessage( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	FIXME("(%ld): stub\n", (long)hwnd);
	return 0;
}

/***********************************************************************
 *           HAIKUDRV_SysCommand   (HAIKUDRV.@)
 *
 * Perform WM_SYSCOMMAND handling.
 */
LRESULT CDECL HAIKUDRV_SysCommand( HWND hwnd, WPARAM wparam, LPARAM lparam )
{
	FIXME("(%ld): stub\n", (long)hwnd);
	return -1;
}

/***********************************************************************
 *           HAIKUDRV_FlashWindowEx   (HAIKUDRV.@)
 */
void CDECL HAIKUDRV_FlashWindowEx( PFLASHWINFO pfinfo )
{
}

/***********************************************************************
 *           MsgWaitForMultipleObjectsEx   (HAIKUDRV.@)
 */
DWORD CDECL HAIKUDRV_MsgWaitForMultipleObjectsEx( DWORD count, const HANDLE *handles,
                                                DWORD timeout, DWORD mask, DWORD flags )
{
	timeout = timeout < 1000 ? timeout : 1000;
	//struct haikudrv_thread_data *data = haikudrv_init_thread_data();
	//if (data == NULL) {
		if (!count && !timeout) return WAIT_TIMEOUT;
		return WaitForMultipleObjectsEx(count, handles, flags & MWMO_WAITALL, timeout, flags & MWMO_ALERTABLE);
	//}
/*
	data->ProcessMessages();

	if (!count && !timeout) return WAIT_TIMEOUT;
	return WaitForMultipleObjectsEx(count, handles, flags & MWMO_WAITALL, timeout, flags & MWMO_ALERTABLE);
*/
}

/*****************************************************************
 *		SetFocus   (HAIKUDRV.@)
 *
 * Set the X focus.
 */
void CDECL HAIKUDRV_SetFocus( HWND hwnd )
{
	FIXME("(%ld)\n", (long)hwnd);
	if (!(hwnd = GetAncestor(hwnd, GA_ROOT))) return;

	if (hwnd == GetForegroundWindow() && hwnd != GetDesktopWindow() && !is_all_the_way_front(hwnd))
		SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

  WineWindow *window = HaikuThisWindow(hwnd, false);
  if (window == NULL) return;
  window->Activate();
}


/***********************************************************************
 *		ToUnicodeEx (HAIKUDRV.@)
 *
 * The ToUnicode function translates the specified virtual-key code and keyboard
 * state to the corresponding Windows character or characters.
 *
 * If the specified key is a dead key, the return value is negative. Otherwise,
 * it is one of the following values:
 * Value	Meaning
 * 0	The specified virtual key has no translation for the current state of the keyboard.
 * 1	One Windows character was copied to the buffer.
 * 2	Two characters were copied to the buffer. This usually happens when a
 *      dead-key character (accent or diacritic) stored in the keyboard layout cannot
 *      be composed with the specified virtual key to form a single character.
 *
 * FIXME : should do the above (return 2 for non matching deadchar+char combinations)
 *
 */
INT CDECL HAIKUDRV_ToUnicodeEx(UINT virtKey, UINT scanCode, const BYTE *lpKeyState,
                             LPWSTR bufW, int bufW_size, UINT flags, HKL hkl)
{
    //FIXME("(%u): stub\n", virtKey);
    if (virtKey >= 256) return -2;
    uint32 key = sHaikuFromVkKeycodes[virtKey];
    uint32 modifiers = HaikuModifiersFromKeyState(lpKeyState);

    char *srcChars;
    size_t srcLen;
    MapKey(srcChars, srcLen, key, modifiers);

		INT dstLen = MultiByteToWideChar(CP_UTF8, 0, srcChars, srcLen, NULL, 0);
		MultiByteToWideChar(CP_UTF8, 0, srcChars, srcLen, bufW, bufW_size);
    return dstLen;
}
