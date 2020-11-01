#include "./VulkanHeader.h"
#include "./Vulkan.h"
#include "./VulkanDebug.h"
#include "./VulkanClasses.h"

namespace VG {

#pragma region Vulkan

std::shared_ptr<Vulkan> Vulkan::create(const string_t& title, SDL_Window* win, bool vsync_enabled, bool wait_fences, bool enableDebug) {
  std::shared_ptr<Vulkan> v = std::make_shared<Vulkan>();
  v->init(title, win, vsync_enabled, wait_fences, enableDebug);
  return v;
}
Vulkan::Vulkan() {
}
Vulkan::~Vulkan() {
  CheckVKRV(vkDeviceWaitIdle, _device);

  _pSwapchain = nullptr;
  _pQueueFamilies = nullptr;

  vkDestroyCommandPool(_device, _commandPool, nullptr);
  vkDestroyDevice(_device, nullptr);
  _pDebug = nullptr;
  vkDestroyInstance(_instance, nullptr);
}
void Vulkan::init(const string_t& title, SDL_Window* win, bool vsync_enabled, bool wait_fences, bool enableDebug) {
  AssertOrThrow2(win != nullptr);

  //Setup vulkan devices
  _vsync_enabled = vsync_enabled;
  _wait_fences = wait_fences;

  initVulkan(title, win, enableDebug);

  int win_w = 0, win_h = 0;
  SDL_GetWindowSize(win, &win_w, &win_h);

  //Create swapchain
  _pSwapchain = std::make_shared<Swapchain>(getThis<Vulkan>());
  _pSwapchain->initSwapchain(BR2::usize2(win_w, win_h));
}

void Vulkan::initVulkan(const string_t& title, SDL_Window* win, bool enableDebug) {
  _pDebug = std::make_unique<VulkanDebug>(getThis<Vulkan>(), enableDebug);
  createInstance(title, win);
  pickPhysicalDevice();
  createLogicalDevice();
  getDeviceProperties();
  createCommandPool();
}
void Vulkan::createInstance(const string_t& title, SDL_Window* win) {
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
    createinfo.enabledLayerCount = static_cast<uint32_t>(layerNames.size());
    createinfo.ppEnabledLayerNames = layerNames.data();
  }
  else {
    createinfo.enabledLayerCount = 0;
    createinfo.ppEnabledLayerNames = nullptr;
  }

  //Extensions
  std::vector<const char*> extensionNames = getRequiredExtensionNames(win);
  createinfo.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size());
  createinfo.ppEnabledExtensionNames = extensionNames.data();

  CheckVKRV(vkCreateInstance, &createinfo, nullptr, &_instance);

  if (!SDL_Vulkan_CreateSurface(win, _instance, &_windowSurface)) {
    checkErrors();
    errorExit("SDL failed to create vulkan window.");
  }
}

void Vulkan::errorExit(const string_t& str) {
  BRLogError(str);

  SDLUtils::checkSDLErr();
  Gu::debugBreak();

  BRThrowException(str);
}

std::vector<const char*> Vulkan::getRequiredExtensionNames(SDL_Window* win) {
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
    extensionNames.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
  }
  return extensionNames;
}
std::vector<const char*> Vulkan::getValidationLayers() {
  std::vector<const char*> layerNames{};
  if (_bEnableValidationLayers) {
    layerNames.push_back("VK_LAYER_LUNARG_standard_validation");
    //layerNames.push_back("VK_LAYER_KHRONOS_validation");
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
bool Vulkan::isValidationLayerSupported(const string_t& name) {
  if (supported_validation_layers.size() == 0) {
    std::vector<string_t> names;
    uint32_t layerCount;

    CheckVKRV(vkEnumerateInstanceLayerProperties, &layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    CheckVKRV(vkEnumerateInstanceLayerProperties, &layerCount, availableLayers.data());

    //    CheckVKRV(vkEnumerateInstanceExtensionProperties, nullptr, &layerCount, nullptr);
    //std::vector<VkExtensionProperties> asdfg(layerCount);
    //    CheckVKRV(vkEnumerateInstanceExtensionProperties, nullptr, &layerCount, asdfg.data());

    for (auto layer : availableLayers) {
      supported_validation_layers.insert(std::make_pair(layer.layerName, layer));
    }
  }

  if (supported_validation_layers.find(name) != supported_validation_layers.end()) {
    return true;
  }

  return false;
}

void Vulkan::pickPhysicalDevice() {
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

          // ** This is mostly debugging stuff - If the driver doens't have it we should create one without it.
          deviceFeatures.fillModeNonSolid &&  // VK_POLYGON_MODE
          deviceFeatures.wideLines &&
          deviceFeatures.largePoints &&

          //deviceFeatures. pipelineStatisticsQuery && // -- Query pools
          //imageCubeArray
          //logicOp should be supported

          //MSAA
          deviceFeatures.shaderStorageImageMultisample &&  //Again not necessary but pretty
          //AF
          deviceFeatures.samplerAnisotropy &&
          deviceFeatures.sampleRateShading) {
        _physicalDevice = device;
        _deviceProperties = deviceProperties;  //Make sure this comes before getMaxUsableSampleCount!
        _deviceFeatures = deviceFeatures;
        _bPhysicalDeviceAcquired = true;
      }
    }

    i++;
  }
  BRLogInfo(devInfo);

  if (_physicalDevice == VK_NULL_HANDLE) {
    errorExit("Failed to find a suitable GPU.");
  }
}
std::unordered_map<string_t, VkExtensionProperties>& Vulkan::getDeviceExtensions() {
  if (_deviceExtensions.size() == 0) {
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
bool Vulkan::isExtensionSupported(const string_t& extName) {
  if (getDeviceExtensions().find(extName) != getDeviceExtensions().end()) {
    return true;
  }
  return false;
}
bool Vulkan::extensionEnabled(const string_t& in_ext) {
  auto it = _enabledExtensions.find(in_ext);
  return it != _enabledExtensions.end();
}
std::vector<const char*> Vulkan::getEnabledDeviceExtensions() {
  //Required Extensions
  const std::vector<const char*> requiredExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
  };
  //Optional
  const std::vector<const char*> optinalExtensions = {
    VK_AMD_MIXED_ATTACHMENT_SAMPLES_EXTENSION_NAME
  };

  string_t extMsg = "";
  bool fatal = false;
  std::vector<const char*> ret;
  //Check Required
  for (auto strext : requiredExtensions) {
    if (!isExtensionSupported(strext)) {
      extMsg += Stz "  Required extension " + strext + " wasn't supported\n";
      fatal = true;
    }
    else {
      ret.push_back(strext);
      _enabledExtensions.insert(string_t(strext));
    }
  }
  //Check Optional
  for (auto strext : optinalExtensions) {
    if (!isExtensionSupported(strext)) {
      extMsg += Stz "  Optional extension " + strext + " wasn't supported\n";
    }
    else {
      ret.push_back(strext);
      _enabledExtensions.insert(string_t(strext));
    }
  }
  if (extMsg.length()) {
    if (fatal) {
      errorExit(extMsg);
    }
    else {
      BRLogWarn(extMsg);
    }
  }

  return ret;
}
void Vulkan::createLogicalDevice() {
  //Here we should really do a "best fit" like we do for OpenGL contexts.
  BRLogInfo("Creating Logical Device.");

  //**NOTE** deviceFeatures must be modified in the deviceFeatures in
  // isDeviceSuitable
  VkPhysicalDeviceFeatures deviceFeatures{};
  deviceFeatures.geometryShader = VK_TRUE;
  deviceFeatures.fillModeNonSolid = VK_TRUE;
  //widelines, largepoints, individualBlendState

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
  std::vector<const char*> extensions = getEnabledDeviceExtensions();

  // Logical Device
  VkDeviceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
  createInfo.pQueueCreateInfos = queueCreateInfos.data();
  createInfo.pEnabledFeatures = &deviceFeatures;
  createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  createInfo.ppEnabledExtensionNames = extensions.data();

  // Validation layers are Deprecated
  createInfo.enabledLayerCount = 0;

  CheckVKRV(vkCreateDevice, _physicalDevice, &createInfo, nullptr, &_device);

  // Create queues
  //**0 is the queue index - this should be checke to make sure that it's less than the queue family size.
  vkGetDeviceQueue(_device, _pQueueFamilies->_graphicsFamily.value(), 0, &_graphicsQueue);
  vkGetDeviceQueue(_device, _pQueueFamilies->_presentFamily.value(), 0, &_presentQueue);
}
Vulkan::QueueFamilies* Vulkan::findQueueFamilies() {
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
void Vulkan::createCommandPool() {
  BRLogInfo("Creating Command Pool.");
  findQueueFamilies();

  VkCommandPoolCreateInfo poolInfo{
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .queueFamilyIndex = _pQueueFamilies->_graphicsFamily.value()
  };
  CheckVKRV(vkCreateCommandPool, _device, &poolInfo, nullptr, &_commandPool);
}
void Vulkan::getDeviceProperties() {
  CheckVKRV(vkGetPhysicalDeviceSurfaceCapabilitiesKHR, _physicalDevice, _windowSurface, &_surfaceCaps);
  _swapchainImageCount = _surfaceCaps.minImageCount + 1;
  //_swapchainImageCount = _surfaceCaps.maxImageCount;
  if (_swapchainImageCount > _surfaceCaps.maxImageCount) {
    _swapchainImageCount = _surfaceCaps.maxImageCount;
  }
  if (_surfaceCaps.maxImageCount > 0 && _swapchainImageCount > _surfaceCaps.maxImageCount) {
    _swapchainImageCount = _surfaceCaps.maxImageCount;
  }
}
void Vulkan::checkErrors() {
  SDLUtils::checkSDLErr();
}
void Vulkan::validateVkResult(VkResult res, const string_t& fname) {
  checkErrors();
  if (res != VK_SUCCESS) {
    errorExit(Stz "Error: '" + fname + "' returned '" + VulkanDebug::VkResult_toString(res) + "' (" + res + ")" + Os::newline());
  }
}

const VkPhysicalDeviceProperties& Vulkan::deviceProperties() {
  AssertOrThrow2(_bPhysicalDeviceAcquired);
  return _deviceProperties;
}
const VkPhysicalDeviceFeatures& Vulkan::deviceFeatures() {
  AssertOrThrow2(_bPhysicalDeviceAcquired);
  return _deviceFeatures;
}
const VkPhysicalDeviceLimits& Vulkan::deviceLimits() {
  AssertOrThrow2(_bPhysicalDeviceAcquired);
  return _deviceProperties.limits;
}
const VkSurfaceCapabilitiesKHR& Vulkan::surfaceCaps() {
  AssertOrThrow2(_bPhysicalDeviceAcquired);
  return _surfaceCaps;
}
MSAA Vulkan::maxMSAA() {
  AssertOrThrow2(_bPhysicalDeviceAcquired);
  VkSampleCountFlags counts = _deviceProperties.limits.framebufferColorSampleCounts &
                              _deviceProperties.limits.framebufferDepthSampleCounts;
  if (counts & VK_SAMPLE_COUNT_64_BIT) {
    return MSAA::MS_64_Samples;
  }
  else if (counts & VK_SAMPLE_COUNT_32_BIT) {
    return MSAA::MS_32_Samples;
  }
  else if (counts & VK_SAMPLE_COUNT_16_BIT) {
    return MSAA::MS_16_Samples;
  }
  else if (counts & VK_SAMPLE_COUNT_8_BIT) {
    return MSAA::MS_8_Samples;
  }
  else if (counts & VK_SAMPLE_COUNT_4_BIT) {
    return MSAA::MS_4_Samples;
  }
  else if (counts & VK_SAMPLE_COUNT_2_BIT) {
    return MSAA::MS_2_Samples;
  }
  return MSAA::Disabled;
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

VkCommandBuffer Vulkan::beginOneTimeGraphicsCommands() {
  VkCommandBufferAllocateInfo allocInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,  //VkStructureType
    .pNext = nullptr,                                         //const void*
    .commandPool = commandPool(),                             //VkCommandPool
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,                 //VkCommandBufferLevel
    .commandBufferCount = 1,                                  //uint32_t
  };

  VkCommandBuffer commandBuffer;
  CheckVKRV(vkAllocateCommandBuffers, device(), &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,  //VkStructureType
    .pNext = nullptr,                                      //const void*
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,  //VkCommandBufferUsageFlags
    .pInheritanceInfo = nullptr,                           //const VkCommandBufferInheritanceInfo*
  };

  CheckVKRV(vkBeginCommandBuffer, commandBuffer, &beginInfo);

  return commandBuffer;
}
void Vulkan::endOneTimeGraphicsCommands(VkCommandBuffer commandBuffer) {
  CheckVKRV(vkEndCommandBuffer, commandBuffer);

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

  CheckVKRV(vkQueueSubmit, graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
  CheckVKRV(vkQueueWaitIdle, graphicsQueue());

  vkFreeCommandBuffers(device(), commandPool(), 1, &commandBuffer);
}
uint32_t Vulkan::swapchainImageCount() {
  return _swapchainImageCount;
}
std::shared_ptr<Swapchain> Vulkan::swapchain() {
  return _pSwapchain;
}
void Vulkan::waitIdle() {
  //A fence that waits for the GPU to finish operations.
  CheckVKRV(vkDeviceWaitIdle, device());
}
float Vulkan::maxAF() {
  return deviceLimits().maxSamplerAnisotropy;
}

#pragma endregion

}  // namespace VG
