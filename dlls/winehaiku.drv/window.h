#pragma once

#define thread_info haiku_thread_info
#include <Application.h>
#include <Window.h>
#include <Bitmap.h>
#include <OS.h>
#include <locks.h>
#undef thread_info

extern "C" {
#include "haikudrv.h"
}
#undef GetMonitorInfo

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


WineWindow* HaikuThisWindow(HWND hwnd, bool create = true);
