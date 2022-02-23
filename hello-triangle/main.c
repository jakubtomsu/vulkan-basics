#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



#pragma comment(lib, "glfw3dll.lib")
#pragma comment(lib, "vulkan-1.lib")



#define WINDOW_SIZE_X  720
#define WINDOW_SIZE_Y 480

// heap memory allocator
#define heap_alloc(num_elements, elem_size)		malloc(num_elements * elem_size)
#define heap_alloc_zeroed(num_elements, elem_size)	calloc(num_elements, elem_size)
#define heap_free(heap_allocd_ptr)			free(heap_allocd_ptr)

#define ERROR_IF(condition, error_fmt, ...) if(condition) { printf("(!) error on line %i: " error_fmt, __LINE__, ##__VA_ARGS__); exit(-1); }



#define CLEAR_COLOR {0.0f, 0.5f, 0.5f, 1.0f}

typedef struct ren_vulkan_data_t {
	VkInstance	instance;
	VkDevice	device;
	int		images_count;
	VkImage*	images;
	VkFence*	fences;
	VkSwapchainKHR	swapchain;
	VkSurfaceKHR	surface;
	VkCommandPool	cmd_pool;
} ren_vulkan_data_t;
static ren_vulkan_data_t ren_vulkan_data = {0};

static GLFWwindow* ren_glfw_window;



void _ren_vulkan_init(); // TODO
void _ren_vulkan_deinit(); // TODO

char* read_entire_file_from_filename(char* fullpath, size_t* const bytes_read);



// mesh data
float vertices[] = {
	// Position			color
	 1.0f,	1.0f, 0.0f, 1.0f,	0.0f,	0.0f,
	-1.0f,	1.0f, 0.0f, 0.0f,	1.0f,	0.0f,
	 0.0f, -1.0f, 0.0f, 0.0f,	0.0f,	1.0f
};
int indices[] = {0, 1, 2};
// Projection Matrix (60deg FOV, 3:2 aspect ratio, [1.0, 256.0] clipping plane range)
// hardcoded for simplicity
float mvp[] = {
	1.155,	0.000,	0.000,	0.000,
	0.000,	1.732,	0.000,	0.000,
	0.000,	0.000, -1.008, -1.000,
	0.000,	0.000, -2.008,	0.000,
	// Model Matrix (identity)
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f,
	// View Matrix (distance of 2.5)
	1.0f,  0.0f,  0.0f,	 0.0f,
	0.0f,  1.0f,  0.0f,	 0.0f,
	0.0f,  0.0f,  1.0f,	 0.0f,
	0.0f,  0.0f, -2.5f,	 1.0f
};



int
main(int argc, char **argv) {
	VkResult res = {0}; // shared result variable

	// open window
	// initialize GLFW.
	// GLFW handles OS-specific interfaces such as creating and accessing a window and gathering input.
	{
		const int glfw_init_res = glfwInit();
		ERROR_IF(glfw_init_res != GLFW_TRUE, "GLFW failed to initialize");
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		ren_glfw_window = glfwCreateWindow(WINDOW_SIZE_X, WINDOW_SIZE_Y, "vulkan-hello-triangle", NULL, NULL);
		ERROR_IF(!ren_glfw_window, "Error creating a GLFW window\n");
	}
	
	// create vulkan instance
	{
		// Retrieves the names of the Vulkan *instance* extensions that are necessary.
		// If NULL is returned, then Vulkan is not usable (likely not installed).
		unsigned int n_inst_exts = 0;
		const char **req_inst_exts = glfwGetRequiredInstanceExtensions(&n_inst_exts);
		ERROR_IF(!req_inst_exts, "Could not find any Vulkan extensions\n");

		// Create a Vulkan Instance.
		// We provide Vulkan information about our program and the extensions available on this system,
		// and it returns a unique Vulkan instance
		VkApplicationInfo app_info = {0};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = "vulkan-hello-triangle";
		app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.pEngineName = "No Engine";
		app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo create_info = {0};
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info.pApplicationInfo = &app_info;
		create_info.enabledExtensionCount = n_inst_exts;
		create_info.ppEnabledExtensionNames = req_inst_exts;

		res = vkCreateInstance(&create_info, NULL, &ren_vulkan_data.instance);
		ERROR_IF(res != VK_SUCCESS, "vkCreateInstance() failed (%d)\n", res);
	}


	// create vulkan device
	VkPhysicalDevice physical_device = {0};
	int queue_index = -1;
	const char** dev_exts;
	VkExtensionProperties* dev_ext_props;
	{
		// Determine the list of graphics hardware devices in this computer.
		// In this example we just select the first ren_vulkan_data.device on the list.
		int physical_device_count = 0;
		res = vkEnumeratePhysicalDevices(ren_vulkan_data.instance, &physical_device_count, NULL);
		ERROR_IF(physical_device_count <= 0, "No graphics hardware was found (physical ren_vulkan_data.device count = %d) (%d)\n", physical_device_count, res);

		physical_device_count = 1;
		res = vkEnumeratePhysicalDevices(ren_vulkan_data.instance, &physical_device_count, &physical_device);
		ERROR_IF(res != VK_SUCCESS, "vkEnumeratePhysicalDevices() failed (%d)\n", res);

		// Determine which queue family to use.
		// In this case, we just look for the first one that can do graphics.
		int n_queues = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &n_queues, NULL);
		ERROR_IF(n_queues <= 0, "No queue families were found\n");

		VkQueueFamilyProperties* qfp = heap_alloc(n_queues, sizeof(VkQueueFamilyProperties));
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &n_queues, qfp);

		for(int i = 0; i < n_queues; i++) {
			if(qfp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				queue_index = i;
				break;
			}
		}
		heap_free(qfp);

		ERROR_IF(queue_index < 0, "Could not find a queue family with graphics support\n");
		// Check that the chosen queue family supports presentation.
		ERROR_IF(!glfwGetPhysicalDevicePresentationSupport(ren_vulkan_data.instance, physical_device, queue_index), "The selected queue family does not support present mode\n");

		// Get all Vulkan *device* extensions (as opposed to ren_vulkan_data.instance extensions)
		unsigned int n_dev_exts = 0;
		res = vkEnumerateDeviceExtensionProperties(physical_device, NULL, &n_dev_exts, NULL);
		ERROR_IF(n_dev_exts <= 0 || res != VK_SUCCESS, "Could not find any Vulkan device extensions (found %d, error %d)\n", n_dev_exts, res);

		dev_ext_props = heap_alloc_zeroed(n_dev_exts, sizeof(VkExtensionProperties));
		res = vkEnumerateDeviceExtensionProperties(physical_device, NULL, &n_dev_exts, dev_ext_props);
		ERROR_IF(res != VK_SUCCESS, "vkEnumerateDeviceExtensionProperties() failed (%d)\n", res);

		dev_exts = heap_alloc_zeroed(n_dev_exts, sizeof(void*));
		for(int i = 0; i < n_dev_exts; i++) {
			dev_exts[i] = &dev_ext_props[i].extensionName[0];
		}

		// Create a virtual device for Vulkan.
		// We pass in information regarding the hardware features we want to use as well as the set of queues,
		// which are essentially the interface between our program and the GPU.
		float priority = 0.0f;
		VkDeviceQueueCreateInfo queue_info = {0};
		queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_info.queueFamilyIndex = queue_index;
		queue_info.queueCount = 1;
		queue_info.pQueuePriorities = &priority;

		VkDeviceCreateInfo device_info = {0};
		device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_info.queueCreateInfoCount = 1;
		device_info.pQueueCreateInfos = &queue_info;
		device_info.enabledExtensionCount = n_dev_exts;
		device_info.ppEnabledExtensionNames = dev_exts;

		res = vkCreateDevice(physical_device, &device_info, NULL, &ren_vulkan_data.device);
		ERROR_IF(res != VK_SUCCESS, "vkCreateDevice() failed (%d)\n", res);
	}

	// Get implementation-specific function pointers.
	// This lets us use parts of the Vulkan API that aren't generalised.
	PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR	GetPhysicalDeviceSurfaceCapabilitiesKHR;
	PFN_vkGetPhysicalDeviceSurfaceFormatsKHR	GetPhysicalDeviceSurfaceFormatsKHR;
	PFN_vkCreateSwapchainKHR			CreateSwapchainKHR;
	PFN_vkDestroySwapchainKHR			DestroySwapchainKHR;
	PFN_vkGetSwapchainImagesKHR			GetSwapchainImagesKHR;
	PFN_vkAcquireNextImageKHR			AcquireNextImageKHR;
	PFN_vkQueuePresentKHR				QueuePresentKHR;
	{
		const int total_fptrs = 7;
		int tally = 0;

		GetPhysicalDeviceSurfaceCapabilitiesKHR
			= (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)vkGetInstanceProcAddr(ren_vulkan_data.instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
		GetPhysicalDeviceSurfaceFormatsKHR
			= (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)vkGetInstanceProcAddr(ren_vulkan_data.instance, "vkGetPhysicalDeviceSurfaceFormatsKHR");

		CreateSwapchainKHR	= (PFN_vkCreateSwapchainKHR)	vkGetDeviceProcAddr(ren_vulkan_data.device, "vkCreateSwapchainKHR");
		DestroySwapchainKHR	= (PFN_vkDestroySwapchainKHR)	vkGetDeviceProcAddr(ren_vulkan_data.device, "vkDestroySwapchainKHR");
		GetSwapchainImagesKHR	= (PFN_vkGetSwapchainImagesKHR)	vkGetDeviceProcAddr(ren_vulkan_data.device, "vkGetSwapchainImagesKHR");
		AcquireNextImageKHR	= (PFN_vkAcquireNextImageKHR)	vkGetDeviceProcAddr(ren_vulkan_data.device, "vkAcquireNextImageKHR");
		QueuePresentKHR		= (PFN_vkQueuePresentKHR)	vkGetDeviceProcAddr(ren_vulkan_data.device, "vkQueuePresentKHR");

		tally += GetPhysicalDeviceSurfaceCapabilitiesKHR != NULL;
		tally += GetPhysicalDeviceSurfaceFormatsKHR != NULL;
		tally += CreateSwapchainKHR != NULL;
		tally += DestroySwapchainKHR != NULL;
		tally += GetSwapchainImagesKHR != NULL;
		tally += AcquireNextImageKHR != NULL;
		tally += QueuePresentKHR != NULL;

		ERROR_IF(tally != total_fptrs, "Error loading KHR extension methods (found %d/%d)\n", tally, total_fptrs);
	}

	// Creating the window surface.
	// In this example I use GLFW's equivalent API, which is platform-agnostic.
	VkSurfaceFormatKHR color_fmt = {0};
	VkCompositeAlphaFlagBitsKHR alpha_fmt = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	VkSurfaceCapabilitiesKHR surf_caps;
	{
		res = glfwCreateWindowSurface(ren_vulkan_data.instance, ren_glfw_window, NULL, &ren_vulkan_data.surface);
		ERROR_IF(res != VK_SUCCESS, "glfwCreateWindowSurface() failed (%d)\n", res);

		// Determine the color format.
		int n_color_formats = 0;
		res = GetPhysicalDeviceSurfaceFormatsKHR(physical_device, ren_vulkan_data.surface, &n_color_formats, NULL);
		ERROR_IF(n_color_formats <= 0 || res != VK_SUCCESS, "Could not find any color formats for the window surface\n");

		VkSurfaceFormatKHR* colors = heap_alloc(n_color_formats, sizeof(VkSurfaceFormatKHR));
		res = GetPhysicalDeviceSurfaceFormatsKHR(physical_device, ren_vulkan_data.surface, &n_color_formats, colors);
		ERROR_IF(res != VK_SUCCESS, "GetPhysicalDeviceSurfaceFormatsKHR() failed (%d)\n", res);

		for(int i = 0; i < n_color_formats; i++) {
			if(colors[i].format == VK_FORMAT_B8G8R8A8_UNORM) {
				color_fmt = colors[i];
				break;
			}
		}
		heap_free(colors);

		ERROR_IF(color_fmt.format == VK_FORMAT_UNDEFINED, "The ren_glfw_window surface does not define a B8G8R8A8 color format\n");

		// Get information about the OS-specific surface.
		res = GetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, ren_vulkan_data.surface, &surf_caps);
		ERROR_IF(res != VK_SUCCESS, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR() failed (%d)\n", res);

		if(surf_caps.currentExtent.width == 0xffffffff) {
			surf_caps.currentExtent.width  = WINDOW_SIZE_X;
			surf_caps.currentExtent.height = WINDOW_SIZE_Y;
		}

		// This would be where you might want to call vkGetPhysicalDeviceSurfacePresentModeKHR()
		// to use a non-Vsync presentation mode

		// Select the composite alpha format.
		VkCompositeAlphaFlagBitsKHR alpha_list[] = {
			VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
		};

		for(int i = 0; i < sizeof(alpha_list) / sizeof(VkCompositeAlphaFlagBitsKHR); i++) {
			if(surf_caps.supportedCompositeAlpha & alpha_list[i]) {
				alpha_fmt = alpha_list[i];
				break;
			}
		}
	}

	// Create a swapchain
	// This lets us maintain a rotating cast of framebuffers.
	// In this example, we set it up for double-buffering.
	VkImageView* img_views;
	{
		int n_swap_images = surf_caps.minImageCount + 1;
		if(surf_caps.maxImageCount > 0 && n_swap_images > surf_caps.maxImageCount)
			n_swap_images = surf_caps.maxImageCount;

		VkImageUsageFlags img_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
			(surf_caps.supportedUsageFlags & (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT));

		VkSwapchainCreateInfoKHR swap_info = {0};
		swap_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swap_info.pNext = NULL;
		swap_info.surface = ren_vulkan_data.surface;
		swap_info.minImageCount = n_swap_images;
		swap_info.imageFormat = color_fmt.format;
		swap_info.imageColorSpace = color_fmt.colorSpace;
		swap_info.imageExtent = surf_caps.currentExtent;
		swap_info.imageUsage = img_usage;
		swap_info.preTransform = (VkSurfaceTransformFlagBitsKHR)surf_caps.currentTransform;
		swap_info.imageArrayLayers = 1;
		swap_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swap_info.queueFamilyIndexCount = 0;
		swap_info.pQueueFamilyIndices = NULL;
		swap_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
		swap_info.oldSwapchain = NULL;
		swap_info.clipped = VK_TRUE;
		swap_info.compositeAlpha = alpha_fmt;

		ren_vulkan_data.swapchain;
		res = CreateSwapchainKHR(ren_vulkan_data.device, &swap_info, NULL, &ren_vulkan_data.swapchain);
		ERROR_IF(res != VK_SUCCESS, "vkCreateSwapchainKHR() failed (%d)\n", res);

		// Get swapchain images
		// These are the endpoints for our framebuffers
		res = GetSwapchainImagesKHR(ren_vulkan_data.device, ren_vulkan_data.swapchain, &ren_vulkan_data.images_count, NULL);
		ERROR_IF(ren_vulkan_data.images_count <= 0 || res != VK_SUCCESS, "Could not find any swapchain images\n");

		ren_vulkan_data.images = heap_alloc_zeroed(ren_vulkan_data.images_count, sizeof(VkImage));
		res = GetSwapchainImagesKHR(ren_vulkan_data.device, ren_vulkan_data.swapchain, &ren_vulkan_data.images_count, ren_vulkan_data.images);
		ERROR_IF(res != VK_SUCCESS, "vkGetSwapchainImagesKHR() failed (%d)\n", res);

		// Create image views for the swapchain.
		VkImageViewCreateInfo iv_info = {0};
		iv_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		iv_info.pNext = NULL;
		iv_info.format = color_fmt.format;
		iv_info.components = (VkComponentMapping){
			.r = VK_COMPONENT_SWIZZLE_R,
			.g = VK_COMPONENT_SWIZZLE_G,
			.b = VK_COMPONENT_SWIZZLE_B,
			.a = VK_COMPONENT_SWIZZLE_A
		};
		iv_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		iv_info.subresourceRange.baseMipLevel = 0;
		iv_info.subresourceRange.levelCount = 1;
		iv_info.subresourceRange.baseArrayLayer = 0;
		iv_info.subresourceRange.layerCount = 1;
		iv_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		iv_info.flags = 0;

		img_views = heap_alloc_zeroed(ren_vulkan_data.images_count, sizeof(VkImageView));
		for(int i = 0; i < ren_vulkan_data.images_count; i++) {
			iv_info.image = ren_vulkan_data.images[i];
			res = vkCreateImageView(ren_vulkan_data.device, &iv_info, NULL, &img_views[i]);
			ERROR_IF(res != VK_SUCCESS, "vkCreateImageView() %d failed (%d)\n", i, res);
		}
	}


	// Create a command pool.
	// A command pool is essentially a thread-specific block of memory that is used for allocating commands.
	{
		VkCommandPoolCreateInfo cpool_info = {0};
		cpool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cpool_info.queueFamilyIndex = queue_index;
		cpool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		res = vkCreateCommandPool(ren_vulkan_data.device, &cpool_info, NULL, &ren_vulkan_data.cmd_pool);
		ERROR_IF(res != VK_SUCCESS, "vkCreateCommandPool() failed (%d)\n", res);
	}

	// Allocate command buffers - one for each image
	VkCommandBufferAllocateInfo cbuf_alloc_info = {0};
	cbuf_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbuf_alloc_info.commandPool = ren_vulkan_data.cmd_pool;
	cbuf_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cbuf_alloc_info.commandBufferCount = ren_vulkan_data.images_count;

	VkCommandBuffer* cmd_buffers = heap_alloc_zeroed(ren_vulkan_data.images_count, sizeof(VkCommandBuffer));
	res = vkAllocateCommandBuffers(ren_vulkan_data.device, &cbuf_alloc_info, cmd_buffers);
	ERROR_IF(res != VK_SUCCESS, "vkAllocateCommandBuffers() failed (%d)\n", res);

	// Select the depth format.
	// Used in the creation of the depth stencil.
	VkFormat formats[] = {
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D16_UNORM
	};

	VkFormat depth_fmt = VK_FORMAT_UNDEFINED;
	for(int i = 0; i < sizeof(formats) / sizeof(VkFormat); i++) {
		VkFormatProperties cfg;
		vkGetPhysicalDeviceFormatProperties(physical_device, formats[i], &cfg);
		if(cfg.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
			depth_fmt = formats[i];
			break;
		}
	}

	ERROR_IF(depth_fmt == VK_FORMAT_UNDEFINED, "Could not find a suitable depth format\n");

	// Create depth stencil image.
	VkImageCreateInfo dimg_info = {0};
	dimg_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	dimg_info.imageType = VK_IMAGE_TYPE_2D;
	dimg_info.format = depth_fmt;
	dimg_info.extent = (VkExtent3D){
		.width	= surf_caps.currentExtent.width,
		.height = surf_caps.currentExtent.height,
		.depth	= 1
	};
	dimg_info.mipLevels = 1;
	dimg_info.arrayLayers = 1;
	dimg_info.samples = VK_SAMPLE_COUNT_1_BIT;
	dimg_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	dimg_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VkImage depth_img;
	res = vkCreateImage(ren_vulkan_data.device, &dimg_info, NULL, &depth_img);
	ERROR_IF(res != VK_SUCCESS, "vkCreateImage() for depth stencil failed (%d)\n", res);

	// Allocate memory for the depth stencil.
	VkPhysicalDeviceMemoryProperties gpu_mem;
	vkGetPhysicalDeviceMemoryProperties(physical_device, &gpu_mem);

	VkMemoryRequirements mem_reqs;
	vkGetImageMemoryRequirements(ren_vulkan_data.device, depth_img, &mem_reqs);

	int mem_type_idx = -1;
	for(int i = 0; i < gpu_mem.memoryTypeCount; i++) {
		if((mem_reqs.memoryTypeBits & (1 << i))
			&& (gpu_mem.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
			mem_type_idx = i;
			break;
		}
	}

	ERROR_IF(mem_type_idx < 0, "Could not find a suitable type of graphics memory\n");

	VkMemoryAllocateInfo alloc_info = {0};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_reqs.size;
	alloc_info.memoryTypeIndex = mem_type_idx;

	VkDeviceMemory depth_mem;
	res = vkAllocateMemory(ren_vulkan_data.device, &alloc_info, NULL, &depth_mem);
	ERROR_IF(res != VK_SUCCESS, "vkAllocateMemory() for depth stencil failed (%d)\n", res);

	// Bind the newly allocated memory to the depth stencil image.
	res = vkBindImageMemory(ren_vulkan_data.device, depth_img, depth_mem, 0);
	ERROR_IF(res != VK_SUCCESS, "vkBindImageMemory() for depth stencil failed (%d)\n", res);

	// Create depth stencil view.
	// This is passed to each framebuffer.
	VkImageAspectFlagBits aspect =
		depth_fmt >= VK_FORMAT_D16_UNORM_S8_UINT ?
		VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT :
		VK_IMAGE_ASPECT_DEPTH_BIT;

	VkImageViewCreateInfo dview_info = {0};
	dview_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	dview_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	dview_info.image = depth_img;
	dview_info.format = depth_fmt;
	dview_info.subresourceRange.baseMipLevel = 0;
	dview_info.subresourceRange.levelCount = 1;
	dview_info.subresourceRange.baseArrayLayer = 0;
	dview_info.subresourceRange.layerCount = 1;
	dview_info.subresourceRange.aspectMask = aspect;

	VkImageView depth_view;
	res = vkCreateImageView(ren_vulkan_data.device, &dview_info, NULL, &depth_view);
	if(res != VK_SUCCESS) {
		fprintf(stderr, "vkCreateImageView() for depth stencil failed (%d)\n", res);
		return 23;
	}

	// Set up the render pass.
	VkAttachmentDescription attachments[] = {
		{ // Color attachment
			.flags			= 0,
			.format			= color_fmt.format,
			.samples		= VK_SAMPLE_COUNT_1_BIT,
			.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp		= VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp		= VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp 	= VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout		= VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout		= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		},
		{ // Depth attachment
			.flags			= 0,
			.format			= depth_fmt,
			.samples		= VK_SAMPLE_COUNT_1_BIT,
			.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp		= VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp		= VK_ATTACHMENT_LOAD_OP_CLEAR,
			.stencilStoreOp		= VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout		= VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		},
	};

	VkAttachmentReference color_ref = {0};
	color_ref.attachment = 0;
	color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depth_ref = {0};
	depth_ref.attachment = 1;
	depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {0};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_ref;
	subpass.pDepthStencilAttachment = &depth_ref;

	VkSubpassDependency dependencies[] = {
		{
			.srcSubpass		= VK_SUBPASS_EXTERNAL,
			.dstSubpass		= 0,
			.srcStageMask		= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			.dstStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask		= VK_ACCESS_MEMORY_READ_BIT,
			.dstAccessMask		= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dependencyFlags	= VK_DEPENDENCY_BY_REGION_BIT
		},
		{
			.srcSubpass		= 0,
			.dstSubpass		= VK_SUBPASS_EXTERNAL,
			.srcStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask		= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			.srcAccessMask		= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask		= VK_ACCESS_MEMORY_READ_BIT,
			.dependencyFlags	= VK_DEPENDENCY_BY_REGION_BIT
		}
	};

	VkRenderPassCreateInfo pass_info = {0};
	pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	pass_info.attachmentCount = 2;
	pass_info.pAttachments = attachments;
	pass_info.subpassCount = 1;
	pass_info.pSubpasses = &subpass;
	pass_info.dependencyCount = 2;
	pass_info.pDependencies = dependencies;

	VkRenderPass renderpass;
	res = vkCreateRenderPass(ren_vulkan_data.device, &pass_info, NULL, &renderpass);
	if(res != VK_SUCCESS) {
		fprintf(stderr, "vkCreateRenderPass() failed (%d)\n", res);
		return 24;
	}

	// Create the frame buffers.
	VkImageView fb_views[2];
	fb_views[1] = depth_view;

	VkFramebufferCreateInfo fb_info = {0};
	fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fb_info.renderPass = renderpass;
	fb_info.attachmentCount = 2;
	fb_info.pAttachments = fb_views;
	fb_info.width = surf_caps.currentExtent.width;
	fb_info.height = surf_caps.currentExtent.height;
	fb_info.layers = 1;

	VkFramebuffer* fbuffers = heap_alloc_zeroed(ren_vulkan_data.images_count, sizeof(VkFramebuffer));
	for(int i = 0; i < ren_vulkan_data.images_count; i++) {
		fb_views[0] = img_views[i];
		res = vkCreateFramebuffer(ren_vulkan_data.device, &fb_info, NULL, &fbuffers[i]);
		ERROR_IF(res != VK_SUCCESS, "vkCreateFramebuffer() %d failed (%d)\n", i, res);
	}

	// Create semaphores for synchronising draw commands and image presentation.
	VkSemaphoreCreateInfo bake_sema = {0};
	bake_sema.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkSemaphore sema_present, sema_render;
	if(vkCreateSemaphore(ren_vulkan_data.device, &bake_sema, NULL, &sema_present) != VK_SUCCESS ||
		vkCreateSemaphore(ren_vulkan_data.device, &bake_sema, NULL, &sema_render) != VK_SUCCESS) {
		fprintf(stderr, "Failed to create Vulkan semaphores\n");
		return 26;
	}

	// Create wait fences - one for each image.
	VkFenceCreateInfo fence_info = {0};
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	ren_vulkan_data.fences = heap_alloc_zeroed(ren_vulkan_data.images_count, sizeof(VkFence));
	for(int i = 0; i < ren_vulkan_data.images_count; i++) {
		res = vkCreateFence(ren_vulkan_data.device, &fence_info, NULL, &ren_vulkan_data.fences[i]);
		ERROR_IF(res != VK_SUCCESS, "vkCreateFence() failed (%d)\n", res);
	}



	struct {
		void *bytes;
		int size;
		VkBufferUsageFlagBits usage;
		VkDeviceMemory memory;
		VkBuffer buffer;
	} data[] = {
		{(void*)vertices, 18 * sizeof(float),	VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,	VK_NULL_HANDLE, VK_NULL_HANDLE},
		{(void*)indices,   3 * sizeof(int),	VK_BUFFER_USAGE_INDEX_BUFFER_BIT,	VK_NULL_HANDLE, VK_NULL_HANDLE},
		{(void*)mvp,	  48 * sizeof(float), 	VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,	VK_NULL_HANDLE, VK_NULL_HANDLE},
	};

	for(int i = 0; i < 3; i++) {
		VkBufferCreateInfo buf_info = {0};
		buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buf_info.size = data[i].size;
		buf_info.usage = data[i].usage;

		res = vkCreateBuffer(ren_vulkan_data.device, &buf_info, NULL, &data[i].buffer);
		if(res != VK_SUCCESS) {
			fprintf(stderr, "vkCreateBuffer() %d failed (%d)\n", i, res);
			return 28;
		}

		VkMemoryRequirements mem_reqs;
		vkGetBufferMemoryRequirements(ren_vulkan_data.device, data[i].buffer, &mem_reqs);

		unsigned int flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		int type_idx = -1;
		for(int j = 0; j < gpu_mem.memoryTypeCount; j++) {
			if((mem_reqs.memoryTypeBits & (1 << j)) &&
				(gpu_mem.memoryTypes[j].propertyFlags & flags) == flags) {
				mem_type_idx = j;
				break;
			}
		}

		ERROR_IF(mem_type_idx < 0, "Could not find an appropriate memory type (%d)\n", i);

		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = mem_reqs.size;
		alloc_info.memoryTypeIndex = mem_type_idx;

		res = vkAllocateMemory(ren_vulkan_data.device, &alloc_info, NULL, &data[i].memory);
		ERROR_IF(res != VK_SUCCESS, "vkAllocateMemory() %d failed (%d)\n", i, res);

		void *buf;
		res = vkMapMemory(ren_vulkan_data.device, data[i].memory, 0, alloc_info.allocationSize, 0, &buf);
		ERROR_IF(res != VK_SUCCESS, "vkMapMemory() %d failed (%d)\n", i, res);

		memcpy(buf, data[i].bytes, data[i].size);
		vkUnmapMemory(ren_vulkan_data.device, data[i].memory);

		res = vkBindBufferMemory(ren_vulkan_data.device, data[i].buffer, data[i].memory, 0);
		ERROR_IF(res != VK_SUCCESS, "vkBindBufferMemory() %d failed (%d)\n", i, res);
	}

	// Describe the MVP to a uniform descriptor.
	VkDescriptorBufferInfo uniform_info = {0};
	uniform_info.buffer = data[2].buffer;
	uniform_info.offset = 0;
	uniform_info.range = data[2].size;

	// Create descriptor set layout.
	VkDescriptorSetLayoutBinding ds_bind = {0};
	ds_bind.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ds_bind.descriptorCount = 1;
	ds_bind.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutCreateInfo ds_info = {0};
	ds_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	ds_info.bindingCount = 1;
	ds_info.pBindings = &ds_bind;

	VkDescriptorSetLayout ds_layout;
	res = vkCreateDescriptorSetLayout(ren_vulkan_data.device, &ds_info, NULL, &ds_layout);
	ERROR_IF(res != VK_SUCCESS, "vkCreateDescriptorSetLayout() failed (%d)\n", res);

	// Create pipeline layout.
	VkPipelineLayoutCreateInfo pl_info = {0};
	pl_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_info.setLayoutCount = 1;
	pl_info.pSetLayouts = &ds_layout;

	VkPipelineLayout pl_layout;
	res = vkCreatePipelineLayout(ren_vulkan_data.device, &pl_info, NULL, &pl_layout);
	ERROR_IF(res != VK_SUCCESS, "vkCreatePipelineLayout() failed (%d)\n", res);

	// load shader file data
	size_t vert_shader_spv_size = 0;
	char* vert_shader_spv = read_entire_file_from_filename("./shader.vert.spv", &vert_shader_spv_size);
	size_t frag_shader_spv_size = 0;
	char* frag_shader_spv = read_entire_file_from_filename("./shader.frag.spv", &frag_shader_spv_size);

	// prepare shaders
	VkShaderModuleCreateInfo mod_info = {0};
	mod_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	mod_info.codeSize = frag_shader_spv_size;
	mod_info.pCode = (unsigned int*)frag_shader_spv;

	VkShaderModule frag_shader;
	res = vkCreateShaderModule(ren_vulkan_data.device, &mod_info, NULL, &frag_shader);
	ERROR_IF(res != VK_SUCCESS, "vkCreateShaderModule() for fragment shader failed (%d)\n", res);

	mod_info.codeSize = vert_shader_spv_size;
	mod_info.pCode = (unsigned int *)vert_shader_spv;

	VkShaderModule vert_shader;
	res = vkCreateShaderModule(ren_vulkan_data.device, &mod_info, NULL, &vert_shader);
	ERROR_IF(res != VK_SUCCESS, "vkCreateShaderModule() for vertex shader failed (%d)\n", res);

	// Create graphics pipeline.
	VkPipelineInputAssemblyStateCreateInfo asm_info = {0};
	asm_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	asm_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkPipelineRasterizationStateCreateInfo raster_info = {0};
	raster_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	raster_info.polygonMode = VK_POLYGON_MODE_FILL;
	raster_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	raster_info.lineWidth = 1.0f;

	VkPipelineColorBlendAttachmentState cblend_att = {0};
	cblend_att.colorWriteMask = 0xf;

	VkPipelineColorBlendStateCreateInfo cblend_info = {0};
	cblend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	cblend_info.attachmentCount = 1;
	cblend_info.pAttachments = &cblend_att;

	VkPipelineViewportStateCreateInfo vp_info = {0};
	vp_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vp_info.viewportCount = 1;
	vp_info.scissorCount = 1;

	VkDynamicState dyn_vars[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dyn_info = {0};
	dyn_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dyn_info.pDynamicStates = dyn_vars;
	dyn_info.dynamicStateCount = 2;

	VkPipelineDepthStencilStateCreateInfo depth_info = {0};
	depth_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_info.depthTestEnable = VK_TRUE;
	depth_info.depthWriteEnable = VK_TRUE;
	depth_info.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depth_info.back.failOp = VK_STENCIL_OP_KEEP;
	depth_info.back.passOp = VK_STENCIL_OP_KEEP;
	depth_info.back.compareOp = VK_COMPARE_OP_ALWAYS;
	depth_info.front = depth_info.back;

	VkPipelineMultisampleStateCreateInfo ms_info = {0};
	ms_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	ms_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkVertexInputBindingDescription vb_info = {0};
	vb_info.binding = 0;
	vb_info.stride = 6 * sizeof(float); // position and color
	vb_info.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription vert_att[] = {
		{ // Position
			.binding  = 0,
			.location = 0,
			.format	  = VK_FORMAT_R32G32B32_SFLOAT,
			.offset	  = 0
		},
		{ // Color
			.binding  = 0,
			.location = 1,
			.format	  = VK_FORMAT_R32G32B32_SFLOAT,
			.offset	  = 3 * sizeof(float)
		}
	};

	VkPipelineVertexInputStateCreateInfo vert_info = {0};
	vert_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vert_info.vertexBindingDescriptionCount = 1;
	vert_info.pVertexBindingDescriptions = &vb_info;
	vert_info.vertexAttributeDescriptionCount = 2;
	vert_info.pVertexAttributeDescriptions = vert_att;

	VkPipelineShaderStageCreateInfo shader_stages[] = {
		{
			.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage	= VK_SHADER_STAGE_VERTEX_BIT,
			.module = vert_shader,
			.pName	= "main"
		},
		{
			.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage	= VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = frag_shader,
			.pName	= "main"
		}
	};

	VkGraphicsPipelineCreateInfo pipe_info = {0};
	pipe_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipe_info.layout = pl_layout;
	pipe_info.stageCount = 2;
	pipe_info.pStages = shader_stages;
	pipe_info.pVertexInputState = &vert_info;
	pipe_info.pInputAssemblyState = &asm_info;
	pipe_info.pRasterizationState = &raster_info;
	pipe_info.pColorBlendState = &cblend_info;
	pipe_info.pMultisampleState = &ms_info;
	pipe_info.pViewportState = &vp_info;
	pipe_info.pDepthStencilState = &depth_info;
	pipe_info.renderPass = renderpass;
	pipe_info.pDynamicState = &dyn_info;

	VkPipeline pipeline;
	res = vkCreateGraphicsPipelines(ren_vulkan_data.device, VK_NULL_HANDLE, 1, &pipe_info, NULL, &pipeline);
	ERROR_IF(res != VK_SUCCESS, "vkCreateGraphicsPipelines() failed (%d)\n", res);

	// Destroy shader modules (now that they have already been incorporated into the pipeline).
	vkDestroyShaderModule(ren_vulkan_data.device, vert_shader, NULL);
	vkDestroyShaderModule(ren_vulkan_data.device, frag_shader, NULL);

	// Create a descriptor pool for our descriptor set.
	VkDescriptorPoolSize ps_info = {0};
	ps_info.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ps_info.descriptorCount = 1;

	VkDescriptorPoolCreateInfo dpool_info = {0};
	dpool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dpool_info.poolSizeCount = 1;
	dpool_info.pPoolSizes = &ps_info;
	dpool_info.maxSets = 1;

	VkDescriptorPool dpool;
	res = vkCreateDescriptorPool(ren_vulkan_data.device, &dpool_info, NULL, &dpool);
	ERROR_IF(res != VK_SUCCESS, "vkCreateDescriptorPool() failed (%d)\n", res);

	// Allocate a descriptor set.
	// Descriptor sets let us pass additional data into our shaders.
	VkDescriptorSetAllocateInfo ds_alloc_info = {0};
	ds_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	ds_alloc_info.descriptorPool = dpool;
	ds_alloc_info.descriptorSetCount = 1;
	ds_alloc_info.pSetLayouts = &ds_layout;

	VkDescriptorSet desc_set;
	res = vkAllocateDescriptorSets(ren_vulkan_data.device, &ds_alloc_info, &desc_set);
	ERROR_IF(res != VK_SUCCESS, "vkAllocateDescriptorSets() failed (%d)\n", res);

	// Set up the descriptor set.
	VkWriteDescriptorSet write_info = {0};
	write_info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write_info.dstSet = desc_set;
	write_info.descriptorCount = 1;
	write_info.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	write_info.pBufferInfo = &uniform_info;
	write_info.dstBinding = 0;

	vkUpdateDescriptorSets(ren_vulkan_data.device, 1, &write_info, 0, NULL);

	// Construct the command buffers.
	// This is where we place the draw commands, which are executed by the GPU later.
	VkCommandBufferBeginInfo cbuf_info = {0};
	cbuf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	VkClearValue clear_values[] = {
		{.color = {CLEAR_COLOR}},
		{.depthStencil = {1.0f, 0}},
	};

	VkRenderPassBeginInfo renderpass_info = {0};
	renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderpass_info.renderPass = renderpass;
	renderpass_info.renderArea.offset.x = 0;
	renderpass_info.renderArea.offset.y = 0;
	renderpass_info.renderArea.extent = surf_caps.currentExtent;
	renderpass_info.clearValueCount = 2;
	renderpass_info.pClearValues = clear_values;

	VkRect2D* scissor = &renderpass_info.renderArea;

	VkViewport viewport = {0};
	viewport.height = (float)surf_caps.currentExtent.height;
	viewport.width = (float)surf_caps.currentExtent.width;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	for(int i = 0; i < ren_vulkan_data.images_count; i++) {
		res = vkBeginCommandBuffer(cmd_buffers[i], &cbuf_info);
		ERROR_IF(res != VK_SUCCESS, "vkBeginCommandBuffer() %d failed (%d)\n", i, res);

		renderpass_info.framebuffer = fbuffers[i];
		vkCmdBeginRenderPass(cmd_buffers[i], &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdSetViewport(cmd_buffers[i], 0, 1, &viewport);
		vkCmdSetScissor(cmd_buffers[i], 0, 1, scissor);

		vkCmdBindDescriptorSets(cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pl_layout, 0, 1, &desc_set, 0, NULL);
		vkCmdBindPipeline(cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(cmd_buffers[i], 0, 1, &data[0].buffer, &offset);
		vkCmdBindIndexBuffer(cmd_buffers[i], data[1].buffer, 0, VK_INDEX_TYPE_UINT32);

		const int n_indices = 3;
		vkCmdDrawIndexed(cmd_buffers[i], n_indices, 1, 0, 0, 1);

		vkCmdEndRenderPass(cmd_buffers[i]);
		res = vkEndCommandBuffer(cmd_buffers[i]);
		ERROR_IF(res != VK_SUCCESS, "vkEndCommandBuffer() %d failed (%d)\n", i, res);
	}



	// Prepare main loop.
	VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubmitInfo submit_info = {0};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pWaitDstStageMask = &wait_stage;
	submit_info.pWaitSemaphores = &sema_present;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &sema_render;
	submit_info.signalSemaphoreCount = 1;
	submit_info.commandBufferCount = 1;

	VkPresentInfoKHR present_info = {0};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &ren_vulkan_data.swapchain;
	present_info.pWaitSemaphores = &sema_render;
	present_info.waitSemaphoreCount = 1;

	VkQueue queue;
	vkGetDeviceQueue(ren_vulkan_data.device, queue_index, 0, &queue);

	unsigned long long frame_num = 0;




	// main loop
	unsigned long long max64 = -1;
	while(!glfwWindowShouldClose(ren_glfw_window)) {
		frame_num++;
		glfwPollEvents(); // read input from GLFW

		// printf("frame %i\n", frame_num);

		int idx;
		res = AcquireNextImageKHR(ren_vulkan_data.device, ren_vulkan_data.swapchain, max64, sema_present, NULL, &idx);
		ERROR_IF(res != VK_SUCCESS, "vkAcquireNextImageKHR() failed (%d)\n", res);

		res = vkWaitForFences(ren_vulkan_data.device, 1, &ren_vulkan_data.fences[idx], VK_TRUE, max64);
		ERROR_IF(res != VK_SUCCESS, "vkWaitForFences() failed (%d)\n", res);

		res = vkResetFences(ren_vulkan_data.device, 1, &ren_vulkan_data.fences[idx]);
		ERROR_IF(res != VK_SUCCESS, "vkResetFences() failed (%d)\n", res);

		submit_info.pCommandBuffers = &cmd_buffers[idx];
		res = vkQueueSubmit(queue, 1, &submit_info, ren_vulkan_data.fences[idx]);
		ERROR_IF(res != VK_SUCCESS, "vkQueueSubmit() failed (%d)\n", res);

		present_info.pImageIndices = &idx;
		res = QueuePresentKHR(queue, &present_info);
		ERROR_IF(res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR, "vkQueuePresentKHR() failed (%d)\n", res);
	}




	// Clean-up.
	for(int i = 0; i < 3; i++) {
		vkDestroyBuffer(ren_vulkan_data.device, data[i].buffer, NULL);
		vkFreeMemory(ren_vulkan_data.device, data[i].memory, NULL);
	}

	vkDestroyImageView(ren_vulkan_data.device, depth_view, NULL);
	vkDestroyImage(ren_vulkan_data.device, depth_img, NULL);
	vkFreeMemory(ren_vulkan_data.device, depth_mem, NULL);

	vkDestroySemaphore(ren_vulkan_data.device, sema_present, NULL);
	vkDestroySemaphore(ren_vulkan_data.device, sema_render, NULL);

	for(int i = 0; i < ren_vulkan_data.images_count; i++) {
		vkDestroyFence(ren_vulkan_data.device, ren_vulkan_data.fences[i], NULL);
	}
	free(ren_vulkan_data.fences);

	for(int i = 0; i < ren_vulkan_data.images_count; i++) {
		vkDestroyFramebuffer(ren_vulkan_data.device, fbuffers[i], NULL);
	}
	free(fbuffers);

	//vkFreeCommandBuffers(ren_vulkan_data.device, ren_vulkan_data.cmd_pool, ren_vulkan_data.images_count, cmd_buffers);
	vkDestroyCommandPool(ren_vulkan_data.device, ren_vulkan_data.cmd_pool, NULL);
	free(cmd_buffers);

	vkDestroyDescriptorPool(ren_vulkan_data.device, dpool, NULL);
	vkDestroyDescriptorSetLayout(ren_vulkan_data.device, ds_layout, NULL);

	vkDestroyPipelineLayout(ren_vulkan_data.device, pl_layout, NULL);
	vkDestroyPipeline(ren_vulkan_data.device, pipeline, NULL);
	vkDestroyRenderPass(ren_vulkan_data.device, renderpass, NULL);

	for(int i = 0; i < ren_vulkan_data.images_count; i++) {
		vkDestroyImageView(ren_vulkan_data.device, img_views[i], NULL);
	}
	free(img_views);

	free(ren_vulkan_data.images);

	DestroySwapchainKHR(ren_vulkan_data.device, ren_vulkan_data.swapchain, NULL);
	vkDestroyDevice(ren_vulkan_data.device, NULL);

	vkDestroySurfaceKHR(ren_vulkan_data.instance, ren_vulkan_data.surface, NULL);
	vkDestroyInstance(ren_vulkan_data.instance, NULL);

	free((void*)dev_exts);
	free(dev_ext_props);



	// deinit GLFW
	{
		glfwDestroyWindow(ren_glfw_window);
		glfwTerminate();
	}



	return 0;
} // main



//! uses `heap_alloc`
// for binary files
static inline char*
read_entire_file_from_filename(const char* filename, size_t* const bytes_read) {
	printf("opening file `%s`...\n", filename);
	FILE* handle = fopen(filename, "rb");
	ERROR_IF(handle == (void*)0, "couldn't load file `%s`\n", filename);

	fseek(handle, 0L, SEEK_END);
	const size_t filesize = ftell(handle);
	// printf("filesize = %i\n", filesize);
	fseek(handle, 0, SEEK_SET);
	char* filedata = heap_alloc(filesize, 1);
	fread(filedata, 1, filesize, handle);
	fclose(handle);

	*bytes_read = filesize;
	return filedata;
} // read_entire_file_from_filename



// fully initialize Vulkan and it's default resources
void
_ren_vulkan_init() {
	// TODO
} // _ren_vulkan_init


void
_ren_vulkan_deinit() {
	// TODO
} // _ren_vulkan_deinit