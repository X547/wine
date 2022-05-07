# WinTab32
@ cdecl AttachEventQueueToTablet(long) HAIKUDRV_AttachEventQueueToTablet
@ cdecl GetCurrentPacket(ptr) HAIKUDRV_GetCurrentPacket
@ cdecl LoadTabletInfo(long) HAIKUDRV_LoadTabletInfo
@ cdecl WTInfoW(long long ptr) HAIKUDRV_WTInfoW

# Desktop
@ cdecl wine_create_desktop(long long) HAIKUDRV_create_desktop

# System tray
@ cdecl wine_notify_icon(long ptr)

#IME Interface
@ stdcall ImeInquire(ptr ptr wstr)
@ stdcall ImeConfigure(long long long ptr)
@ stdcall ImeDestroy(long)
@ stdcall ImeEscape(long long ptr)
@ stdcall ImeSelect(long long)
@ stdcall ImeSetActiveContext(long long)
@ stdcall ImeToAsciiEx(long long ptr ptr long long)
@ stdcall NotifyIME(long long long long)
@ stdcall ImeRegisterWord(wstr long wstr)
@ stdcall ImeUnregisterWord(wstr long wstr)
@ stdcall ImeEnumRegisterWord(ptr wstr long wstr ptr)
@ stdcall ImeSetCompositionString(long long ptr long ptr long)
@ stdcall ImeConversionList(long wstr ptr long long)
@ stdcall ImeProcessKey(long long long ptr)
@ stdcall ImeGetRegisterWordStyle(long ptr)
@ stdcall ImeGetImeMenuItems(long long long ptr ptr long)
