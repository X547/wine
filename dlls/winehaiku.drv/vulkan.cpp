extern "C" {
#include "config.h"
#include "haikudrv.h"
#include "wine/heap.h"
#include "wine/debug.h"

#define WINE_VK_HOST

#include "wine/vulkan.h"
#include "wine/vulkan_driver.h"
}
#include <AutoDeleter.h>

WINE_DEFAULT_DEBUG_CHANNEL(vulkan);


#define VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME "VK_EXT_headless_surface"
typedef VkFlags VkHeadlessSurfaceCreateFlagsEXT;
typedef struct VkHeadlessSurfaceCreateInfoEXT {
    VkStructureType                    sType;
    const void*                        pNext;
    VkHeadlessSurfaceCreateFlagsEXT    flags;
} VkHeadlessSurfaceCreateInfoEXT;

enum {
	VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT = 1000256000
};

typedef VkResult (VKAPI_PTR *PFN_vkCreateHeadlessSurfaceEXT)(VkInstance instance, const VkHeadlessSurfaceCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface);


static void *haikudrv_get_vk_device_proc_addr(const char *name);
static void *haikudrv_get_vk_instance_proc_addr(VkInstance instance, const char *name);


static VkResult wine_vk_instance_convert_create_info(const VkInstanceCreateInfo *src,
        VkInstanceCreateInfo *dst)
{
    unsigned int i;
    const char **enabled_extensions = NULL;

    dst->sType = src->sType;
    dst->flags = src->flags;
    dst->pApplicationInfo = src->pApplicationInfo;
    dst->pNext = src->pNext;
    dst->enabledLayerCount = 0;
    dst->ppEnabledLayerNames = NULL;
    dst->enabledExtensionCount = 0;
    dst->ppEnabledExtensionNames = NULL;
    
    TRACE("sType: %d\n", dst->sType);
    TRACE("dst->flags: %d\n", dst->flags);
    TRACE("dst->pApplicationInfo->sType: %d\n", dst->pApplicationInfo->sType);
    TRACE("dst->pApplicationInfo->pApplicationName: \"%s\"\n", dst->pApplicationInfo->pApplicationName);
    TRACE("dst->pApplicationInfo->pEngineName: \"%s\"\n", dst->pApplicationInfo->pEngineName);
    TRACE("dst->pApplicationInfo->apiVersion: %d\n", dst->pApplicationInfo->apiVersion);
    TRACE("dst->pNext: %p\n", dst->pNext);
    ((VkApplicationInfo*)dst->pApplicationInfo)->apiVersion = 4194304;
/*
    enabled_extensions = heap_calloc(2, sizeof(*src->ppEnabledExtensionNames));
    enabled_extensions[0] = "VK_KHR_surface";
    enabled_extensions[1] = "VK_EXT_headless_surface";
    dst->ppEnabledExtensionNames = enabled_extensions;
    dst->enabledExtensionCount = 2;
    return VK_SUCCESS;
*/
    if (src->enabledExtensionCount > 0)
    {
        enabled_extensions = (const char**)heap_calloc(src->enabledExtensionCount, sizeof(*src->ppEnabledExtensionNames));
        if (!enabled_extensions)
        {
            ERR("Failed to allocate memory for enabled extensions\n");
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }

        for (i = 0; i < src->enabledExtensionCount; i++)
        {
            if (
            	!strcmp(src->ppEnabledExtensionNames[i], "VK_KHR_win32_surface")
            )
            {
                enabled_extensions[i] = "VK_EXT_headless_surface";
            }
            else
            {
                enabled_extensions[i] = src->ppEnabledExtensionNames[i];
            }
            TRACE("enabled_extensions[%d]: \"%s\"\n", i, enabled_extensions[i]);
        }
        dst->ppEnabledExtensionNames = enabled_extensions;
        dst->enabledExtensionCount = src->enabledExtensionCount;
    }

    return VK_SUCCESS;
}

static const char *wine_vk_native_fn_name(const char *name)
{
    if (strcmp(name, "vkCreateWin32SurfaceKHR") == 0) {
    	return "vkCreateHeadlessSurfaceEXT";
    }
    if (strcmp(name, "vkGetPhysicalDeviceWin32PresentationSupportKHR") == 0) {
    	return "vkCreateHeadlessSurfaceEXT";
    }
    return name;
}


//#pragma mark -

class VKWineSurface {
};

class VKWineImage {
private:
	VkImage fImage;
	VkDeviceMemory fMemory;
};

class VKWineSwapchain {
private:
	ArrayDeleter<VKWineImage> fImages;

public:
	VKWineSwapchain(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo);
};


//#pragma mark -

static VkResult haikudrv_vkCreateInstance(const VkInstanceCreateInfo *create_info,
        const VkAllocationCallbacks *allocator, VkInstance *instance)
{
    VkInstanceCreateInfo create_info_host;
    VkResult res;
    TRACE("(create_info: %p, allocator: %p, instance: %p)\n", create_info, allocator, instance);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    /* Perform a second pass on converting VkInstanceCreateInfo. Winevulkan
     * performed a first pass in which it handles everything except for WSI
     * functionality such as VK_KHR_win32_surface. Handle this now.
     */
    res = wine_vk_instance_convert_create_info(create_info, &create_info_host);
    if (res != VK_SUCCESS)
    {
        ERR("Failed to convert instance create info, res=%d\n", res);
        return res;
    }

    res = vkCreateInstance(&create_info_host, NULL /* allocator */, instance);

    heap_free((void *)create_info_host.ppEnabledExtensionNames);
    return res;
}

static VkResult haikudrv_vkCreateSwapchainKHR(VkDevice device,
        const VkSwapchainCreateInfoKHR *create_info,
        const VkAllocationCallbacks *allocator, VkSwapchainKHR *swapchain)
{
	TRACE("()\n");
	return vkCreateSwapchainKHR(device, create_info, allocator, swapchain);
}

static VkResult haikudrv_vkCreateWin32SurfaceKHR(VkInstance instance,
        const VkWin32SurfaceCreateInfoKHR *create_info,
        const VkAllocationCallbacks *allocator, VkSurfaceKHR *surface)
{
	TRACE("()\n");
	PFN_vkCreateHeadlessSurfaceEXT proc = (PFN_vkCreateHeadlessSurfaceEXT)
		vkGetInstanceProcAddr(instance, "vkCreateHeadlessSurfaceEXT");
	VkHeadlessSurfaceCreateInfoEXT surfaceCreateInfo = {};
	surfaceCreateInfo.sType = (VkStructureType)VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT;
	return proc(instance, &surfaceCreateInfo, allocator, surface);
}

static void haikudrv_vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks *allocator)
{
	TRACE("()\n");
	vkDestroyInstance(instance, allocator);
}

static void haikudrv_vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface,
        const VkAllocationCallbacks *allocator)
{
	TRACE("()\n");
	vkDestroySurfaceKHR(instance, surface, allocator);
}

static void haikudrv_vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain,
         const VkAllocationCallbacks *allocator)
{
	TRACE("()\n");
	vkDestroySwapchainKHR(device, swapchain, allocator);
}

static VkResult haikudrv_vkEnumerateInstanceExtensionProperties(const char *layer_name,
        uint32_t *count, VkExtensionProperties* properties)
{
    unsigned int i;
    BOOL seen_surface = FALSE;
    VkResult res;

    TRACE("layer_name %s, count %p, properties %p\n", debugstr_a(layer_name), count, properties);

    /* This shouldn't get called with layer_name set, the ICD loader prevents it. */
    if (layer_name)
    {
        ERR("Layer enumeration not supported from ICD.\n");
        return VK_ERROR_LAYER_NOT_PRESENT;
    }

    /* We will return at most the same number of instance extensions reported by the host back to
     * winevulkan. Along the way we may replace MoltenVK extensions with their win32 equivalents,
     * or remove redundant extensions outright.
     * Winevulkan will perform more detailed filtering as it knows whether it has thunks
     * for a particular extension.
     */
    res = vkEnumerateInstanceExtensionProperties(layer_name, count, properties);
    if (!properties || res < 0)
        return res;

    for (i = 0; i < *count; i++)
    {
        /* For now the only MoltenVK extensions we need to fixup. Long-term we may need an array. */
        if (!strcmp(properties[i].extensionName, "VK_EXT_headless_surface"))
        {
            if (seen_surface)
            {
                /* If we've already seen a surface extension, just hide this one. */
                memmove(properties + i, properties + i + 1, (*count - i - 1) * sizeof(*properties));
                --*count;
                --i;
                continue;
            }
            TRACE("Substituting %s for VK_KHR_win32_surface\n", properties[i].extensionName);

            snprintf(properties[i].extensionName, sizeof(properties[i].extensionName),
                    VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
            properties[i].specVersion = VK_KHR_WIN32_SURFACE_SPEC_VERSION;
            seen_surface = TRUE;
        }
    }

    TRACE("Returning %u extensions.\n", *count);
    return res;
}

static void *haikudrv_vkGetDeviceProcAddr(VkDevice device, const char *name)
{
    void *proc_addr;

    TRACE("(%p, %s)\n", device, debugstr_a(name));

    if (!vkGetDeviceProcAddr(device, wine_vk_native_fn_name(name))) {
        ERR("%s not found\n", debugstr_a(name));
        return NULL;
    }

    if ((proc_addr = haikudrv_get_vk_device_proc_addr(name)))
        return proc_addr;

    return (void*)vkGetDeviceProcAddr(device, name);
}

static void *haikudrv_vkGetInstanceProcAddr(VkInstance instance, const char *name)
{
    void *proc_addr;

    TRACE("(%p, %s)\n", instance, debugstr_a(name));

    if (!vkGetInstanceProcAddr(instance, wine_vk_native_fn_name(name))) {
        ERR("%s not found\n", debugstr_a(name));
        return NULL;
    }

    if ((proc_addr = haikudrv_get_vk_instance_proc_addr(instance, name)))
        return proc_addr;

    return (void*)vkGetInstanceProcAddr(instance, name);
}

static VkResult haikudrv_vkGetPhysicalDeviceSurfaceCapabilities2KHR(VkPhysicalDevice phys_dev,
        const VkPhysicalDeviceSurfaceInfo2KHR *surface_info, VkSurfaceCapabilities2KHR *capabilities)
{
	TRACE("()\n");
	return vkGetPhysicalDeviceSurfaceCapabilities2KHR(phys_dev, surface_info, capabilities);
}

static VkResult haikudrv_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice phys_dev,
        VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR *capabilities)
{
	TRACE("()\n");
	return vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_dev, surface, capabilities);
}

static VkResult haikudrv_vkGetPhysicalDeviceSurfaceFormats2KHR(VkPhysicalDevice phys_dev,
        const VkPhysicalDeviceSurfaceInfo2KHR *surface_info, uint32_t *count, VkSurfaceFormat2KHR *formats)
{
	TRACE("()\n");
	return vkGetPhysicalDeviceSurfaceFormats2KHR(phys_dev, surface_info, count, formats);
}

static VkResult haikudrv_vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice phys_dev,
        VkSurfaceKHR surface, uint32_t *count, VkSurfaceFormatKHR *formats)
{
	TRACE("()\n");
	return vkGetPhysicalDeviceSurfaceFormatsKHR(phys_dev, surface, count, formats);
}

static VkResult haikudrv_vkGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice phys_dev,
        VkSurfaceKHR surface, uint32_t *count, VkPresentModeKHR *modes)
{
	TRACE("()\n");
	return vkGetPhysicalDeviceSurfacePresentModesKHR(phys_dev, surface, count, modes);
}

static VkResult haikudrv_vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice phys_dev,
        uint32_t index, VkSurfaceKHR surface, VkBool32 *supported)
{
	TRACE("()\n");
	return vkGetPhysicalDeviceSurfaceSupportKHR(phys_dev, index, surface, supported);
}

static VkBool32 haikudrv_vkGetPhysicalDeviceWin32PresentationSupportKHR(VkPhysicalDevice phys_dev,
        uint32_t index)
{
	TRACE("()\n");
	return VK_TRUE;
}

static VkResult haikudrv_vkGetSwapchainImagesKHR(VkDevice device,
        VkSwapchainKHR swapchain, uint32_t *count, VkImage *images)
{
	TRACE("()\n");
	return vkGetSwapchainImagesKHR(device, swapchain, count, images);
}

static VkResult haikudrv_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *present_info)
{
	TRACE("()\n");
	return vkQueuePresentKHR(queue, present_info);
}

static VkSurfaceKHR haikudrv_wine_get_native_surface(VkSurfaceKHR surface)
{
	TRACE("0x%s\n", wine_dbgstr_longlong(surface));
	return surface;
}


#define VULKAN_HOOK(name) .p_##name = haikudrv_##name
static const struct vulkan_funcs vulkan_funcs =
{
    VULKAN_HOOK(vkCreateInstance),
    VULKAN_HOOK(vkCreateSwapchainKHR),
    VULKAN_HOOK(vkCreateWin32SurfaceKHR),
    VULKAN_HOOK(vkDestroyInstance),
    VULKAN_HOOK(vkDestroySurfaceKHR),
    VULKAN_HOOK(vkDestroySwapchainKHR),
    VULKAN_HOOK(vkEnumerateInstanceExtensionProperties),
    // vkGetDeviceGroupSurfacePresentModesKHR
    VULKAN_HOOK(vkGetDeviceProcAddr),
    VULKAN_HOOK(vkGetInstanceProcAddr),
    // vkGetPhysicalDevicePresentRectanglesKHR
    VULKAN_HOOK(vkGetPhysicalDeviceSurfaceCapabilities2KHR),
    VULKAN_HOOK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR),
    VULKAN_HOOK(vkGetPhysicalDeviceSurfaceFormats2KHR),
    VULKAN_HOOK(vkGetPhysicalDeviceSurfaceFormatsKHR),
    VULKAN_HOOK(vkGetPhysicalDeviceSurfacePresentModesKHR),
    VULKAN_HOOK(vkGetPhysicalDeviceSurfaceSupportKHR),
    VULKAN_HOOK(vkGetPhysicalDeviceWin32PresentationSupportKHR),
    VULKAN_HOOK(vkGetSwapchainImagesKHR),
    VULKAN_HOOK(vkQueuePresentKHR),

    VULKAN_HOOK(wine_get_native_surface)
};
#undef VULKAN_HOOK

static void *haikudrv_get_vk_device_proc_addr(const char *name)
{
    TRACE("(\"%s\")\n", name);
    return get_vulkan_driver_device_proc_addr(&vulkan_funcs, name);
}

static void *haikudrv_get_vk_instance_proc_addr(VkInstance instance, const char *name)
{
    TRACE("(\"%s\")\n", name);
    return get_vulkan_driver_instance_proc_addr(&vulkan_funcs, instance, name);
}


static const struct vulkan_funcs *get_vulkan_driver(UINT version)
{
    TRACE("()\n");
    if (version != WINE_VULKAN_DRIVER_VERSION)
    {
        ERR("version mismatch, vulkan wants %u but driver has %u\n", version, WINE_VULKAN_DRIVER_VERSION);
        return NULL;
    }

    return &vulkan_funcs;
}


const struct vulkan_funcs *CDECL HAIKUDRV_wine_get_vulkan_driver(PHYSDEV dev, UINT version)
{
    TRACE("()\n");
    const struct vulkan_funcs *ret;

    if (!(ret = get_vulkan_driver( version )))
    {
        dev = GET_NEXT_PHYSDEV( dev, wine_get_vulkan_driver );
        ret = dev->funcs->wine_get_vulkan_driver( dev, version );
    }
    return ret;
}
