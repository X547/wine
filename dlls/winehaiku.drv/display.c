/*
 * HAIKUDRV display device functions
 *
 * Copyright 2019 Zhiyi Zhang for CodeWeavers
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

#include "windef.h"
#include "winbase.h"
#include "rpc.h"
#include "winreg.h"
#include "cfgmgr32.h"
#include "initguid.h"
#include "devguid.h"
#include "devpkey.h"
#include "ntddvdeo.h"
#include "setupapi.h"
#define WIN32_NO_STATUS
#include "winternl.h"
#include "wine/debug.h"
#include "wine/unicode.h"
#include "haikudrv.h"

WINE_DEFAULT_DEBUG_CHANNEL(haikudrv);


void CDECL HAIKUDRV_UpdateDisplayDevices( const struct gdi_device_manager *device_manager,
                                        BOOL force, void *param )
{
    INT gpu_count, adapter_count, monitor_count;
    DWORD len;
    static BOOL called = FALSE;
    
    if (!called) {
    	called = TRUE;
    } else {
        if (!force) return;
    }

    for (int i = 0; i < 1; i++)
    {
        struct gdi_gpu gdi_gpu =
        {
            .id = 1,
            .vendor_id = 0x1002,
            .device_id = 0x683f,
            .subsys_id = 0,
            .revision_id = 0,
        };
        const char *gpuName = "RADV VERDE";
        RtlUTF8ToUnicodeN(gdi_gpu.name, sizeof(gdi_gpu.name), &len, gpuName, strlen(gpuName));
        device_manager->add_gpu(&gdi_gpu, param);

        for (int j = 0; j < 1; j++)
        {
            struct gdi_adapter gdi_adapter =
            {
                .id = 1,
                .state_flags = DISPLAY_DEVICE_ATTACHED_TO_DESKTOP | DISPLAY_DEVICE_PRIMARY_DEVICE,
            };
            device_manager->add_adapter( &gdi_adapter, param );

            for (int k = 0; k < 1; k++)
            {
                struct gdi_monitor gdi_monitor =
                {
                    .rc_monitor = {0, 0, 1920, 1080},
                    .rc_work = {0, 0, 1920, 1080},
                    .state_flags = DISPLAY_DEVICE_ATTACHED | DISPLAY_DEVICE_ACTIVE,
                };
                const char *monitorName = "Generic Non-PnP Monitor";
                RtlUTF8ToUnicodeN(gdi_monitor.name, sizeof(gdi_monitor.name), &len,
                                  monitorName, strlen(monitorName));
                device_manager->add_monitor( &gdi_monitor, param );
            }

        }
    }
}
