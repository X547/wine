#include <locks.h>

extern "C" {
#include "config.h"
#include "haikudrv.h"
#include "wine/heap.h"
#include "wine/debug.h"

#define WINE_VK_HOST

#include "wine/vulkan.h"
#include "wine/vulkan_driver.h"
}
#include <new>
#include <cassert>
#include <algorithm>
#include <map>
#include <locks.h>
#include <AutoDeleter.h>

WINE_DEFAULT_DEBUG_CHANNEL(haiku_vk);


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

#define VkCheckRet(err) {VkResult _err = (err); if (_err != VK_SUCCESS) return _err;}


recursive_lock sDevicesLock = RECURSIVE_LOCK_INITIALIZER("vkDevices");
std::map<VkDevice, VkPhysicalDevice> *sDevices;


static VkPhysicalDevice GetPhysicalDevice(VkDevice dev)
{
	RecursiveLocker lock(&sDevicesLock);
	if (sDevices == NULL)
		return NULL;
	auto it = sDevices->find(dev);
	if (it == sDevices->end())
		return NULL;
	return it->second;
}

static uint32_t getMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t typeBits, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);
	for (uint32_t i = 0; i < deviceMemoryProperties.memoryTypeCount; i++) {
		if ((typeBits & 1) == 1) {
			if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}
		typeBits >>= 1;
	}
	return 0;
}

static void insertImageMemoryBarrier(
	VkCommandBuffer cmdbuffer,
	VkImage image,
	VkAccessFlags srcAccessMask,
	VkAccessFlags dstAccessMask,
	VkImageLayout oldImageLayout,
	VkImageLayout newImageLayout,
	VkPipelineStageFlags srcStageMask,
	VkPipelineStageFlags dstStageMask,
	VkImageSubresourceRange subresourceRange)
{
	VkImageMemoryBarrier imageMemoryBarrier{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = srcAccessMask,
		.dstAccessMask = dstAccessMask,
		.oldLayout = oldImageLayout,
		.newLayout = newImageLayout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = image,
		.subresourceRange = subresourceRange
	};

	vkCmdPipelineBarrier(
		cmdbuffer,
		srcStageMask,
		dstStageMask,
		0,
		0, nullptr,
		0, nullptr,
		1, &imageMemoryBarrier
	);
}

static VkResult submitWork(VkDevice device, VkCommandBuffer cmdBuffer, VkQueue queue)
{
	VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;
	VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
	VkFence fence;
	VkCheckRet(vkCreateFence(device, &fenceInfo, nullptr, &fence));
	VkCheckRet(vkQueueSubmit(queue, 1, &submitInfo, fence));
	VkCheckRet(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
	vkDestroyFence(device, fence, nullptr);
	return VK_SUCCESS;
}


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

class BufferQueue {
private:
	ArrayDeleter<int32> fItems;
	int32 fBeg, fLen, fMaxLen;

public:
	BufferQueue(int32 maxLen = 0);
	bool SetMaxLen(int32 maxLen);

	inline int32 Length() {return fLen;}
	bool Add(int32 val);
	int32 Remove();
	int32 Begin();
};

BufferQueue::BufferQueue(int32 maxLen):
	fItems((maxLen > 0) ? new int32[maxLen] : NULL),
	fBeg(0), fLen(0), fMaxLen(maxLen)
{}

bool BufferQueue::SetMaxLen(int32 maxLen)
{
	if (!(maxLen > 0)) {
		fItems.Unset();
	} else {
		auto newItems = new(std::nothrow) int32[maxLen];
		if (newItems == NULL)
			return false;
		fItems.SetTo(newItems);
	}
	fMaxLen = maxLen;
	fBeg = 0; fLen = 0; fMaxLen = maxLen;
	return true;
}


bool BufferQueue::Add(int32 val)
{
	if (!(fLen < fMaxLen))
		return false;
	fItems[(fBeg + fLen)%fMaxLen] = val;
	fLen++;
	return true;
}

int32 BufferQueue::Remove()
{
	if (!(fLen > 0))
		return -1;
	int32 res = fItems[fBeg%fMaxLen];
	fBeg = (fBeg + 1)%fMaxLen;
	fLen--;
	return res;
}

int32 BufferQueue::Begin()
{
	if (!(fLen > 0))
		return -1;
	return fItems[fBeg%fMaxLen];
}


//#pragma mark -

class VKWineImage {
private:
	VkDevice fDevice;
	VkImage fImage;
	VkDeviceMemory fMemory;

public:
	VKWineImage();
	~VKWineImage();
	VkResult Init(VkDevice device, const VkImageCreateInfo &createInfo, bool cpuMem = false);

	VkImage ToHandle() {return fImage;}
	VkDeviceMemory GetMemoryHandle() {return fMemory;}
};

class VKWineSurface {
private:
	HWND fHwnd;
public:
	VKWineSurface();
	~VKWineSurface();
	VkResult Init(VkInstance instance, const VkWin32SurfaceCreateInfoKHR &createInfo);

	VkResult GetCapabilities(VkPhysicalDevice physDev, VkSurfaceCapabilitiesKHR *capabilities);
	VkResult GetFormats(VkPhysicalDevice physDev, uint32_t *count, VkSurfaceFormatKHR *formats);
	VkResult GetPresentModes(VkPhysicalDevice physDev, uint32_t *count, VkPresentModeKHR *modes);
	VkResult GetPresentRectangles(VkPhysicalDevice physDev, uint32_t* pRectCount, VkRect2D* pRects);

	static VKWineSurface *FromHandle(VkSurfaceKHR surface) {return (VKWineSurface*)surface;}
	VkSurfaceKHR ToHandle() {return (VkSurfaceKHR)this;}
	HWND GetHwnd() {return fHwnd;}
};

class VKWineSwapchain {
private:
	recursive_lock fLock;
	VkDevice fDevice;
	VKWineSurface *fSurface;
	VkExtent2D fImageExtent;
	uint32 fImageCnt;
	ArrayDeleter<VKWineImage> fImages;
	BufferQueue fImagePool;
	ObjectDeleter<VKWineImage> fBuffer;
	VkCommandPool fCommandPool;
	VkQueue fQueue;
	VkFence fFence;

	VkImageCreateInfo ImageFromCreateInfo(const VkSwapchainCreateInfoKHR &createInfo);
	VkResult CreateBuffer();
	VkResult CopyToBuffer(VkImage srcImage, int32_t width, int32_t height);

public:
	VKWineSwapchain();
	~VKWineSwapchain();
	VkResult Init(VkDevice device, const VkSwapchainCreateInfoKHR &createInfo);

	VkResult GetSwapchainImages(uint32_t *count, VkImage *images);
	VkResult AcquireNextImage(const VkAcquireNextImageInfoKHR *pAcquireInfo, uint32_t *pImageIndex);
	VkResult QueuePresent(VkQueue queue, const VkPresentInfoKHR *present_info, uint32_t idx);

	static VKWineSwapchain *FromHandle(VkSwapchainKHR surface) {return (VKWineSwapchain*)surface;}
	VkSwapchainKHR ToHandle() {return (VkSwapchainKHR)this;}
};


//#pragma mark - VKWineImage

VKWineImage::VKWineImage():
	fDevice(NULL), fImage(0), fMemory(0)
{}

VKWineImage::~VKWineImage()
{
	vkFreeMemory(fDevice, fMemory, NULL);
	vkDestroyImage(fDevice, fImage, NULL);
}

VkResult VKWineImage::Init(VkDevice device, const VkImageCreateInfo &createInfo, bool cpuMem)
{
	fDevice = device;

	VkCheckRet(vkCreateImage(fDevice, &createInfo, NULL, &fImage));

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, fImage, &memRequirements);
	size_t memTypeIdx = 0;
	if (cpuMem) {
		memTypeIdx = getMemoryTypeIndex(GetPhysicalDevice(fDevice), memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	} else {
		for (; memTypeIdx < 8 * sizeof(memRequirements.memoryTypeBits); ++memTypeIdx) {
			if (memRequirements.memoryTypeBits & (1u << memTypeIdx))
				break;
		}
		assert(memTypeIdx <= 8 * sizeof(memRequirements.memoryTypeBits) - 1);
	}

	VkMemoryAllocateInfo memAllocInfo{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memRequirements.size,
		.memoryTypeIndex = (uint32_t)memTypeIdx
	};
	VkCheckRet(vkAllocateMemory(device, &memAllocInfo, nullptr, &fMemory));
	VkCheckRet(vkBindImageMemory(device, fImage, fMemory, 0));

	return VK_SUCCESS;
}


//#pragma mark - VKWineSurface

VKWineSurface::VKWineSurface():
	fHwnd(NULL)
{}

VKWineSurface::~VKWineSurface()
{}

VkResult VKWineSurface::Init(VkInstance instance, const VkWin32SurfaceCreateInfoKHR &createInfo)
{
	fHwnd = createInfo.hwnd;
	return VK_SUCCESS;
}

VkResult VKWineSurface::GetCapabilities(VkPhysicalDevice physDev, VkSurfaceCapabilitiesKHR *surfaceCapabilities)
{
	/* Image count limits */
	surfaceCapabilities->minImageCount = 1;
	surfaceCapabilities->maxImageCount = 3;

	/* Surface extents */
	surfaceCapabilities->currentExtent = {0xffffffff, 0xffffffff};
	surfaceCapabilities->minImageExtent = {1, 1};
	/* Ask the device for max */
	VkPhysicalDeviceProperties devProps;
	vkGetPhysicalDeviceProperties(physDev, &devProps);

	surfaceCapabilities->maxImageExtent = {
		devProps.limits.maxImageDimension2D, devProps.limits.maxImageDimension2D
	};
	surfaceCapabilities->maxImageArrayLayers = 1;

	/* Surface transforms */
	surfaceCapabilities->supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	surfaceCapabilities->currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

	/* Composite alpha */
	surfaceCapabilities->supportedCompositeAlpha = (VkCompositeAlphaFlagBitsKHR)(
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR | VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR |
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR | VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR
	);

	/* Image usage flags */
	surfaceCapabilities->supportedUsageFlags =
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
	
	return VK_SUCCESS;
}

VkResult VKWineSurface::GetFormats(VkPhysicalDevice physDev, uint32_t *count, VkSurfaceFormatKHR *surfaceFormats)
{
#if 0
	VkFormat formats[] = {VK_FORMAT_B8G8R8A8_UNORM};
	uint32_t formatCnt = 1;
#else
	constexpr int max_core_1_0_formats = VK_FORMAT_ASTC_12x12_SRGB_BLOCK + 1;
	VkFormat formats[max_core_1_0_formats];
	uint32_t formatCnt = 0;
	
	for (int format = 0; format < max_core_1_0_formats; format++) {
		VkImageFormatProperties formatProps;
		VkResult res = vkGetPhysicalDeviceImageFormatProperties(
			physDev, (VkFormat)format, VK_IMAGE_TYPE_2D,
			VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
			&formatProps
		);
		if (res != VK_ERROR_FORMAT_NOT_SUPPORTED) {
			formats[formatCnt++] = (VkFormat)format;
		}
	}
#endif

	if (surfaceFormats == NULL) {
		*count = formatCnt;
		return VK_SUCCESS;
	}
	memcpy(surfaceFormats, formats, sizeof(VkFormat)*std::min<uint32_t>(*count, formatCnt));
	if (*count < formatCnt)
		return VK_INCOMPLETE;
	return VK_SUCCESS;
}

VkResult VKWineSurface::GetPresentModes(VkPhysicalDevice physDev, uint32_t *count, VkPresentModeKHR *presentModes)
{
	static const VkPresentModeKHR modes[] = {VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_FIFO_RELAXED_KHR};
	if (presentModes == NULL) {
		*count = B_COUNT_OF(modes);
		return VK_SUCCESS;
	}
	memcpy(presentModes, modes, sizeof(VkPresentModeKHR)*std::min<uint32_t>(*count, B_COUNT_OF(modes)));
	if (*count < B_COUNT_OF(modes))
		return VK_INCOMPLETE;
	return VK_SUCCESS;
}

VkResult VKWineSurface::GetPresentRectangles(VkPhysicalDevice physDev, uint32_t* pRectCount, VkRect2D* pRects)
{
	if (pRects == NULL) {
		*pRectCount = 1;
		return VK_SUCCESS;
	}
	if (*pRectCount < 1) {
		return VK_INCOMPLETE;
	}
	VkSurfaceCapabilitiesKHR caps;
	VkCheckRet(GetCapabilities(physDev, &caps));
	pRects[0].offset.x = 0;
	pRects[0].offset.y = 0;
	pRects[0].extent = caps.currentExtent;
	return VK_SUCCESS;
}


//#pragma mark - VKWineSwapchain

VKWineSwapchain::VKWineSwapchain():
	fLock(RECURSIVE_LOCK_INITIALIZER("VKWineSwapchain")),
	fCommandPool(VK_NULL_HANDLE),
	fFence(VK_NULL_HANDLE)
{}

VKWineSwapchain::~VKWineSwapchain()
{
	if (fCommandPool != VK_NULL_HANDLE) {
		vkDestroyCommandPool(fDevice, fCommandPool, nullptr);
		vkQueueWaitIdle(fQueue);
	}

	vkDestroyFence(fDevice, fFence, NULL);
	recursive_lock_destroy(&fLock);
}

VkImageCreateInfo VKWineSwapchain::ImageFromCreateInfo(const VkSwapchainCreateInfoKHR &createInfo)
{
	return VkImageCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = createInfo.imageFormat,
		.extent = {
			createInfo.imageExtent.width,
			createInfo.imageExtent.height,
			1
		},
		.mipLevels = 1,
		.arrayLayers = createInfo.imageArrayLayers,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = createInfo.imageUsage,
		.sharingMode = createInfo.imageSharingMode,
		.queueFamilyIndexCount = createInfo.queueFamilyIndexCount,
		.pQueueFamilyIndices = createInfo.pQueueFamilyIndices,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};
}

VkResult VKWineSwapchain::CreateBuffer()
{
	VkCommandPoolCreateInfo cmdPoolInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = 0
	};
	VkCheckRet(vkCreateCommandPool(fDevice, &cmdPoolInfo, nullptr, &fCommandPool));

	VkImageCreateInfo createInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_B8G8R8A8_UNORM,
		.extent = {
			.width = fImageExtent.width,
			.height = fImageExtent.height,
			.depth = 1
		},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_LINEAR,
		.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};
	fBuffer.SetTo(new(std::nothrow) VKWineImage());
	if (!fBuffer.IsSet())
		return VK_ERROR_OUT_OF_HOST_MEMORY;
	VkCheckRet(fBuffer->Init(fDevice, createInfo, true));
	return VK_SUCCESS;
}

VkResult VKWineSwapchain::CopyToBuffer(VkImage srcImage, int32_t width, int32_t height)
{
	// Do the actual blit from the offscreen image to our host visible destination image
	VkCommandBufferAllocateInfo cmdBufAllocateInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = fCommandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};
	VkCommandBuffer copyCmd;
	VkCheckRet(vkAllocateCommandBuffers(fDevice, &cmdBufAllocateInfo, &copyCmd));
	VkCommandBufferBeginInfo cmdBufInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	VkCheckRet(vkBeginCommandBuffer(copyCmd, &cmdBufInfo));

	// Transition destination image to transfer destination layout
	insertImageMemoryBarrier(
		copyCmd,
		fBuffer->ToHandle(),
		0,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	);

	// srcImage is already in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, and does not need to be transitioned

#if 0
	VkImageCopy imageCopyRegion{};
	imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageCopyRegion.srcSubresource.layerCount = 1;
	imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageCopyRegion.dstSubresource.layerCount = 1;
	imageCopyRegion.extent.width = width;
	imageCopyRegion.extent.height = height;
	imageCopyRegion.extent.depth = 1;

	vkCmdCopyImage(
		copyCmd,
		srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		fBuffer->ToHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&imageCopyRegion
	);
#endif
	// Define the region to blit (we will blit the whole swapchain image)
	VkOffset3D blitSize;
	blitSize.x = width;
	blitSize.y = height;
	blitSize.z = 1;
	VkImageBlit imageBlitRegion{};
	imageBlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageBlitRegion.srcSubresource.layerCount = 1;
	imageBlitRegion.srcOffsets[1] = blitSize;
	imageBlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageBlitRegion.dstSubresource.layerCount = 1;
	imageBlitRegion.dstOffsets[1] = blitSize;

	// Issue the blit command
	vkCmdBlitImage(
		copyCmd,
		srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		fBuffer->ToHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&imageBlitRegion,
		VK_FILTER_NEAREST
	);

	// Transition destination image to general layout, which is the required layout for mapping the image memory later on
	insertImageMemoryBarrier(
		copyCmd,
		fBuffer->ToHandle(),
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_MEMORY_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
	);

	VkCheckRet(vkEndCommandBuffer(copyCmd));

	VkCheckRet(submitWork(fDevice, copyCmd, fQueue));
	vkFreeCommandBuffers(fDevice, fCommandPool, 1, &copyCmd);

	return VK_SUCCESS;
}

VkResult VKWineSwapchain::Init(VkDevice device, const VkSwapchainCreateInfoKHR &createInfo)
{
	fDevice = device;
	fSurface = VKWineSurface::FromHandle(createInfo.surface);

	VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, 0};
	VkCheckRet(vkCreateFence(fDevice, &fence_info, NULL, &fFence));

	fImageExtent.width = createInfo.imageExtent.width;
	fImageExtent.height = createInfo.imageExtent.height;

	VkImageCreateInfo imageCreateInfo = ImageFromCreateInfo(createInfo);

	fImageCnt = createInfo.minImageCount;
	fImages.SetTo(new(std::nothrow) VKWineImage[fImageCnt]);
	if (!fImages.IsSet())
		return VK_ERROR_OUT_OF_HOST_MEMORY;
	if(!fImagePool.SetMaxLen(fImageCnt))
		return VK_ERROR_OUT_OF_HOST_MEMORY;

	for (uint32_t i = 0; i < fImageCnt; i++) {
		VkCheckRet(fImages[i].Init(device, imageCreateInfo));
		fImagePool.Add(i);
	}

	vkGetDeviceQueue(device, 0, 0, &fQueue);
	//VkCheckRet(vkSetDeviceLoaderData(device, fQueue));

	VkCheckRet(CreateBuffer());

	return VK_SUCCESS;
}

VkResult VKWineSwapchain::GetSwapchainImages(uint32_t *count, VkImage *images)
{
	if (images == NULL) {
		*count = fImageCnt;
		return VK_SUCCESS;
	}
	uint32_t copyCnt = std::min<uint32_t>(*count, fImageCnt);
	for (uint32_t i = 0; i < copyCnt; i++) {
		images[i] = fImages[i].ToHandle();
	}
	if (*count < fImageCnt)
		return VK_INCOMPLETE;
	return VK_SUCCESS;
}

VkResult VKWineSwapchain::AcquireNextImage(const VkAcquireNextImageInfoKHR *pAcquireInfo, uint32_t *pImageIndex)
{
	int32 imageIdx;
	for(;;) {
		recursive_lock_lock(&fLock);
		imageIdx = fImagePool.Remove();
		recursive_lock_unlock(&fLock);
		if (imageIdx < 0) {
			snooze(100);
			continue;
		}
		break;
	};
	*pImageIndex = imageIdx;

	if (VK_NULL_HANDLE != pAcquireInfo->semaphore || VK_NULL_HANDLE != pAcquireInfo->fence) {
		VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
	
		if (VK_NULL_HANDLE != pAcquireInfo->semaphore) {
			submit.signalSemaphoreCount = 1;
			submit.pSignalSemaphores = &pAcquireInfo->semaphore;
		}
	
		submit.commandBufferCount = 0;
		submit.pCommandBuffers = nullptr;
		VkResult retval = vkQueueSubmit(fQueue, 1, &submit, pAcquireInfo->fence);
		assert(retval == VK_SUCCESS);
	}

	return VK_SUCCESS;
}

VkResult VKWineSwapchain::QueuePresent(VkQueue queue, const VkPresentInfoKHR *presentInfo, uint32_t idx)
{
	vkResetFences(fDevice, 1, &fFence);
	VkPipelineStageFlags pipeline_stage_flags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

	VkSubmitInfo submit_info = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL, presentInfo->waitSemaphoreCount, presentInfo->pWaitSemaphores, &pipeline_stage_flags, 0, NULL, 0, NULL
	};

	VkResult result = vkQueueSubmit(queue, 1, &submit_info, fFence);
	if (result == VK_SUCCESS) {
		vkWaitForFences(fDevice, 1, &fFence, VK_TRUE, 100000000000);
	}

	recursive_lock_lock(&fLock);
	uint32_t imageIdx = presentInfo->pImageIndices[idx];

	CopyToBuffer(fImages[imageIdx].ToHandle(), fImageExtent.width, fImageExtent.height);
	VKWineImage &srcImage = *fBuffer.Get();

	VkImageSubresource subResource{
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
	};
	VkSubresourceLayout subResourceLayout;
	vkGetImageSubresourceLayout(fDevice, srcImage.ToHandle(), &subResource, &subResourceLayout);
	size_t stride = subResourceLayout.rowPitch;

	char *imagedata = NULL;
	vkMapMemory(fDevice, srcImage.GetMemoryHandle(), 0, VK_WHOLE_SIZE, 0, (void**)&imagedata);
	imagedata += subResourceLayout.offset;

	HDC hdc = GetDC(fSurface->GetHwnd());
	BITMAPINFO bitmapInfo{
		.bmiHeader = {
			.biSize = sizeof(BITMAPINFOHEADER),
			.biWidth = (LONG)(stride/4),
			.biHeight = -(LONG)fImageExtent.height,
			.biPlanes = 1,
			.biBitCount = 32,
			.biCompression = BI_RGB,
			.biSizeImage = (DWORD)(stride*fImageExtent.height)
		}
	};
	SetDIBitsToDevice(hdc, 0, 0, fImageExtent.width, fImageExtent.height, 0, 0, 0, fImageExtent.height, imagedata, &bitmapInfo, DIB_RGB_COLORS);
	ReleaseDC(fSurface->GetHwnd(), hdc);

	vkUnmapMemory(fDevice, srcImage.GetMemoryHandle());

	fImagePool.Add(imageIdx);
	recursive_lock_unlock(&fLock);
	return VK_SUCCESS;
}


//#pragma mark - Instance

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

static void haikudrv_vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks *allocator)
{
	TRACE("()\n");
	vkDestroyInstance(instance, allocator);
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

static VkResult VKAPI_CALL haikudrv_vkCreateDevice(
    VkPhysicalDevice                            physicalDevice,
    const VkDeviceCreateInfo*                   pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDevice*                                   pDevice)
{
	TRACE("()\n");
	VkCheckRet(vkCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice));

	RecursiveLocker lock(&sDevicesLock);
	if (sDevices == NULL) {
		sDevices = new std::map<VkDevice, VkPhysicalDevice>();
	}
	sDevices->emplace(*pDevice, physicalDevice);

	return VK_SUCCESS;
}

static void VKAPI_CALL haikudrv_vkDestroyDevice(
    VkDevice                                    device,
    const VkAllocationCallbacks*                pAllocator)
{
	TRACE("()\n");

	{
		RecursiveLocker lock(&sDevicesLock);
		auto it = sDevices->find(device);
		sDevices->erase(it);
	}

	vkDestroyDevice(device, pAllocator);
}

//#pragma mark - Surface

static VkResult haikudrv_vkCreateWin32SurfaceKHR(VkInstance instance,
        const VkWin32SurfaceCreateInfoKHR *createInfo,
        const VkAllocationCallbacks *allocator, VkSurfaceKHR *surface)
{
	TRACE("()\n");

	auto wineSurface = new(std::nothrow) VKWineSurface();
	if (wineSurface == NULL)
		return VK_ERROR_OUT_OF_HOST_MEMORY;
	VkCheckRet(wineSurface->Init(instance, *createInfo));
	*surface = wineSurface->ToHandle();
	return VK_SUCCESS;
}

static void haikudrv_vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks *allocator)
{
	TRACE("()\n");
	delete VKWineSurface::FromHandle(surface);
}

static VkResult haikudrv_vkGetPhysicalDeviceSurfaceCapabilities2KHR(VkPhysicalDevice physDev,
        const VkPhysicalDeviceSurfaceInfo2KHR *surface_info, VkSurfaceCapabilities2KHR *capabilities)
{
	ERR("(): not implemented\n");
	return VK_NOT_READY;
}

static VkResult haikudrv_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice physDev,
        VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR *capabilities)
{
	TRACE("()\n");
	return VKWineSurface::FromHandle(surface)->GetCapabilities(physDev, capabilities);
}

static VkResult haikudrv_vkGetPhysicalDeviceSurfaceFormats2KHR(VkPhysicalDevice physDev,
        const VkPhysicalDeviceSurfaceInfo2KHR *surface_info, uint32_t *count, VkSurfaceFormat2KHR *formats)
{
	ERR("(): not implemented\n");
	return VK_NOT_READY;
}

static VkResult haikudrv_vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physDev,
        VkSurfaceKHR surface, uint32_t *count, VkSurfaceFormatKHR *formats)
{
	TRACE("()\n");
	return VKWineSurface::FromHandle(surface)->GetFormats(physDev, count, formats);
}

static VkResult haikudrv_vkGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice physDev,
        VkSurfaceKHR surface, uint32_t *count, VkPresentModeKHR *modes)
{
	TRACE("()\n");
	return VKWineSurface::FromHandle(surface)->GetPresentModes(physDev, count, modes);
}

static VkResult haikudrv_vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice physDev,
        uint32_t index, VkSurfaceKHR surface, VkBool32 *supported)
{
	TRACE("()\n");
	*supported = VK_TRUE;
	return VK_SUCCESS;
}

static VkBool32 haikudrv_vkGetPhysicalDeviceWin32PresentationSupportKHR(VkPhysicalDevice physDev, uint32_t index)
{
	TRACE("()\n");
	return VK_TRUE;
}

static VkResult VKAPI_CALL haikudrv_vkGetDeviceGroupSurfacePresentModesKHR(
	VkDevice device, VkSurfaceKHR surface, VkDeviceGroupPresentModeFlagsKHR *pModes
) {
	TRACE("()\n");
	*pModes = VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHR;
	return VK_SUCCESS;
}

static VkResult VKAPI_CALL haikudrv_vkGetPhysicalDevicePresentRectanglesKHR(
	VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pRectCount, VkRect2D* pRects
) {
	TRACE("()\n");
	return VKWineSurface::FromHandle(surface)->GetPresentRectangles(physicalDevice, pRectCount, pRects);
}

static VkSurfaceKHR haikudrv_wine_get_native_surface(VkSurfaceKHR surface)
{
	TRACE("0x%s\n", wine_dbgstr_longlong(surface));
	return surface;
}


//#pragma mark - Swapchain

static VkResult haikudrv_vkCreateSwapchainKHR(VkDevice device,
        const VkSwapchainCreateInfoKHR *createInfo,
        const VkAllocationCallbacks *allocator, VkSwapchainKHR *swapchain)
{
	TRACE("()\n");
	auto wineSwapchain = new(std::nothrow) VKWineSwapchain();
	if (wineSwapchain == NULL) return VK_ERROR_OUT_OF_HOST_MEMORY;
	VkCheckRet(wineSwapchain->Init(device, *createInfo));
	*swapchain = wineSwapchain->ToHandle();
	return VK_SUCCESS;
}

static void haikudrv_vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks *allocator)
{
	TRACE("()\n");
	delete VKWineSwapchain::FromHandle(swapchain);
}

static VkResult haikudrv_vkGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t *count, VkImage *images)
{
	TRACE("()\n");
	return VKWineSwapchain::FromHandle(swapchain)->GetSwapchainImages(count, images);
}

static VkResult haikudrv_vkAcquireNextImageKHR(
	VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t *pImageIndex
) {
	TRACE("()\n");
	VkAcquireNextImageInfoKHR info{
		.sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
		.swapchain = swapchain,
		.timeout = timeout,
		.semaphore = semaphore,
		.fence = fence
	};
	return VKWineSwapchain::FromHandle(swapchain)->AcquireNextImage(&info, pImageIndex);
}

static VkResult haikudrv_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo)
{
	TRACE("()\n");

	VkResult ret = VK_SUCCESS;
	for (uint32_t i = 0; i < pPresentInfo->swapchainCount; ++i) {
		auto *sc = VKWineSwapchain::FromHandle(pPresentInfo->pSwapchains[i]);
		VkResult res = sc->QueuePresent(queue, pPresentInfo, i);

		if (pPresentInfo->pResults != nullptr)
			pPresentInfo->pResults[i] = res;

		if (res != VK_SUCCESS && ret == VK_SUCCESS)
			ret = res;
	}

	return ret;
}


//#pragma mark -

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
    VULKAN_HOOK(vkGetDeviceGroupSurfacePresentModesKHR),
    VULKAN_HOOK(vkGetDeviceProcAddr),
    VULKAN_HOOK(vkGetInstanceProcAddr),
    VULKAN_HOOK(vkGetPhysicalDevicePresentRectanglesKHR),
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
    if (strcmp(name, "vkAcquireNextImageKHR") == 0)
    	return (void*)haikudrv_vkAcquireNextImageKHR;
    if (strcmp(name, "vkDestroyDevice") == 0)
    	return (void*)haikudrv_vkDestroyDevice;
    return get_vulkan_driver_device_proc_addr(&vulkan_funcs, name);
}

static void *haikudrv_get_vk_instance_proc_addr(VkInstance instance, const char *name)
{
    TRACE("(\"%s\")\n", name);
    if (strcmp(name, "vkCreateDevice") == 0)
    	return (void*)haikudrv_vkCreateDevice;
    return get_vulkan_driver_instance_proc_addr(&vulkan_funcs, instance, name);
}


const struct vulkan_funcs *HAIKUDRV_wine_get_vulkan_driver(UINT version)
{
    TRACE("()\n");
    if (version != WINE_VULKAN_DRIVER_VERSION)
    {
        ERR("version mismatch, vulkan wants %u but driver has %u\n", version, WINE_VULKAN_DRIVER_VERSION);
        return NULL;
    }

    return &vulkan_funcs;
}
