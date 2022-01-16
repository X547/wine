#include "config.h"
#include "haikudrv.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(haikudrv);


typedef struct
{
    struct gdi_physdev  dev;
} HAIKUDRV_PDEVICE;

static inline HAIKUDRV_PDEVICE *get_haikudrv_dev(PHYSDEV dev)
{
    return (HAIKUDRV_PDEVICE*)dev;
}


/* a few dynamic device caps */
static int horz_size;           /* horz. size of screen in millimeters */
static int vert_size;           /* vert. size of screen in millimeters */
static int bits_per_pixel;      /* pixel depth of screen */
static int device_data_valid;   /* do the above variables have up-to-date values? */

int retina_on = FALSE;

static CRITICAL_SECTION device_data_section;
static CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &device_data_section,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": device_data_section") }
};
static CRITICAL_SECTION device_data_section = { &critsect_debug, -1, 0, 0, 0, 0 };


/**********************************************************************
 *              device_init
 *
 * Perform initializations needed upon creation of the first device.
 */
static void device_init(void)
{
    /* Initialize device caps */
    horz_size = 1920;
    vert_size = 1080;

    bits_per_pixel = 32;
    device_data_valid = TRUE;
}


void haikudrv_reset_device_metrics(void)
{
    EnterCriticalSection(&device_data_section);
    device_data_valid = FALSE;
    LeaveCriticalSection(&device_data_section);
}


static HAIKUDRV_PDEVICE *create_haiku_physdev(void)
{
    HAIKUDRV_PDEVICE *physDev;
 
    EnterCriticalSection(&device_data_section);
    if (!device_data_valid) device_init();
    LeaveCriticalSection(&device_data_section);

    if (!(physDev = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*physDev)))) return NULL;

    return physDev;
}


/**********************************************************************
 *              CreateDC (HAIKUDRV.@)
 */
BOOL CDECL HAIKUDRV_CreateDC(PHYSDEV *pdev, LPCWSTR device, LPCWSTR output,
                                  const DEVMODEW* initData)
{
    HAIKUDRV_PDEVICE *physDev = create_haiku_physdev();

    TRACE("pdev %p hdc %p device %s output %s initData %p\n", pdev,
          (*pdev)->hdc, debugstr_w(device), debugstr_w(output), initData);

    if (!physDev) return FALSE;

    push_dc_driver(pdev, &physDev->dev, &haikudrv_funcs.dc_funcs);
    return TRUE;
}


/**********************************************************************
 *              CreateCompatibleDC (HAIKUDRV.@)
 */
BOOL CDECL HAIKUDRV_CreateCompatibleDC(PHYSDEV orig, PHYSDEV *pdev)
{
    HAIKUDRV_PDEVICE *physDev = create_haiku_physdev();

    TRACE("orig %p orig->hdc %p pdev %p pdev->hdc %p\n", orig, (orig ? orig->hdc : NULL), pdev,
          ((pdev && *pdev) ? (*pdev)->hdc : NULL));

    if (!physDev) return FALSE;

    push_dc_driver(pdev, &physDev->dev, &haikudrv_funcs.dc_funcs);
    return TRUE;
}


/**********************************************************************
 *              DeleteDC (HAIKUDRV.@)
 */
BOOL CDECL HAIKUDRV_DeleteDC(PHYSDEV dev)
{
    HAIKUDRV_PDEVICE *physDev = get_haikudrv_dev(dev);

    TRACE("hdc %p\n", dev->hdc);

    HeapFree(GetProcessHeap(), 0, physDev);
    return TRUE;
}


/***********************************************************************
 *              GetDeviceCaps (HAIKUDRV.@)
 */
INT CDECL HAIKUDRV_GetDeviceCaps(PHYSDEV dev, INT cap)
{
    INT ret;

    TRACE("hdc %p\n", dev->hdc);

    EnterCriticalSection(&device_data_section);

    if (!device_data_valid) device_init();

    switch(cap)
    {
    case HORZSIZE:
        ret = horz_size;
        break;
    case VERTSIZE:
        ret = vert_size;
        break;
    case BITSPIXEL:
        ret = bits_per_pixel;
        break;
    case HORZRES:
    case VERTRES:
    default:
        LeaveCriticalSection(&device_data_section);
        dev = GET_NEXT_PHYSDEV( dev, pGetDeviceCaps );
        ret = dev->funcs->pGetDeviceCaps( dev, cap );
        if ((cap == HORZRES || cap == VERTRES) && retina_on)
            ret *= 2;
        return ret;
    }

    TRACE("cap %d -> %d\n", cap, ret);

    LeaveCriticalSection(&device_data_section);
    return ret;
}
