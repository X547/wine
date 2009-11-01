/*
 * Copyright 2005 Jacek Caban
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
#include <stdio.h>

#define COBJMACROS
#define NONAMELESSUNION
#define NONAMELESSSTRUCT

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "ole2.h"
#include "shlguid.h"
#include "idispids.h"

#include "wine/debug.h"
#include "wine/unicode.h"

#include "mshtml_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(mshtml);

static BOOL use_gecko_script(LPCWSTR url)
{
    static const WCHAR fileW[] = {'f','i','l','e',':'};
    static const WCHAR aboutW[] = {'a','b','o','u','t',':'};

    return strncmpiW(fileW, url, sizeof(fileW)/sizeof(WCHAR))
        && strncmpiW(aboutW, url, sizeof(aboutW)/sizeof(WCHAR));
}

void set_current_mon(HTMLWindow *This, IMoniker *mon)
{
    HRESULT hres;

    if(This->mon) {
        IMoniker_Release(This->mon);
        This->mon = NULL;
    }

    if(This->url) {
        CoTaskMemFree(This->url);
        This->url = NULL;
    }

    if(!mon)
        return;

    IMoniker_AddRef(mon);
    This->mon = mon;

    hres = IMoniker_GetDisplayName(mon, NULL, NULL, &This->url);
    if(FAILED(hres))
        WARN("GetDisplayName failed: %08x\n", hres);

    set_script_mode(This, use_gecko_script(This->url) ? SCRIPTMODE_GECKO : SCRIPTMODE_ACTIVESCRIPT);
}

static void set_progress_proc(task_t *_task)
{
    docobj_task_t *task = (docobj_task_t*)_task;
    IOleCommandTarget *olecmd = NULL;
    HTMLDocumentObj *doc = task->doc;
    HRESULT hres;

    TRACE("(%p)\n", doc);

    if(doc->client)
        IOleClientSite_QueryInterface(doc->client, &IID_IOleCommandTarget, (void**)&olecmd);

    if(olecmd) {
        VARIANT progress_max, progress;

        V_VT(&progress_max) = VT_I4;
        V_I4(&progress_max) = 0; /* FIXME */
        IOleCommandTarget_Exec(olecmd, NULL, OLECMDID_SETPROGRESSMAX, OLECMDEXECOPT_DONTPROMPTUSER,
                               &progress_max, NULL);

        V_VT(&progress) = VT_I4;
        V_I4(&progress) = 0; /* FIXME */
        IOleCommandTarget_Exec(olecmd, NULL, OLECMDID_SETPROGRESSPOS, OLECMDEXECOPT_DONTPROMPTUSER,
                               &progress, NULL);
    }

    if(doc->usermode == EDITMODE && doc->hostui) {
        DOCHOSTUIINFO hostinfo;

        memset(&hostinfo, 0, sizeof(DOCHOSTUIINFO));
        hostinfo.cbSize = sizeof(DOCHOSTUIINFO);
        hres = IDocHostUIHandler_GetHostInfo(doc->hostui, &hostinfo);
        if(SUCCEEDED(hres))
            /* FIXME: use hostinfo */
            TRACE("hostinfo = {%u %08x %08x %s %s}\n",
                    hostinfo.cbSize, hostinfo.dwFlags, hostinfo.dwDoubleClick,
                    debugstr_w(hostinfo.pchHostCss), debugstr_w(hostinfo.pchHostNS));
    }
}

static void set_downloading_proc(task_t *_task)
{
    HTMLDocumentObj *doc = ((docobj_task_t*)_task)->doc;
    IOleCommandTarget *olecmd;
    HRESULT hres;

    TRACE("(%p)\n", doc);

    if(doc->frame)
        IOleInPlaceFrame_SetStatusText(doc->frame, NULL /* FIXME */);

    if(!doc->client)
        return;

    hres = IOleClientSite_QueryInterface(doc->client, &IID_IOleCommandTarget, (void**)&olecmd);
    if(SUCCEEDED(hres)) {
        VARIANT var;

        V_VT(&var) = VT_I4;
        V_I4(&var) = 1;

        IOleCommandTarget_Exec(olecmd, NULL, OLECMDID_SETDOWNLOADSTATE, OLECMDEXECOPT_DONTPROMPTUSER,
                               &var, NULL);
        IOleCommandTarget_Release(olecmd);
    }

    if(doc->hostui) {
        IDropTarget *drop_target = NULL;

        hres = IDocHostUIHandler_GetDropTarget(doc->hostui, NULL /* FIXME */, &drop_target);
        if(drop_target) {
            FIXME("Use IDropTarget\n");
            IDropTarget_Release(drop_target);
        }
    }
}

static HRESULT set_moniker(HTMLDocument *This, IMoniker *mon, IBindCtx *pibc, BOOL *bind_complete)
{
    nsChannelBSC *bscallback;
    LPOLESTR url = NULL;
    docobj_task_t *task;
    HRESULT hres;
    nsresult nsres;

    if(pibc) {
        IUnknown *unk = NULL;

        /* FIXME:
         * Use params:
         * "__PrecreatedObject"
         * "BIND_CONTEXT_PARAM"
         * "__HTMLLOADOPTIONS"
         * "__DWNBINDINFO"
         * "URL Context"
         * "CBinding Context"
         * "_ITransData_Object_"
         * "_EnumFORMATETC_"
         */

        IBindCtx_GetObjectParam(pibc, (LPOLESTR)SZ_HTML_CLIENTSITE_OBJECTPARAM, &unk);
        if(unk) {
            IOleClientSite *client = NULL;

            hres = IUnknown_QueryInterface(unk, &IID_IOleClientSite, (void**)&client);
            if(SUCCEEDED(hres)) {
                TRACE("Got client site %p\n", client);
                IOleObject_SetClientSite(OLEOBJ(This), client);
                IOleClientSite_Release(client);
            }

            IUnknown_Release(unk);
        }
    }

    set_ready_state(This->window, READYSTATE_LOADING);
    update_doc(This, UPDATE_TITLE);

    HTMLDocument_LockContainer(This->doc_obj, TRUE);
    
    hres = IMoniker_GetDisplayName(mon, pibc, NULL, &url);
    if(FAILED(hres)) {
        WARN("GetDiaplayName failed: %08x\n", hres);
        return hres;
    }

    TRACE("got url: %s\n", debugstr_w(url));

    set_current_mon(This->window, mon);

    if(This->doc_obj->client) {
        VARIANT silent, offline;
        IOleCommandTarget *cmdtrg = NULL;

        hres = get_client_disp_property(This->doc_obj->client, DISPID_AMBIENT_SILENT, &silent);
        if(SUCCEEDED(hres)) {
            if(V_VT(&silent) != VT_BOOL)
                WARN("V_VT(silent) = %d\n", V_VT(&silent));
            else if(V_BOOL(&silent))
                FIXME("silent == true\n");
        }

        hres = get_client_disp_property(This->doc_obj->client,
                DISPID_AMBIENT_OFFLINEIFNOTCONNECTED, &offline);
        if(SUCCEEDED(hres)) {
            if(V_VT(&silent) != VT_BOOL)
                WARN("V_VT(offline) = %d\n", V_VT(&silent));
            else if(V_BOOL(&silent))
                FIXME("offline == true\n");
        }

        hres = IOleClientSite_QueryInterface(This->doc_obj->client, &IID_IOleCommandTarget,
                (void**)&cmdtrg);
        if(SUCCEEDED(hres)) {
            VARIANT var;

            V_VT(&var) = VT_I4;
            V_I4(&var) = 0;
            IOleCommandTarget_Exec(cmdtrg, &CGID_ShellDocView, 37, 0, &var, NULL);

            IOleCommandTarget_Release(cmdtrg);
        }
    }

    bscallback = create_channelbsc(mon);

    if(This->doc_obj->frame) {
        task = heap_alloc(sizeof(docobj_task_t));
        task->doc = This->doc_obj;
        push_task(&task->header, set_progress_proc, This->doc_obj->basedoc.task_magic);
    }

    task = heap_alloc(sizeof(docobj_task_t));
    task->doc = This->doc_obj;
    push_task(&task->header, set_downloading_proc, This->doc_obj->basedoc.task_magic);

    if(This->doc_obj->nscontainer) {
        This->doc_obj->nscontainer->bscallback = bscallback;
        nsres = nsIWebNavigation_LoadURI(This->doc_obj->nscontainer->navigation, url,
                LOAD_FLAGS_NONE, NULL, NULL, NULL);
        This->doc_obj->nscontainer->bscallback = NULL;
        if(NS_FAILED(nsres)) {
            WARN("LoadURI failed: %08x\n", nsres);
            IUnknown_Release((IUnknown*)bscallback);
            CoTaskMemFree(url);
            return E_FAIL;
        }
    }

    set_window_bscallback(This->window, bscallback);
    IUnknown_Release((IUnknown*)bscallback);
    CoTaskMemFree(url);

    if(bind_complete)
        *bind_complete = FALSE;
    return S_OK;
}

void set_ready_state(HTMLWindow *window, READYSTATE readystate)
{
    window->readystate = readystate;
    if(window->doc_obj->basedoc.window == window)
        call_property_onchanged(&window->doc_obj->basedoc.cp_propnotif, DISPID_READYSTATE);
}

static HRESULT get_doc_string(HTMLDocumentNode *This, char **str)
{
    nsIDOMNode *nsnode;
    LPCWSTR strw;
    nsAString nsstr;
    nsresult nsres;

    if(!This->nsdoc) {
        WARN("NULL nsdoc\n");
        return E_UNEXPECTED;
    }

    nsres = nsIDOMHTMLDocument_QueryInterface(This->nsdoc, &IID_nsIDOMNode, (void**)&nsnode);
    if(NS_FAILED(nsres)) {
        ERR("Could not get nsIDOMNode failed: %08x\n", nsres);
        return E_FAIL;
    }

    nsAString_Init(&nsstr, NULL);
    nsnode_to_nsstring(nsnode, &nsstr);
    nsIDOMNode_Release(nsnode);

    nsAString_GetData(&nsstr, &strw);
    TRACE("%s\n", debugstr_w(strw));

    *str = heap_strdupWtoA(strw);

    nsAString_Finish(&nsstr);

    return S_OK;
}


/**********************************************************
 * IPersistMoniker implementation
 */

#define PERSISTMON_THIS(iface) DEFINE_THIS(HTMLDocument, PersistMoniker, iface)

static HRESULT WINAPI PersistMoniker_QueryInterface(IPersistMoniker *iface, REFIID riid,
                                                            void **ppvObject)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);
    return IHTMLDocument2_QueryInterface(HTMLDOC(This), riid, ppvObject);
}

static ULONG WINAPI PersistMoniker_AddRef(IPersistMoniker *iface)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);
    return IHTMLDocument2_AddRef(HTMLDOC(This));
}

static ULONG WINAPI PersistMoniker_Release(IPersistMoniker *iface)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);
    return IHTMLDocument2_Release(HTMLDOC(This));
}

static HRESULT WINAPI PersistMoniker_GetClassID(IPersistMoniker *iface, CLSID *pClassID)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);
    return IPersist_GetClassID(PERSIST(This), pClassID);
}

static HRESULT WINAPI PersistMoniker_IsDirty(IPersistMoniker *iface)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);

    TRACE("(%p)\n", This);

    return IPersistStreamInit_IsDirty(PERSTRINIT(This));
}

static HRESULT WINAPI PersistMoniker_Load(IPersistMoniker *iface, BOOL fFullyAvailable,
        IMoniker *pimkName, LPBC pibc, DWORD grfMode)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);
    BOOL bind_complete = FALSE;
    HRESULT hres;

    TRACE("(%p)->(%x %p %p %08x)\n", This, fFullyAvailable, pimkName, pibc, grfMode);

    hres = set_moniker(This, pimkName, pibc, &bind_complete);
    if(FAILED(hres))
        return hres;

    if(!bind_complete)
        return start_binding(This->window, NULL, (BSCallback*)This->window->bscallback, pibc);

    return S_OK;
}

static HRESULT WINAPI PersistMoniker_Save(IPersistMoniker *iface, IMoniker *pimkName,
        LPBC pbc, BOOL fRemember)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);
    FIXME("(%p)->(%p %p %x)\n", This, pimkName, pbc, fRemember);
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistMoniker_SaveCompleted(IPersistMoniker *iface, IMoniker *pimkName, LPBC pibc)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);
    FIXME("(%p)->(%p %p)\n", This, pimkName, pibc);
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistMoniker_GetCurMoniker(IPersistMoniker *iface, IMoniker **ppimkName)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);

    TRACE("(%p)->(%p)\n", This, ppimkName);

    if(!This->window || !This->window->mon)
        return E_UNEXPECTED;

    IMoniker_AddRef(This->window->mon);
    *ppimkName = This->window->mon;
    return S_OK;
}

static const IPersistMonikerVtbl PersistMonikerVtbl = {
    PersistMoniker_QueryInterface,
    PersistMoniker_AddRef,
    PersistMoniker_Release,
    PersistMoniker_GetClassID,
    PersistMoniker_IsDirty,
    PersistMoniker_Load,
    PersistMoniker_Save,
    PersistMoniker_SaveCompleted,
    PersistMoniker_GetCurMoniker
};

/**********************************************************
 * IMonikerProp implementation
 */

#define MONPROP_THIS(iface) DEFINE_THIS(HTMLDocument, MonikerProp, iface)

static HRESULT WINAPI MonikerProp_QueryInterface(IMonikerProp *iface, REFIID riid, void **ppvObject)
{
    HTMLDocument *This = MONPROP_THIS(iface);
    return IHTMLDocument2_QueryInterface(HTMLDOC(This), riid, ppvObject);
}

static ULONG WINAPI MonikerProp_AddRef(IMonikerProp *iface)
{
    HTMLDocument *This = MONPROP_THIS(iface);
    return IHTMLDocument2_AddRef(HTMLDOC(This));
}

static ULONG WINAPI MonikerProp_Release(IMonikerProp *iface)
{
    HTMLDocument *This = MONPROP_THIS(iface);
    return IHTMLDocument_Release(HTMLDOC(This));
}

static HRESULT WINAPI MonikerProp_PutProperty(IMonikerProp *iface, MONIKERPROPERTY mkp, LPCWSTR val)
{
    HTMLDocument *This = MONPROP_THIS(iface);

    TRACE("(%p)->(%d %s)\n", This, mkp, debugstr_w(val));

    switch(mkp) {
    case MIMETYPEPROP:
        heap_free(This->doc_obj->mime);
        This->doc_obj->mime = heap_strdupW(val);
        break;

    case CLASSIDPROP:
        break;

    default:
        FIXME("mkp %d\n", mkp);
        return E_NOTIMPL;
    }

    return S_OK;
}

static const IMonikerPropVtbl MonikerPropVtbl = {
    MonikerProp_QueryInterface,
    MonikerProp_AddRef,
    MonikerProp_Release,
    MonikerProp_PutProperty
};

/**********************************************************
 * IPersistFile implementation
 */

#define PERSISTFILE_THIS(iface) DEFINE_THIS(HTMLDocument, PersistFile, iface)

static HRESULT WINAPI PersistFile_QueryInterface(IPersistFile *iface, REFIID riid, void **ppvObject)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);
    return IHTMLDocument2_QueryInterface(HTMLDOC(This), riid, ppvObject);
}

static ULONG WINAPI PersistFile_AddRef(IPersistFile *iface)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);
    return IHTMLDocument2_AddRef(HTMLDOC(This));
}

static ULONG WINAPI PersistFile_Release(IPersistFile *iface)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);
    return IHTMLDocument2_Release(HTMLDOC(This));
}

static HRESULT WINAPI PersistFile_GetClassID(IPersistFile *iface, CLSID *pClassID)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);

    TRACE("(%p)->(%p)\n", This, pClassID);

    if(!pClassID)
        return E_INVALIDARG;

    *pClassID = CLSID_HTMLDocument;
    return S_OK;
}

static HRESULT WINAPI PersistFile_IsDirty(IPersistFile *iface)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);

    TRACE("(%p)\n", This);

    return IPersistStreamInit_IsDirty(PERSTRINIT(This));
}

static HRESULT WINAPI PersistFile_Load(IPersistFile *iface, LPCOLESTR pszFileName, DWORD dwMode)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);
    FIXME("(%p)->(%s %08x)\n", This, debugstr_w(pszFileName), dwMode);
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistFile_Save(IPersistFile *iface, LPCOLESTR pszFileName, BOOL fRemember)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);
    char *str;
    DWORD written=0;
    HANDLE file;
    HRESULT hres;

    TRACE("(%p)->(%s %x)\n", This, debugstr_w(pszFileName), fRemember);

    file = CreateFileW(pszFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if(file == INVALID_HANDLE_VALUE) {
        WARN("Could not create file: %u\n", GetLastError());
        return E_FAIL;
    }

    hres = get_doc_string(This->doc_node, &str);
    if(SUCCEEDED(hres))
        WriteFile(file, str, strlen(str), &written, NULL);

    CloseHandle(file);
    return hres;
}

static HRESULT WINAPI PersistFile_SaveCompleted(IPersistFile *iface, LPCOLESTR pszFileName)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(pszFileName));
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistFile_GetCurFile(IPersistFile *iface, LPOLESTR *pszFileName)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);
    FIXME("(%p)->(%p)\n", This, pszFileName);
    return E_NOTIMPL;
}

static const IPersistFileVtbl PersistFileVtbl = {
    PersistFile_QueryInterface,
    PersistFile_AddRef,
    PersistFile_Release,
    PersistFile_GetClassID,
    PersistFile_IsDirty,
    PersistFile_Load,
    PersistFile_Save,
    PersistFile_SaveCompleted,
    PersistFile_GetCurFile
};

#define PERSTRINIT_THIS(iface) DEFINE_THIS(HTMLDocument, PersistStreamInit, iface)

static HRESULT WINAPI PersistStreamInit_QueryInterface(IPersistStreamInit *iface,
                                                       REFIID riid, void **ppv)
{
    HTMLDocument *This = PERSTRINIT_THIS(iface);
    return IHTMLDocument2_QueryInterface(HTMLDOC(This), riid, ppv);
}

static ULONG WINAPI PersistStreamInit_AddRef(IPersistStreamInit *iface)
{
    HTMLDocument *This = PERSTRINIT_THIS(iface);
    return IHTMLDocument2_AddRef(HTMLDOC(This));
}

static ULONG WINAPI PersistStreamInit_Release(IPersistStreamInit *iface)
{
    HTMLDocument *This = PERSTRINIT_THIS(iface);
    return IHTMLDocument2_Release(HTMLDOC(This));
}

static HRESULT WINAPI PersistStreamInit_GetClassID(IPersistStreamInit *iface, CLSID *pClassID)
{
    HTMLDocument *This = PERSTRINIT_THIS(iface);
    return IPersist_GetClassID(PERSIST(This), pClassID);
}

static HRESULT WINAPI PersistStreamInit_IsDirty(IPersistStreamInit *iface)
{
    HTMLDocument *This = PERSTRINIT_THIS(iface);

    TRACE("(%p)\n", This);

    if(This->doc_obj->usermode == EDITMODE)
        return editor_is_dirty(This);

    return S_FALSE;
}

static HRESULT WINAPI PersistStreamInit_Load(IPersistStreamInit *iface, LPSTREAM pStm)
{
    HTMLDocument *This = PERSTRINIT_THIS(iface);
    IMoniker *mon;
    HRESULT hres;

    static const WCHAR about_blankW[] = {'a','b','o','u','t',':','b','l','a','n','k',0};

    TRACE("(%p)->(%p)\n", This, pStm);

    hres = CreateURLMoniker(NULL, about_blankW, &mon);
    if(FAILED(hres)) {
        WARN("CreateURLMoniker failed: %08x\n", hres);
        return hres;
    }

    hres = set_moniker(This, mon, NULL, NULL);
    IMoniker_Release(mon);
    if(FAILED(hres))
        return hres;

    return channelbsc_load_stream(This->window->bscallback, pStm);
}

static HRESULT WINAPI PersistStreamInit_Save(IPersistStreamInit *iface, LPSTREAM pStm,
                                             BOOL fClearDirty)
{
    HTMLDocument *This = PERSTRINIT_THIS(iface);
    char *str;
    DWORD written=0;
    HRESULT hres;

    TRACE("(%p)->(%p %x)\n", This, pStm, fClearDirty);

    hres = get_doc_string(This->doc_node, &str);
    if(FAILED(hres))
        return hres;

    hres = IStream_Write(pStm, str, strlen(str), &written);
    if(FAILED(hres))
        FIXME("Write failed: %08x\n", hres);

    heap_free(str);

    if(fClearDirty)
        set_dirty(This, VARIANT_FALSE);

    return S_OK;
}

static HRESULT WINAPI PersistStreamInit_GetSizeMax(IPersistStreamInit *iface,
                                                   ULARGE_INTEGER *pcbSize)
{
    HTMLDocument *This = PERSTRINIT_THIS(iface);
    FIXME("(%p)->(%p)\n", This, pcbSize);
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistStreamInit_InitNew(IPersistStreamInit *iface)
{
    HTMLDocument *This = PERSTRINIT_THIS(iface);
    FIXME("(%p)\n", This);
    return E_NOTIMPL;
}

#undef PERSTRINIT_THIS

static const IPersistStreamInitVtbl PersistStreamInitVtbl = {
    PersistStreamInit_QueryInterface,
    PersistStreamInit_AddRef,
    PersistStreamInit_Release,
    PersistStreamInit_GetClassID,
    PersistStreamInit_IsDirty,
    PersistStreamInit_Load,
    PersistStreamInit_Save,
    PersistStreamInit_GetSizeMax,
    PersistStreamInit_InitNew
};

/**********************************************************
 * IPersistHistory implementation
 */

#define PERSISTHIST_THIS(iface) DEFINE_THIS(HTMLDocument, PersistHistory, iface)

static HRESULT WINAPI PersistHistory_QueryInterface(IPersistHistory *iface, REFIID riid, void **ppvObject)
{
    HTMLDocument *This = PERSISTHIST_THIS(iface);
    return IHTMLDocument2_QueryInterface(HTMLDOC(This), riid, ppvObject);
}

static ULONG WINAPI PersistHistory_AddRef(IPersistHistory *iface)
{
    HTMLDocument *This = PERSISTHIST_THIS(iface);
    return IHTMLDocument2_AddRef(HTMLDOC(This));
}

static ULONG WINAPI PersistHistory_Release(IPersistHistory *iface)
{
    HTMLDocument *This = PERSISTHIST_THIS(iface);
    return IHTMLDocument2_Release(HTMLDOC(This));
}

static HRESULT WINAPI PersistHistory_GetClassID(IPersistHistory *iface, CLSID *pClassID)
{
    HTMLDocument *This = PERSISTHIST_THIS(iface);
    return IPersist_GetClassID(PERSIST(This), pClassID);
}

static HRESULT WINAPI PersistHistory_LoadHistory(IPersistHistory *iface, IStream *pStream, IBindCtx *pbc)
{
    HTMLDocument *This = PERSISTHIST_THIS(iface);
    FIXME("(%p)->(%p %p)\n", This, pStream, pbc);
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistHistory_SaveHistory(IPersistHistory *iface, IStream *pStream)
{
    HTMLDocument *This = PERSISTHIST_THIS(iface);
    FIXME("(%p)->(%p)\n", This, pStream);
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistHistory_SetPositionCookie(IPersistHistory *iface, DWORD dwPositioncookie)
{
    HTMLDocument *This = PERSISTHIST_THIS(iface);
    FIXME("(%p)->(%x)\n", This, dwPositioncookie);
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistHistory_GetPositionCookie(IPersistHistory *iface, DWORD *pdwPositioncookie)
{
    HTMLDocument *This = PERSISTHIST_THIS(iface);
    FIXME("(%p)->(%p)\n", This, pdwPositioncookie);
    return E_NOTIMPL;
}

#undef PERSISTHIST_THIS

static const IPersistHistoryVtbl PersistHistoryVtbl = {
    PersistHistory_QueryInterface,
    PersistHistory_AddRef,
    PersistHistory_Release,
    PersistHistory_GetClassID,
    PersistHistory_LoadHistory,
    PersistHistory_SaveHistory,
    PersistHistory_SetPositionCookie,
    PersistHistory_GetPositionCookie
};

void HTMLDocument_Persist_Init(HTMLDocument *This)
{
    This->lpPersistMonikerVtbl = &PersistMonikerVtbl;
    This->lpPersistFileVtbl = &PersistFileVtbl;
    This->lpMonikerPropVtbl = &MonikerPropVtbl;
    This->lpPersistStreamInitVtbl = &PersistStreamInitVtbl;
    This->lpPersistHistoryVtbl = &PersistHistoryVtbl;
}
