#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>
#include <vector>
#include <optional>
#include <set>
#include <fstream>

#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "glfw3.lib")

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

bool checkValidationLayerSupport()
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for(const auto& layerName : validationLayers)
    {
        bool layerFound = false;
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

std::vector<const char*> getRequiredExtensions()
{
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if(enableValidationLayers)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

VkResult CreateDebugUtilMessengerEXT(VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance,
        "vkCreateDebugUtilsMessengerEXT");
    if(func != nullptr)
    {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
        instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        func(instance, debugMessenger, pAllocator);
    }
}


class HelloTriangleApplication {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window;

    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;


    void initWindow() {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    void printExtensions()
    {
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::cout << extensionCount << " extensions supported\n";

        VkExtensionProperties extensions[32] = { 0 };
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions);
        for(int i = 0; i < extensionCount && i<32; i++)
        {
            const VkExtensionProperties& extension = extensions[i];
            std::cout << extension.extensionName << " at " << extension.specVersion << std::endl;
        }
    }
    
    void setupDebugMessenger()
    {
        if(!enableValidationLayers)
        {
            return;
        }

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        if(VK_SUCCESS != CreateDebugUtilMessengerEXT(instance, &createInfo, nullptr, &debugMessenger))
        {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    #pragma region Physical devices and queue families
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    void pickPhysicalDevice()
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if(deviceCount == 0)
        {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for(const auto& device : devices)
        {
            if(isDeviceSuitable(device))
            {
            	physicalDevice = device;
            	break;
            }
        }

    	if(physicalDevice == VK_NULL_HANDLE)
    	{
			throw std::runtime_error("failed to find a suitable GPU!");
    	}
    }

    bool isDeviceSuitable(VkPhysicalDevice device)
    {
    	QueueFamilyIndices indices = findQueueFamilies(device);

        bool extensionsSupported = checkDeviceExtensionSupport(device);

    	bool swapChainAdequate = false;
    	if(extensionsSupported)
    	{
    		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
    		swapChainAdequate = !swapChainSupport.formats.empty()
    			&& !swapChainSupport.presentModes.empty();
    	}

    	return indices.isComplete() && extensionsSupported && swapChainAdequate;
    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice device)
    {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
            availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : availableExtensions)
        {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphicsFamily;
    	std::optional<uint32_t> presentFamily;
    	bool isComplete()
    	{
    		return graphicsFamily.has_value() && presentFamily.has_value();
    	}
    };

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices;

    	uint32_t queueFamilyCount = 0;
    	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    	
    	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i=0;
    	for (const auto& queueFamily : queueFamilies)
    	{
    		if(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
    		{
    			indices.graphicsFamily = i;
    		}
    		
    		//Querying for presentation support
    		VkBool32 presentSupport = false;
    		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
    		if(presentSupport)
    		{
    			indices.presentFamily = i;
    		}
    		
    		if(indices.isComplete())
    		{
    			break;
    		}
    		i++;
    	}
    	
        return indices;
    }

    #pragma endregion

	#pragma region Logical device and queues
	VkDevice device;
    VkQueue graphicsQueue;
    void createLogicalDevice()
    {
    	//Specifying the queues to be created
	    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::vector<uint32_t> uniqueQueueFamilies =
        {
        	indices.graphicsFamily.value(),
        	indices.presentFamily.value()
        };
    	
    	float queuePriority = 1.0f;

    	for (uint32_t queueFamily : uniqueQueueFamilies)
    	{
    		VkDeviceQueueCreateInfo queueCreateInfo{};
    		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    		queueCreateInfo.queueFamilyIndex = queueFamily;
    		queueCreateInfo.queueCount = 1;
    		queueCreateInfo.pQueuePriorities = &queuePriority;
    		queueCreateInfos.push_back(queueCreateInfo);
    	}

    	//Specifying used device features
        VkPhysicalDeviceFeatures deviceFeatures {};

    	//Creating the logical device
        VkDeviceCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    	createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());

        createInfo.pEnabledFeatures = &deviceFeatures;

        //Enabling device extensions
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if(enableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

    	if(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
    	{
    		throw std::runtime_error("failed to create logical device!");
    	}

    	//Retrieving queue handles
    	vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    	vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
    }
	#pragma endregion

#pragma region Window surface
	VkSurfaceKHR surface = nullptr;
    void createSurface()
    {
    	//Window surface creation
        if(glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
        {
	        throw std::runtime_error("failed to create window surface!");
        }
    }
	VkQueue presentQueue = nullptr;//the presentation queue handle
#pragma endregion

#pragma region Swap chain
    const std::vector<const char*> deviceExtensions =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME//Checking for swap chain support
    };
    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device)
    {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if(formatCount != 0)
        {
        	details.formats.resize(formatCount);
        	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
                details.formats.data());
        }

    	uint32_t presentModeCount;
    	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    	if(presentModeCount != 0)
    	{
    		details.presentModes.resize(presentModeCount);
    		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount,
                details.presentModes.data());
    	}

        return details;
    }

	//Surface format
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
    {
	    for (const auto& availableFormat : availableFormats)
	    {
		    if(availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB
                && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		    {
			    return availableFormat;
		    }
	    }

    	return availableFormats[0];
    }

	//Presentation mode
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
    {
    	for (const auto& availablePresentMode : availablePresentModes)
        {
	        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
	        {
				return availablePresentMode;
	        }
	    }
	    return VK_PRESENT_MODE_FIFO_KHR;
    }

	//Swap extent
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
    {
	    if(capabilities.currentExtent.width != UINT32_MAX)
	    {
		    return capabilities.currentExtent;
	    }
    	
	    int width, height;
	    glfwGetFramebufferSize(window, &width, &height);

	    VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
	    actualExtent.width = std::max(capabilities.minImageExtent.width,
	                                  std::min(capabilities.maxImageExtent.width, actualExtent.width));
	    actualExtent.height = std::max(capabilities.minImageExtent.height,
	                                   std::min(capabilities.maxImageExtent.height, actualExtent.height));
	    return actualExtent;
    }

    //Create the swap chain
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    void createSwapChain()
    {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if(swapChainSupport.capabilities.maxImageCount > 0
            && imageCount > swapChainSupport.capabilities.maxImageCount)
        {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] =
            {indices.graphicsFamily.value(), indices.presentFamily.value()};
        if(indices.graphicsFamily != indices.presentFamily)
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;

        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;

        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create swap chain!");
        }

        //Retrieving the swap chain images
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }	
#pragma endregion

#pragma region Image views
	std::vector<VkImageView> swapChainImageViews;
    void createImageviews()
    {
    	swapChainImageViews.resize(swapChainImages.size());
        for (size_t i = 0; i < swapChainImages.size(); ++i)
        {
	        VkImageViewCreateInfo createInfo{};
        	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        	createInfo.image = swapChainImages[i];
        	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        	createInfo.format = swapChainImageFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        	createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        	createInfo.subresourceRange.baseMipLevel = 0;
        	createInfo.subresourceRange.levelCount = 1;
        	createInfo.subresourceRange.baseArrayLayer = 0;
        	createInfo.subresourceRange.layerCount = 1;
        	if(vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS)
        	{
        		throw std::runtime_error("failed to create image views!");
        	}
        }
    }
#pragma endregion

#pragma region Shader modules
	//Loading a shader
    static std::vector<char> readFile(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
    	file.close();

    	return buffer;
    }

	//Creating shader modules
	VkShaderModule createShaderModule(const std::vector<char>& code)
    {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create shader module!");
        }

        return shaderModule;
    }
#pragma endregion

#pragma region Fixed functions
    //Vertex input
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	void createVertexInput()
    {
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    	vertexInputInfo.vertexBindingDescriptionCount = 0;
    	vertexInputInfo.pVertexBindingDescriptions = nullptr;
    	vertexInputInfo.vertexAttributeDescriptionCount = 0;
    	vertexInputInfo.pVertexAttributeDescriptions = nullptr;
    }
	//Input assembly
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    void createInputAssembly()
    {
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;
    }
    //Viewports and scissors
    VkViewport viewport{};
    VkRect2D scissor{};
    VkPipelineViewportStateCreateInfo viewportState{};
    void createViewportAndScissor()
    {
    	viewport.x = 0.0f;
    	viewport.y = 0.0f;
    	viewport.width = (float) swapChainExtent.width;
    	viewport.height = (float) swapChainExtent.height;
    	viewport.minDepth = 0.0f;
    	viewport.maxDepth = 1.0f;

    	scissor.offset = {0, 0};
    	scissor.extent = swapChainExtent;

    	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    	viewportState.viewportCount = 1;
    	viewportState.pViewports = &viewport;
    	viewportState.scissorCount = 1;
    	viewportState.pScissors = &scissor;
    }
	//Rasterizer
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	void createRasterizer()
    {
    	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    	rasterizer.depthClampEnable = VK_FALSE;
    	rasterizer.rasterizerDiscardEnable = VK_FALSE;
    	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    	rasterizer.lineWidth = 1.0f;
    	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    	rasterizer.depthBiasEnable = VK_FALSE;
    	rasterizer.depthBiasConstantFactor = 0.0f;
    	rasterizer.depthBiasClamp = 0.0f;
    	rasterizer.depthBiasSlopeFactor = 0.0f;
    }
    //Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
	void createMultisampling()
    {
    	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    	multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 1.0f;
		multisampling.pSampleMask = nullptr;
		multisampling.alphaToCoverageEnable = VK_FALSE;
		multisampling.alphaToOneEnable = VK_FALSE;
    }
	//Depth and stencil testing (nullptr for now)
	//Color blending
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    VkPipelineColorBlendStateCreateInfo colorBlending{};
	void createColorBlending()
    {
    	colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
    		| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    	colorBlendAttachment.blendEnable = VK_FALSE;
    	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;
    }
	//Dynamic state
	VkDynamicState dynamicStates[2];
    VkPipelineDynamicStateCreateInfo dynamicState{};
	void createDynamicState()
    {
        dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
        dynamicStates[1] = VK_DYNAMIC_STATE_LINE_WIDTH;
    	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    	dynamicState.dynamicStateCount = 2;
    	dynamicState.pDynamicStates = dynamicStates;
    }
    void createFixedFunctions()
    {
        createVertexInput();
        createInputAssembly();
        createViewportAndScissor();
        createRasterizer();
        createMultisampling();
        createColorBlending();
        createDynamicState();
    }

	//Pipeline layout
    VkPipelineLayout pipelineLayout;
#pragma endregion 

#pragma region Render Pass
    VkRenderPass renderPass;
    void createRenderPass()
    {
        //Attachment description
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        //Subpasses and attachment references
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        //Render pass
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        if(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create render pass!");
        }
    }
#pragma endregion
    
    VkPipeline graphicsPipeline;
    void createGraphicsPipeline()
    {
        auto vertShaderCode = readFile("vert.spv");
        auto fragShaderCode = readFile("frag.spv");

        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        //Shader stage creation
        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";
        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";
        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    	//Pipeline layout
    	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    	pipelineLayoutInfo.setLayoutCount = 0;
    	pipelineLayoutInfo.pSetLayouts = nullptr;
    	pipelineLayoutInfo.pushConstantRangeCount = 0;
    	pipelineLayoutInfo.pPushConstantRanges = nullptr;

    	if(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    	{
    		throw std::runtime_error("failed to create pipeline layout!");
    	}

        createFixedFunctions();

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        if(vkCreateGraphicsPipelines(
            device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }

#pragma region Framebuffers
	std::vector<VkFramebuffer> swapChainFramebuffers;
    void createFramebuffers()
    {
    	swapChainFramebuffers.resize(swapChainImageViews.size());
        for (size_t i = 0; i < swapChainImageViews.size(); ++i)
        {
	        VkImageView attachments[] = { swapChainImageViews[i] };

        	VkFramebufferCreateInfo framebufferInfo{};
        	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        	framebufferInfo.renderPass = renderPass;
        	framebufferInfo.attachmentCount = 1;
        	framebufferInfo.pAttachments = attachments;
        	framebufferInfo.width = swapChainExtent.width;
        	framebufferInfo.height = swapChainExtent.height;
        	framebufferInfo.layers = 1;
        	if(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
        	{
        		throw std::runtime_error("failed to create framebuffer!");
        	}
        }
    }
#pragma endregion

#pragma region Command buffers
	//Command pools
    VkCommandPool commandPool;
    void createCommandPool()
    {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    	VkCommandPoolCreateInfo poolInfo{};
    	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    	poolInfo.flags = 0;

    	if(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
    	{
    		throw std::runtime_error("failed to create command pool!");
    	}
    }

	//Commnad buffer allocation
	std::vector<VkCommandBuffer> commandBuffers;
	void createCommandBuffers()
	{
		commandBuffers.resize(swapChainFramebuffers.size());
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
		if(vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate command buffer!");
		}
		
		//Starting command buffer recording
		for (size_t i = 0; i < commandBuffers.size(); ++i)
		{
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = 0;
			beginInfo.pInheritanceInfo = nullptr;
			if(vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to begin recording command buffer!");
			}
			
			//Starting a render pass
			VkRenderPassBeginInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = renderPass;
			renderPassInfo.framebuffer = swapChainFramebuffers[i];
            renderPassInfo.renderArea.offset = {0, 0};
			renderPassInfo.renderArea.extent = swapChainExtent;
			VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearColor;
			vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			//Basic drawing commands
			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
			vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

			//Finishing up
			vkCmdEndRenderPass(commandBuffers[i]);
			if(vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to record command buffer!");
			}
		}

	}

	
#pragma endregion

    void initVulkan()
    {
        printExtensions();
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
    	createLogicalDevice();
        createSwapChain();
        createImageviews();
        createRenderPass();
        createGraphicsPipeline();
    	createFramebuffers();
        createCommandPool();
		createCommandBuffers();
    }

    void mainLoop()
    {
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
        }
    }

    void cleanup()
    {
    	vkDestroyCommandPool(device, commandPool, nullptr);
	    for (auto framebuffer : swapChainFramebuffers)
	    {
		    vkDestroyFramebuffer(device, framebuffer, nullptr);
	    }
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);
    	
	    for (VkImageView imageView : swapChainImageViews)
	    {
		    vkDestroyImageView(device, imageView, nullptr);
	    }
        vkDestroySwapchainKHR(device, swapChain, nullptr);
    	vkDestroyDevice(device, nullptr);
    	
        if(enableValidationLayers)
        {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

    	vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);

        glfwTerminate();
    }

    void createInstance() {
        if(enableValidationLayers && !checkValidationLayerSupport())
        {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
        if(enableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = &debugCreateInfo;
        }
        else
        {
            createInfo.enabledLayerCount = 0;

            createInfo.pNext =  nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
    }
};

int main() {
    HelloTriangleApplication app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}