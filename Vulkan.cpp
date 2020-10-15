#include "./VulkanHeader.h"
#include "./Vulkan.h"
#include "./VulkanDebug.h"
#include "./VulkanClasses.h"

namespace VG {
#pragma region Macros
//Validate within the vulkan instance.
#define CheckVKRV(fname_, ...)              \
  do {                                      \
    VkResult res__ = (fname_(__VA_ARGS__)); \
    validateVkResult(res__, (#fname_));     \
  } while (0)

//Macro to load vulkan extension
#define VkLoadExt(_i, _v)                          \
  do {                                             \
    _v = (PFN_##_v)vkGetInstanceProcAddr(_i, #_v); \
    if (_v == nullptr) {                           \
      BRLogError("Could not find " + #_v);         \
    }                                              \
  } while (0)
#pragma endregion

#pragma region Vulkan_Internal

class Vulkan_Internal {
public:
  struct QueueFamilies {
    std::optional<uint32_t> _graphicsFamily;
    std::optional<uint32_t> _computeFamily;
    std::optional<uint32_t> _presentFamily;
  };

#pragma region Props
  VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
  VkDevice _device = VK_NULL_HANDLE;
  VkInstance _instance = VK_NULL_HANDLE;
  VkCommandPool _commandPool = VK_NULL_HANDLE;
  VkQueue _graphicsQueue = VK_NULL_HANDLE;  // Device queues are implicitly cleaned up when the device is destroyed, so we don't need to do anything in cleanup.
  VkQueue _presentQueue = VK_NULL_HANDLE;
  VkSampleCountFlagBits _maxMSAASamples = VK_SAMPLE_COUNT_1_BIT;
  VkSampleCountFlagBits _msaaSamples = VK_SAMPLE_COUNT_1_BIT;
  VkSurfaceKHR _windowSurface;
  bool _bEnableValidationLayers = true;  // TODO: set this in settings
  std::unique_ptr<QueueFamilies> _pQueueFamilies = nullptr;
  std::unordered_map<string_t, VkExtensionProperties> _deviceExtensions;
  std::unordered_map<std::string, VkLayerProperties> supported_validation_layers;

  //Extension functions
  VkExtFn(vkCreateDebugUtilsMessengerEXT);
  // PFN_vkCreateDebugUtilsMessengerEXT
  // vkCreateDebugUtilsMessengerEXT;
  VkExtFn(vkDestroyDebugUtilsMessengerEXT);
  VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
  VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

#pragma endregion

  Vulkan_Internal(const string_t& title, SDL_Window* win) {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = title.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createinfo{};
    createinfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createinfo.pApplicationInfo = &appInfo;

    //Validation layers
    std::vector<const char*> layerNames = getValidationLayers();
    if (_bEnableValidationLayers) {
      createinfo.enabledLayerCount = layerNames.size();
      createinfo.ppEnabledLayerNames = layerNames.data();
    }
    else {
      createinfo.enabledLayerCount = 0;
      createinfo.ppEnabledLayerNames = nullptr;
    }

    //Extensions
    std::vector<const char*> extensionNames = getRequiredExtensionNames(win);
    createinfo.enabledExtensionCount = extensionNames.size();
    createinfo.ppEnabledExtensionNames = extensionNames.data();

    populateDebugMessangerCreateInfo();
    createinfo.pNext = nullptr;  //(VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    createinfo.flags = 0;

    CheckVKRV(vkCreateInstance, &createinfo, nullptr, &_instance);

    if (!SDL_Vulkan_CreateSurface(win, _instance, &_windowSurface)) {
      checkErrors();
      errorExit("SDL failed to create vulkan window.");
    }

    debugPrintSupportedExtensions();

    loadExtensions();
    setupDebug();
    pickPhysicalDevice();
    createLogicalDevice();
    createCommandPool();
    // You can log every vulkan call to stdout.
  }
  virtual ~Vulkan_Internal() {
    vkDestroyDevice(_device, nullptr);
    if (_bEnableValidationLayers) {
      vkDestroyDebugUtilsMessengerEXT(_instance, debugMessenger, nullptr);
    }
    vkDestroyInstance(_instance, nullptr);
  }

#pragma region Debug Messanger
  //TODO: move this to the VulkanDebug class.

  void loadExtensions() {
    // Quick macro.

    // Load Extensions
    VkLoadExt(_instance, vkCreateDebugUtilsMessengerEXT);
    VkLoadExt(_instance, vkDestroyDebugUtilsMessengerEXT);
  }
  VkDebugUtilsMessengerCreateInfoEXT populateDebugMessangerCreateInfo() {
    debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugCreateInfo.flags = 0;
    debugCreateInfo.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugCreateInfo.pfnUserCallback = debugCallback;
    debugCreateInfo.pUserData = nullptr;
    return debugCreateInfo;
  }
  void setupDebug() {
    if (!_bEnableValidationLayers) {
      return;
    }
    CheckVKRV(vkCreateDebugUtilsMessengerEXT, _instance, &debugCreateInfo, nullptr, &debugMessenger);
  }
  std::vector<const char*> getRequiredExtensionNames(SDL_Window* win) {
    std::vector<const char*> extensionNames{};
    //TODO: SDL_Vulkan_GetInstanceExtensions -the window parameter may not need to be valid in future releases.
    //**test this.
    //Returns # of REQUIRED instance extensions
    unsigned int extensionCount;
    if (!SDL_Vulkan_GetInstanceExtensions(win, &extensionCount, nullptr)) {
      errorExit("Couldn't get instance extensions");
    }
    extensionNames = std::vector<const char*>(extensionCount);
    if (!SDL_Vulkan_GetInstanceExtensions(win, &extensionCount,
                                          extensionNames.data())) {
      errorExit("Couldn't get instance extensions (2)");
    }

    // Debug print the extension names.
    std::string exts = "";
    std::string del = "";
    for (const char* st : extensionNames) {
      exts += del + std::string(st) + "\r\n";
      del = "  ";
    }
    BRLogInfo("Available Vulkan Extensions: \r\n" + exts);

    if (_bEnableValidationLayers) {
      extensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensionNames;
  }
  std::vector<const char*> getValidationLayers() {
    std::vector<const char*> layerNames{};
    if (_bEnableValidationLayers) {
      layerNames.push_back("VK_LAYER_LUNARG_standard_validation");
    }

    //Check if validation layers are supported.
    string_t str = "";
    for (auto layer : layerNames) {
      if (!isValidationLayerSupported(layer)) {
        str += Stz layer + "\r\n";
      }
    }
    if (str.length()) {
      errorExit("One or more validation layers are not supported:\r\n" + str);
    }
    str = "Enabling Validation Layers: \r\n";
    for (auto layer : layerNames) {
      str += Stz "  " + layer;
    }
    BRLogInfo(str);

    return layerNames;
  }
  bool isValidationLayerSupported(const string_t& name) {
    if (supported_validation_layers.size() == 0) {
      std::vector<string_t> names;
      uint32_t layerCount;

      CheckVKRV(vkEnumerateInstanceLayerProperties, &layerCount, nullptr);
      std::vector<VkLayerProperties> availableLayers(layerCount);
      CheckVKRV(vkEnumerateInstanceLayerProperties, &layerCount, availableLayers.data());

      for (auto layer : availableLayers) {
        supported_validation_layers.insert(std::make_pair(layer.layerName, layer));
      }
    }

    if (supported_validation_layers.find(name) != supported_validation_layers.end()) {
      return true;
    }

    return false;
  }
  void debugPrintSupportedExtensions() {
    // Get extension properties.
    uint32_t extensionCount = 0;
    CheckVKRV(vkEnumerateInstanceExtensionProperties, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    CheckVKRV(vkEnumerateInstanceExtensionProperties, nullptr, &extensionCount, extensions.data());
    string_t st = "Supported Vulkan Extensions:" + Os::newline() +
                  "Version   Extension" + Os::newline();
    for (auto ext : extensions) {
      st += Stz "  [" + std::to_string(ext.specVersion) + "] " + ext.extensionName + Os::newline();
    }
    BRLogInfo(st);
  }
  static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                      VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                      const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                      void* pUserData) {
    std::string msghead = "[GPU]";
    if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
      msghead += Stz "[G]";
    }
    else if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
      msghead += Stz "[V]";
    }
    else if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
      msghead += Stz "[P]";
    }
    else {
      msghead += Stz "[?]";
    }

    std::string msg = "";
    if (pCallbackData != nullptr) {
      msg = std::string(pCallbackData->pMessage);
    }

    if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
      msghead += Stz "[V]";
      msghead += Stz ":";
      BRLogInfo(msghead + msg);
    }
    else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
      msghead += Stz "[I]";
      msghead += Stz ":";
      BRLogInfo(msghead + msg);
    }
    else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
      msghead += Stz "[W]";
      msghead += Stz ":";
      BRLogWarn(msghead + msg);
    }
    else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
      msghead += Stz "[E]";
      msghead += Stz ":";
      BRLogError(msghead + msg);
    }
    else {
      msghead += Stz "[?]";
      msghead += Stz ":";
      BRLogWarn(msghead + msg);
    }
    return VK_FALSE;
  }

#pragma endregion

#pragma region Setup

  void pickPhysicalDevice() {
    BRLogInfo("  Finding Physical Device.");

    //** TODO: some kind of operatino that lets us choose the best device.
    // Or let the user choose the device, right?

    uint32_t deviceCount = 0;
    CheckVKRV(vkEnumeratePhysicalDevices, _instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
      errorExit("No Vulkan enabled GPUs available.");
    }
    BRLogInfo("Found " + deviceCount + " rendering device(s).");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    CheckVKRV(vkEnumeratePhysicalDevices, _instance, &deviceCount, devices.data());

    // List all devices for debug, and also pick one.
    std::string devInfo = "";
    int i = 0;
    for (const auto& device : devices) {
      VkPhysicalDeviceProperties deviceProperties;
      VkPhysicalDeviceFeatures deviceFeatures;
      vkGetPhysicalDeviceProperties(device, &deviceProperties);
      vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

      devInfo += Stz " Device " + i + ": " + deviceProperties.deviceName + "\r\n";
      devInfo += Stz "  Driver Version: " + deviceProperties.driverVersion + "\r\n";
      devInfo += Stz "  API Version: " + deviceProperties.apiVersion + "\r\n";

      //**NOTE** deviceFeatures must be modified in the deviceFeatures in
      if (_physicalDevice == VK_NULL_HANDLE) {
        //We need to fix this to allow for optional samplerate shading.
        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
            deviceFeatures.geometryShader &&
            deviceFeatures.fillModeNonSolid &&
            deviceFeatures.samplerAnisotropy &&
            deviceFeatures.sampleRateShading) {
          _physicalDevice = device;
          _maxMSAASamples = getMaxUsableSampleCount();
          int n = 0;
          n++;
        }
      }

      i++;
    }
    BRLogInfo(devInfo);

    if (_physicalDevice == VK_NULL_HANDLE) {
      errorExit("Failed to find a suitable GPU.");
    }
  }
  std::unordered_map<string_t, VkExtensionProperties>& getDeviceExtensions() {
    if (_deviceExtensions.size() == 0) {
      // Extensions
      std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
      uint32_t extensionCount;
      CheckVKRV(vkEnumerateDeviceExtensionProperties, _physicalDevice, nullptr, &extensionCount, nullptr);
      std::vector<VkExtensionProperties> availableExtensions(extensionCount);
      CheckVKRV(vkEnumerateDeviceExtensionProperties, _physicalDevice, nullptr, &extensionCount, availableExtensions.data());
      for (auto ext : availableExtensions) {
        _deviceExtensions.insert(std::make_pair(ext.extensionName, ext));
      }
    }
    return _deviceExtensions;
  }
  bool isExtensionSupported(const string_t& extName) {
    std::string req_ext = std::string(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if (getDeviceExtensions().find(extName) != getDeviceExtensions().end()) {
      return true;
    }
    return false;
  }
  void createLogicalDevice() {
    //Here we should really do a "best fit" like we do for OpenGL contexts.
    BRLogInfo("Creating Logical Device.");

    //**NOTE** deviceFeatures must be modified in the deviceFeatures in
    // isDeviceSuitable
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.geometryShader = VK_TRUE;
    deviceFeatures.fillModeNonSolid = VK_TRUE;  // uh.. uh..
                                                //widelines, largepoints

    // Queues
    findQueueFamilies();

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { _pQueueFamilies->_graphicsFamily.value(),
                                               _pQueueFamilies->_presentFamily.value() };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
      VkDeviceQueueCreateInfo queueCreateInfo = {};
      queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueCreateInfo.queueFamilyIndex = queueFamily;
      queueCreateInfo.queueCount = 1;
      queueCreateInfo.pQueuePriorities = &queuePriority;
      queueCreateInfos.push_back(queueCreateInfo);
    }

    //Check Device Extensions
    const std::vector<const char*> deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    for (auto strext : deviceExtensions) {
      if (!isExtensionSupported(strext)) {
        errorExit(Stz "Extension " + strext + " wasn't supported");
      }
    }

    // Logical Device
    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    // Validation layers are Deprecated
    createInfo.enabledLayerCount = 0;

    CheckVKRV(vkCreateDevice, _physicalDevice, &createInfo, nullptr, &_device);

    // Create queues
    //**0 is the queue index - this should be checke to make sure that it's less than the queue family size.
    vkGetDeviceQueue(_device, _pQueueFamilies->_graphicsFamily.value(), 0, &_graphicsQueue);
    vkGetDeviceQueue(_device, _pQueueFamilies->_presentFamily.value(), 0, &_presentQueue);
  }
  QueueFamilies* findQueueFamilies() {
    if (_pQueueFamilies != nullptr) {
      return _pQueueFamilies.get();
    }
    _pQueueFamilies = std::make_unique<QueueFamilies>();

    //These specify the KIND of queue (command pool)
    //Ex: Grahpics, or Compute, or Present (really that's mostly what there is)
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueFamilyCount, queueFamilies.data());

    string_t qf_info = " Device Queue Families" + Os::newline();

    for (int i = 0; i < queueFamilies.size(); ++i) {
      auto& queueFamily = queueFamilies[i];
      // Check for presentation support
      VkBool32 presentSupport = false;
      CheckVKRV(vkGetPhysicalDeviceSurfaceSupportKHR, _physicalDevice, i, _windowSurface, &presentSupport);

      if (queueFamily.queueCount > 0 && presentSupport) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
          _pQueueFamilies->_graphicsFamily = i;
        }
        if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
          _pQueueFamilies->_computeFamily = i;
        }
        _pQueueFamilies->_presentFamily = i;
      }
    }

    BRLogInfo(qf_info);

    if (_pQueueFamilies->_graphicsFamily.has_value() == false || _pQueueFamilies->_presentFamily.has_value() == false) {
      errorExit("GPU doesn't contain any suitable queue families.");
    }

    return _pQueueFamilies.get();
  }
  VkSampleCountFlagBits getMaxUsableSampleCount() {
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(_physicalDevice, &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT) {
      return VK_SAMPLE_COUNT_64_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_32_BIT) {
      return VK_SAMPLE_COUNT_32_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_16_BIT) {
      return VK_SAMPLE_COUNT_16_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_8_BIT) {
      return VK_SAMPLE_COUNT_8_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_4_BIT) {
      return VK_SAMPLE_COUNT_4_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_2_BIT) {
      return VK_SAMPLE_COUNT_2_BIT;
    }

    return VK_SAMPLE_COUNT_1_BIT;
  }
  void createCommandPool() {
    BRLogInfo("Creating Command Pool.");

    //command pools allow you to do all the work in multiple threads.
    //std::vector<VkQueueFamilyProperties> queueFamilyIndices = findQueueFamilies();

    findQueueFamilies();

    VkCommandPoolCreateInfo poolInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .queueFamilyIndex = _pQueueFamilies->_graphicsFamily.value()
    };
    CheckVKRV(vkCreateCommandPool, _device, &poolInfo, nullptr, &_commandPool);
  }

#pragma endregion

#pragma region ErrorHandling

  //Error Handling
  void checkErrors() {
    SDLUtils::checkSDLErr();
  }
  void validateVkResult(VkResult res, const string_t& fname) {
    checkErrors();
    if (res != VK_SUCCESS) {
      errorExit(Stz "Error: '" + fname + "' returned '" + VulkanDebug::VkResult_toString(res) + "' (" + res + ")" + Os::newline());
    }
  }
  void errorExit(const string_t& str) {
    SDLUtils::checkSDLErr();
    BRLogError(str);

    Gu::debugBreak();

    BRThrowException(str);
  }
#pragma endregion
};

#pragma endregion

#pragma region Vulkan
Vulkan::Vulkan(const string_t& title, SDL_Window* win) {
  _pInt = std::make_unique<Vulkan_Internal>(title, win);
}
Vulkan::~Vulkan() {
  _pInt = nullptr;
}

VkSurfaceKHR& Vulkan::windowSurface() { return _pInt->_windowSurface; }
VkPhysicalDevice& Vulkan::physicalDevice() { return _pInt->_physicalDevice; }
VkDevice& Vulkan::device() { return _pInt->_device; }
VkInstance& Vulkan::instance() { return _pInt->_instance; }
VkCommandPool& Vulkan::commandPool() { return _pInt->_commandPool; }
VkQueue& Vulkan::graphicsQueue() { return _pInt->_graphicsQueue; }
VkQueue& Vulkan::presentQueue() { return _pInt->_presentQueue; }

#pragma region Errors
void Vulkan::checkErrors() {
  _pInt->checkErrors();
}
void Vulkan::validateVkResult(VkResult res, const string_t& fname) {
  _pInt->validateVkResult(res, fname);
}
void Vulkan::errorExit(const string_t& str) {
  _pInt->errorExit(str);
}
#pragma endregion

#pragma region Helpers
VkSampleCountFlagBits Vulkan::getMaxUsableSampleCount() {
  return _pInt->getMaxUsableSampleCount();
}
VkImageView Vulkan::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels) {
  VkImageViewCreateInfo createInfo = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,  //VkStructureType
    .pNext = nullptr,                                   //const void*
    .flags = 0,                                         //VkImageViewCreateFlags
    .image = image,                                     //VkImage
    .viewType = VK_IMAGE_VIEW_TYPE_2D,                  //VkImageViewType
    .format = format,                                   //VkFormat
    .components = {
      .r = VK_COMPONENT_SWIZZLE_IDENTITY,
      .g = VK_COMPONENT_SWIZZLE_IDENTITY,
      .b = VK_COMPONENT_SWIZZLE_IDENTITY,
      .a = VK_COMPONENT_SWIZZLE_IDENTITY,
    },  //VkComponentMapping
    .subresourceRange = {
      .aspectMask = aspectFlags,  // VkImageAspectFlags
      .baseMipLevel = 0,          // uint32_t
      .levelCount = mipLevels,    // uint32_t
      .baseArrayLayer = 0,        // uint32_t
      .layerCount = 1,            // uint32_t
    },                            //VkImageSubresourceRange

  };
  VkImageView ret;
  CheckVKRV(vkCreateImageView, device(), &createInfo, nullptr, &ret);
  return ret;
}
VkFormat Vulkan::findDepthFormat() {
  return findSupportedFormat(
    { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT },
    VK_IMAGE_TILING_OPTIMAL,
    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}
VkFormat Vulkan::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
  for (VkFormat format : candidates) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(this->physicalDevice(), format, &props);

    if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
      return format;
    }
    else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
      return format;
    }
  }

  throw std::runtime_error("failed to find supported format!");
}
uint32_t Vulkan::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties props;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice(), &props);

  for (uint32_t i = 0; i < props.memoryTypeCount; i++) {
    if (typeFilter & (1 << i) && (props.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }
  throw std::runtime_error("Failed to find valid memory type for vt buffer.");
  return 0;
}
VkCommandBuffer Vulkan::beginOneTimeGraphicsCommands() {
  VkCommandBufferAllocateInfo allocInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,  //VkStructureType
    .pNext = nullptr,                                         //const void*
    .commandPool = commandPool(),                             //VkCommandPool
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,                 //VkCommandBufferLevel
    .commandBufferCount = 1,                                  //uint32_t
  };

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(device(), &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,  //VkStructureType
    .pNext = nullptr,                                      //const void*
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,  //VkCommandBufferUsageFlags
    .pInheritanceInfo = nullptr,                           //const VkCommandBufferInheritanceInfo*
  };

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  return commandBuffer;
}
void Vulkan::endOneTimeGraphicsCommands(VkCommandBuffer commandBuffer) {
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,  //VkStructureType
    .pNext = nullptr,                        //const void*
    .pWaitSemaphores = nullptr,              //const VkSemaphore*
    .pWaitDstStageMask = nullptr,            //const VkPipelineStageFlags*
    .commandBufferCount = 1,                 //uint32_t
    .pCommandBuffers = &commandBuffer,       //const VkCommandBuffer*
    .signalSemaphoreCount = 0,               //uint32_t
    .pSignalSemaphores = nullptr,            //const VkSemaphore*
  };

  vkQueueSubmit(graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(graphicsQueue());

  vkFreeCommandBuffers(device(), commandPool(), 1, &commandBuffer);
}
#pragma endregion

#pragma endregion

}  // namespace VG
