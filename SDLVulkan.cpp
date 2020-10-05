#include "./SDLVulkan.h"
#include "./VulkanDebug.h"
/*
  Queue family - a queue with a common set of characteristics, and a number of possible queues.
  Logical device - the features of the physical device that we are using.
    Multiple per phsical device.  

  initialize, SURFACE, create validation layers, pick physical device, create logical device from physical, get surface format from our surface,
   make swapchain, make swapchain images, make image views for images,
   create shader modules
   create pipeline


  opaque pointer
  opaque handle - 
  handle - abstarct refernce to some underlying implementation 
  subpass - render pass that depends ont he contents of the framebuffers of previous passes.
  ImageView - You can't access images direclty you need to use an image view.
  Push Constants - A small bank of values writable via the API and accessible in shaders. Push constants allow the application to set values used in shaders without creating buffers or modifying and binding descriptor sets for each update.
    https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#glossary

  Descriptor - (Resource Descriptor) - This is like a GL Buffer Binding. Tells Pipeline how to access and lay out memory for a SSBO or UBO.
  Fences - CPU->GPU synchronization - finish rendering all frames before next loop
  Semaphores -> GPU->GPU sync, finish one pipeline stage before continuing.

  Mesh Shaders - An NVidia extension that combines the primitive assembly stages as a single dispatched compute "mesh" stage (multiple threads)
  and a "task" sage.
    https://www.geeks3d.com/20200519/introduction-to-mesh-shaders-opengl-and-vulkan/
    - Meshlets are small meshes of a big mesh to render.
    - Meshes are decmoposed because each mesh shader is limited on the number of output meshes.
    - 
    
  Nvidia WARP unit
    https://www.geeks3d.com/hacklab/20180705/demo-visualizing-nvidia-gl_threadinwarpnv-gl_warpidnv-and-gl_smidnv-gl_nv_shader_thread_group/
    - it is a set of 32 fragment (pixel) threads.
    - warps are grouped in SM's (streaming multiprocessors)
    - each SM contains 64 warps - 2048 threads.
    - gtx 1080 contains 20 SMs
      - Each GPU core can run 16 threads
    - rtx 3080 has 68 SMs

  Deferred expression   

*/

//Macros
//Validate a Vk Result.
#define CheckVKR(fname_, emsg_, ...)                 \
  do {                                               \
    VkResult res__ = (fname_(__VA_ARGS__));          \
    validateVkResult(res__, (Stz emsg_), (#fname_)); \
  } while (0)
//Find Vk Extension
#define VkExtFn(_vkFn) PFN_##_vkFn _vkFn = nullptr;

namespace VG {

class SDLVulkan_Internal {
public:
  const int MAX_FRAMES_IN_FLIGHT = 2;
  std::vector<VkSemaphore> _imageAvailableSemaphores;
  std::vector<VkSemaphore> _renderFinishedSemaphores;
  size_t _currentFrame = 0;
  std::vector<VkFence> _inFlightFences;
  std::vector<VkFence> _imagesInFlight;

  SDL_Window* _pSDLWindow = nullptr;
  // std::vector<VkLayerProperties> _availableLayers;
  bool _bEnableValidationLayers = true;
  //VK_DEFINE_NON_DISPATCHABLE_HANDLE
  struct QueueFamilies {
    std::optional<uint32_t> _graphicsFamily;
    std::optional<uint32_t> _computeFamily;
    std::optional<uint32_t> _presentFamily;
  };
  std::unique_ptr<QueueFamilies> _pQueueFamilies = nullptr;

  VkSurfaceKHR _main_window_surface = VK_NULL_HANDLE;
  VkInstance _instance = VK_NULL_HANDLE;
  VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
  VkDevice _device = VK_NULL_HANDLE;
  VkSwapchainKHR _swapChain = VK_NULL_HANDLE;
  VkExtent2D _swapChainExtent;
  VkFormat _swapChainImageFormat;
  std::vector<VkImage> _swapChainImages;
  std::vector<VkImageView> _swapChainImageViews;
  VkQueue _graphicsQueue = VK_NULL_HANDLE;  // Device queues are implicitly cleaned up when the device is destroyed, so we don't need to do anything in cleanup.
  VkQueue _presentQueue = VK_NULL_HANDLE;
  VkRenderPass _renderPass = VK_NULL_HANDLE;
  VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;
  VkPipeline _graphicsPipeline = VK_NULL_HANDLE;
  VkShaderModule _vertShaderModule = VK_NULL_HANDLE;
  VkShaderModule _fragShaderModule = VK_NULL_HANDLE;
  std::vector<VkFramebuffer> _swapChainFramebuffers;
  VkCommandPool _commandPool = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> _commandBuffers;
  bool _bSwapChainOutOfDate = false;

  //Extension functions
  VkExtFn(vkCreateDebugUtilsMessengerEXT);
  // PFN_vkCreateDebugUtilsMessengerEXT
  // vkCreateDebugUtilsMessengerEXT;
  VkExtFn(vkDestroyDebugUtilsMessengerEXT);
#pragma region ErrorHandling

  //Error Handling
  void checkErrors() {
    SDLUtils::checkSDLErr();
  }
  void validateVkResult(VkResult res, const string_t& errmsg, const string_t& fname) {
    checkErrors();
    if (res != VK_SUCCESS) {
      errorExit(Stz "Error: '" + fname + "' returned '" + VulkanDebug::VkResult_toString(res) + "' (" + res + ")" + Os::newline() + (errmsg.length() ? (Stz "  Msg: " + errmsg) : Stz ""));
    }
  }
  void errorExit(const string_t& str) {
    SDLUtils::checkSDLErr();
    BRLogError(str);

    Gu::debugBreak();

    BRThrowException(str);
  }
#pragma endregion

#pragma region SDL
  SDL_Window* makeSDLWindow(const GraphicsWindowCreateParameters& params,
                            int render_system, bool show) {
    string_t title;
    bool bFullscreen = false;
    SDL_Window* ret = nullptr;

    int style_flags = 0;
    style_flags |= (show ? SDL_WINDOW_SHOWN : SDL_WINDOW_HIDDEN);
    if (params._type == GraphicsWindowCreateParameters::Wintype_Desktop) {
      style_flags |= SDL_WINDOW_RESIZABLE;
    }
    else if (params._type == GraphicsWindowCreateParameters::Wintype_Utility) {
    }
    else if (params._type == GraphicsWindowCreateParameters::Wintype_Noborder) {
      style_flags |= SDL_WINDOW_BORDERLESS;
    }

    int x = params._x;
    int y = params._y;
    int w = params._width;
    int h = params._height;

    int flags = 0;
#ifdef BR2_OS_IPHONE
    int flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_SHOWN |
                SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_OPENGL;
    title = "";
#elif defined(BR2_OS_WINDOWS) || defined(BR2_OS_LINUX)
    flags |= style_flags;
    flags |= render_system;
    title = params._title;
#else
    OS_NOT_SUPPORTED_ERROR
#endif

    // Note: This calls SDL_GL_LOADLIBRARY if SDL_WINDOW_OPENGL is set.
    ret = SDL_CreateWindow(title.c_str(), x, y, w, h, flags);
    if (ret != nullptr) {
      // On Linux SDL will set an error if unable to represent a GL/Vulkan
      // profile, as we try different ones. Ignore them for now. Windows SDL
      // sets an error when the context is created.
      SDLUtils::checkSDLErr();

      // Fullscreen nonsense
      if (bFullscreen) {
        SDL_SetWindowFullscreen(ret, SDL_WINDOW_FULLSCREEN);
      }
      if (show) {
        SDL_ShowWindow(ret);
      }

      SDLUtils::checkSDLErr();
    }
    else {
      // Linux: Couldn't find matching GLX visual.
      SDLUtils::checkSDLErr(true, false);
    }

    // Customize window (per display system)
    // setWindowProps(ret, params);

    checkErrors();

    return ret;
  }
#pragma endregion

#pragma region Vulkan Initialization
  void init() {
    // Make the window.s
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == -1) {
      errorExit(Stz "SDL could not be initialized: " + SDL_GetError());
    }

    string_t title = "hello vuklkan";
    GraphicsWindowCreateParameters params(
      title, 100, 100, 500, 500,
      GraphicsWindowCreateParameters::Wintype_Desktop, false, true, false,
      nullptr);
    _pSDLWindow = makeSDLWindow(params, SDL_WINDOW_VULKAN, false);

    printVideoDiagnostics();

    createVulkanInstance(title, _pSDLWindow);
    loadExtensions();
    setupDebug();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandPool();
    createCommandBuffers();
    createSemaphores();

    BRLogInfo("Showing window..");
    SDL_ShowWindow(_pSDLWindow);
  }
  void recreateSwapChain() {
    vkDeviceWaitIdle(_device);

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandBuffers();

    _bSwapChainOutOfDate = false;
  }
  void printGpuSpecs(const VkPhysicalDeviceFeatures* const features) const {
    // features.
  }

  void loadExtensions() {
    // Quick macro.
#define VkLoadExt(_i, _v)                          \
  do {                                             \
    _v = (PFN_##_v)vkGetInstanceProcAddr(_i, #_v); \
    if (_v == nullptr) {                           \
      BRLogError("Could not find " + #_v);         \
    }                                              \
  } while (0)

    // Load Extensions
    VkLoadExt(_instance, vkCreateDebugUtilsMessengerEXT);
    VkLoadExt(_instance, vkDestroyDebugUtilsMessengerEXT);
  }
  VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;

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
    CheckVKR(vkCreateDebugUtilsMessengerEXT, "", _instance, &debugCreateInfo, nullptr, &debugMessenger);
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
  std::unordered_map<std::string, VkLayerProperties> supported_validation_layers;
  bool isValidationLayerSupported(const string_t& name) {
    if (supported_validation_layers.size() == 0) {
      std::vector<string_t> names;
      uint32_t layerCount;

      CheckVKR(vkEnumerateInstanceLayerProperties, "", &layerCount, nullptr);
      std::vector<VkLayerProperties> availableLayers(layerCount);
      CheckVKR(vkEnumerateInstanceLayerProperties, "", &layerCount, availableLayers.data());

      for (auto layer : availableLayers) {
        supported_validation_layers.insert(std::make_pair(layer.layerName, layer));
      }
    }

    if (supported_validation_layers.find(name) != supported_validation_layers.end()) {
      return true;
    }

    return false;
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

  void createVulkanInstance(string_t title, SDL_Window* win) {
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

    CheckVKR(vkCreateInstance, "Failed to create vulkan instance.", &createinfo, nullptr, &_instance);

    if (!SDL_Vulkan_CreateSurface(win, _instance, &_main_window_surface)) {
      checkErrors();
      errorExit("SDL failed to create vulkan window.");
    }

    debugPrintSupportedExtensions();

    // You can log every vulkan call to stdout.
  }
  void debugPrintSupportedExtensions() {
    // Get extension properties.
    uint32_t extensionCount = 0;
    CheckVKR(vkEnumerateInstanceExtensionProperties, "", nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    CheckVKR(vkEnumerateInstanceExtensionProperties, "", nullptr, &extensionCount, extensions.data());
    string_t st = "Supported Vulkan Extensions:" + Os::newline() +
                  "Version   Extension" + Os::newline();
    for (auto ext : extensions) {
      st += Stz "  [" + std::to_string(ext.specVersion) + "] " + ext.extensionName + Os::newline();
    }
    BRLogInfo(st);
  }
  void printVideoDiagnostics() {
    // Init Video
    // SDL_Init(SDL_INIT_VIDEO);

    // Drivers (useless in sdl2)
    const char* driver = SDL_GetCurrentVideoDriver();
    if (driver) {
      BRLogInfo("Default Video Driver: " + driver);
    }
    BRLogInfo("Installed Video Drivers: ");
    int idrivers = SDL_GetNumVideoDrivers();
    for (int idriver = 0; idriver < idrivers; ++idriver) {
      driver = SDL_GetVideoDriver(idriver);
      BRLogInfo(" " + driver);
    }

    // Get current display mode of all displays.
    int nDisplays = SDL_GetNumVideoDisplays();
    BRLogInfo(nDisplays + " Displays:");
    for (int idisplay = 0; idisplay < nDisplays; ++idisplay) {
      SDL_DisplayMode current;
      int should_be_zero = SDL_GetCurrentDisplayMode(idisplay, &current);

      if (should_be_zero != 0) {
        // In case of error...
        BRLogInfo("  Could not get display mode for video display #%d: %s" +
                  idisplay);
        SDLUtils::checkSDLErr();
      }
      else {
        // On success, print the current display mode.
        BRLogInfo("  Display " + idisplay + ": " + current.w + "x" + current.h +
                  ", " + current.refresh_rate + "hz");
        SDLUtils::checkSDLErr();
      }
    }
    checkErrors();
  }

  static VKAPI_ATTR VkBool32 VKAPI_CALL
  debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT messageType,
                const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
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
  void pickPhysicalDevice() {
    BRLogInfo("  Finding Physical Device.");

    //** TODO: some kind of operatino that lets us choose the best device.
    // Or let the user choose the device, right?

    uint32_t deviceCount = 0;
    CheckVKR(vkEnumeratePhysicalDevices, "", _instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
      errorExit("No Vulkan enabled GPUs available.");
    }
    BRLogInfo("Found " + deviceCount + " rendering device(s).");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    CheckVKR(vkEnumeratePhysicalDevices, "", _instance, &deviceCount, devices.data());

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
        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && deviceFeatures.geometryShader && deviceFeatures.fillModeNonSolid) {
          _physicalDevice = device;
        }
      }

      i++;
    }
    BRLogInfo(devInfo);

    if (_physicalDevice == VK_NULL_HANDLE) {
      errorExit("Failed to find a suitable GPU.");
    }
  }

  //This should be on the logical device class.
  std::unordered_map<string_t, VkExtensionProperties> _deviceExtensions;
  std::unordered_map<string_t, VkExtensionProperties>& getDeviceExtensions() {
    if (_deviceExtensions.size() == 0) {
      // Extensions
      std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
      uint32_t extensionCount;
      CheckVKR(vkEnumerateDeviceExtensionProperties, "", _physicalDevice, nullptr, &extensionCount, nullptr);
      std::vector<VkExtensionProperties> availableExtensions(extensionCount);
      CheckVKR(vkEnumerateDeviceExtensionProperties, "", _physicalDevice, nullptr, &extensionCount, availableExtensions.data());
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
  //Here we should really do a "best fit" like we do for OpenGL contexts.
  void createLogicalDevice() {
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

    CheckVKR(vkCreateDevice, "Failed to create logical device.", _physicalDevice, &createInfo, nullptr, &_device);

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
      CheckVKR(vkGetPhysicalDeviceSurfaceSupportKHR, "", _physicalDevice, i, _main_window_surface, &presentSupport);

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

  void createSwapChain() {
    BRLogInfo("Creating Swapchain.");

    //VkPresentModeKHR;
    //VkSurfaceFormatKHR;
    uint32_t formatCount;
    CheckVKR(vkGetPhysicalDeviceSurfaceFormatsKHR, "", _physicalDevice, _main_window_surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount != 0) {
      CheckVKR(vkGetPhysicalDeviceSurfaceFormatsKHR, "", _physicalDevice, _main_window_surface, &formatCount, formats.data());
    }
    string_t fmts = "Surface formats: " + Os::newline();
    for (int i = 0; i < formats.size(); ++i) {
      fmts += " Format " + i;
      fmts += "  Color space: " + VulkanDebug::VkColorSpaceKHR_toString(formats[i].colorSpace);
      fmts += "  Format: " + VulkanDebug::VkFormat_toString(formats[i].format);
    }

    // How the surfaces are presented from the swapchain.
    uint32_t presentModeCount;
    CheckVKR(vkGetPhysicalDeviceSurfacePresentModesKHR, "", _physicalDevice, _main_window_surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    if (presentModeCount != 0) {
      CheckVKR(vkGetPhysicalDeviceSurfacePresentModesKHR, "", _physicalDevice, _main_window_surface, &presentModeCount, presentModes.data());
    }
    //This is cool. Directly query the color space
    VkSurfaceFormatKHR surfaceFormat;
    for (const auto& availableFormat : formats) {
      if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        surfaceFormat = availableFormat;
        break;
      }
    }
    //VK_PRESENT_MODE_FIFO_KHR mode is guaranteed to be available
    // = VK_PRESENT_MODE_FIFO_KHR = vsync
    VkPresentModeKHR presentMode;
    for (const auto& availablePresentMode : presentModes) {
      if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
        presentMode = availablePresentMode;
        break;
      }
    }

    VkSurfaceCapabilitiesKHR caps;
    CheckVKR(vkGetPhysicalDeviceSurfaceCapabilitiesKHR, "", _physicalDevice, _main_window_surface, &caps);

    //Image count, double buffer = 2
    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
      imageCount = caps.maxImageCount;
    }
    if (imageCount > 2) {
      BRLogDebug("Supported Swapchain Image count > 2 : " + imageCount);
    }

    auto m = std::numeric_limits<uint32_t>::max();

    int win_w = 0, win_h = 0;
    SDL_GetWindowSize(_pSDLWindow, &win_w, &win_h);
    _swapChainExtent.width = win_w;
    _swapChainExtent.height = win_h;

    //Extent = Image size
    //Not sure what this as for.
    // if (caps.currentExtent.width != m) {
    //   _swapChainExtent = caps.currentExtent;
    // }
    // else {
    //   VkExtent2D actualExtent = { 0, 0 };
    //   actualExtent.width = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, actualExtent.width));
    //   actualExtent.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, actualExtent.height));
    //   _swapChainExtent = actualExtent;
    // }

    //Create swapchain
    VkSwapchainCreateInfoKHR swapChainCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = _main_window_surface,
      .minImageCount = imageCount,
      .imageFormat = surfaceFormat.format,
      .imageColorSpace = surfaceFormat.colorSpace,
      .imageExtent = _swapChainExtent,
      .imageArrayLayers = 1,  //more than 1 for stereoscopic application
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .preTransform = caps.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = presentMode,
      .clipped = VK_TRUE,
      .oldSwapchain = VK_NULL_HANDLE,  // ** For window resizing.
    };

    CheckVKR(vkCreateSwapchainKHR, "", _device, &swapChainCreateInfo, nullptr, &_swapChain);
    CheckVKR(vkGetSwapchainImagesKHR, "", _device, _swapChain, &imageCount, nullptr);

    _swapChainImages.resize(imageCount);
    CheckVKR(vkGetSwapchainImagesKHR, "", _device, _swapChain, &imageCount, _swapChainImages.data());

    _swapChainImageFormat = surfaceFormat.format;
  }
  void createImageViews() {
    BRLogInfo("Creating Image Views.");
    for (size_t i = 0; i < _swapChainImages.size(); i++) {
      VkImageViewCreateInfo createInfo = {};
      createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      createInfo.image = _swapChainImages[i];
      createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      createInfo.format = _swapChainImageFormat;
      createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      //Image view is a contiguous range of image sub-resources.
      //Subresources are. . ?
      createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      createInfo.subresourceRange.baseMipLevel = 0;
      createInfo.subresourceRange.levelCount = 1;
      createInfo.subresourceRange.baseArrayLayer = 0;
      createInfo.subresourceRange.layerCount = 1;
      _swapChainImageViews.push_back(VkImageView{});

      CheckVKR(vkCreateImageView, "", _device, &createInfo, nullptr, &_swapChainImageViews[i]);
    }
  }

  std::vector<char> readFile(const std::string& file) {
    string_t file_loc = App::combinePath(App::_appRoot, file);
    std::cout << "Loading file " << file_loc << std::endl;

    //chdir("~/git/GWindow_Sandbox/");
    //#endif
    //::ate avoid seeking to the end. Neat trick.
    std::ifstream fs(file_loc, std::ios::in | std::ios::binary | std::ios::ate);
    if (!fs.good() || !fs.is_open()) {
      errorExit(Stz "Could not open shader file '" + file_loc + "'");
      return std::vector<char>{};
    }

    auto size = fs.tellg();
    fs.seekg(0, std::ios::beg);
    std::vector<char> ret(size);
    //ret.reserve(size);
    fs.read(ret.data(), size);

    fs.close();
    return ret;
  }

  std::vector<VkPipelineShaderStageCreateInfo> createShaderForPipeline() {
#ifdef BR2_OS_LINUX
    std::vector<char> vertShaderCode = readFile(Stz "./../test_vs.spv");
    std::vector<char> fragShaderCode = readFile(Stz "./../test_fs.spv");
#else
    std::vector<char> vertShaderCode = readFile(Stz "./../../test_vs.spv");
    std::vector<char> fragShaderCode = readFile(Stz "./../../test_fs.spv");
#endif

    _vertShaderModule = createShaderModule(vertShaderCode);
    _fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = _vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = _fragShaderModule;
    fragShaderStageInfo.pName = "main";

    std::vector<VkPipelineShaderStageCreateInfo> inf{ vertShaderStageInfo, fragShaderStageInfo };
    return inf;
  }
  void createGraphicsPipeline() {
    BRLogInfo("Creating Graphics Pipeline.");
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages = createShaderForPipeline();

    //This is basically a glsl attribute specifying a layout identifier
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)_swapChainExtent.width;
    viewport.height = (float)_swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = _swapChainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;  //Disable
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    //*Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    //VkPipelineDepthStencilStateCreateInfo // depth / stencil - we'll use null here.

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .attachmentCount = 1,
      .pAttachments = &colorBlendAttachment,
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 0,            // Optional
      .pSetLayouts = nullptr,         // Optional
      .pushConstantRangeCount = 0,    // Optional
      .pPushConstantRanges = nullptr  // Optional
    };

    CheckVKR(vkCreatePipelineLayout, "", _device, &pipelineLayoutInfo, nullptr, &_pipelineLayout);

    //Create render pass for pipeline
    createRenderPass();

    //Pipeline dynamic state. - Change states in the pipeline without rebuilding the pipeline.
    VkDynamicState dynamicStates[] = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_LINE_WIDTH
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;  // vertex shader, frag shader.
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr;  // Optional
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = nullptr;  //&dynamicState;  // Optional
    pipelineInfo.layout = _pipelineLayout;
    pipelineInfo.renderPass = _renderPass;
    pipelineInfo.subpass = 0;                          //Index of subpass - we have 1 subpass
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;  // Derive this pipeline from another one. This is pretty neat.
    //pipelineInfo.basePipelineIndex = -1;               // Optional

    // BRLogInfo(VulkanDebug::get_VkGraphicsPipelineCreateInfo());

    CheckVKR(vkCreateGraphicsPipelines, "", _device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_graphicsPipeline);
  }
  void createRenderPass() {
    BRLogInfo("Creating Render Pass.");

    //This is essentially a set of data that we supply to VkCmdRenderPass

    //VkCmdRenderPass -> RenderPassInfo.colorAttachment -> framebuffer index.

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = _swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;  // Number of samples.
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    //Framebuffer attachments.
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    //Subpass.
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    CheckVKR(vkCreateRenderPass, "", _device, &renderPassInfo, nullptr, &_renderPass);
  }
  VkShaderModule createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;

    BRLogInfo("Creating shader : " + code.size() + " bytes.");
    CheckVKR(vkCreateShaderModule, "", _device, &createInfo, nullptr, &shaderModule);

    return shaderModule;
  }

  void createFramebuffers() {
    BRLogInfo("Creating Framebuffers.");

    _swapChainFramebuffers.resize(_swapChainImageViews.size());

    for (size_t i = 0; i < _swapChainImageViews.size(); i++) {
      VkImageView attachments[] = { _swapChainImageViews[i] };

      VkFramebufferCreateInfo framebufferInfo{};
      framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebufferInfo.renderPass = _renderPass;
      framebufferInfo.attachmentCount = 1;
      framebufferInfo.pAttachments = attachments;
      framebufferInfo.width = _swapChainExtent.width;
      framebufferInfo.height = _swapChainExtent.height;
      framebufferInfo.layers = 1;
      framebufferInfo.flags = 0;  //VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT * Imageless framebuffer

      CheckVKR(vkCreateFramebuffer, "", _device, &framebufferInfo, nullptr, &_swapChainFramebuffers[i]);
    }
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
    CheckVKR(vkCreateCommandPool, "", _device, &poolInfo, nullptr, &_commandPool);
  }

  void createCommandBuffers() {
    BRLogInfo("Creating Command Buffers.");
    //One framebuffer per swapchain image. {double buffered means just 2 I think}
    //One command buffer per framebuffer.
    //Command buffers have various states like "pending" which determines what primary/secondary and you can do with them

    _commandBuffers.resize(_swapChainFramebuffers.size());
    VkCommandBufferAllocateInfo allocInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = _commandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = (uint32_t)_commandBuffers.size(),
    };
    CheckVKR(vkAllocateCommandBuffers, "", _device, &allocInfo, _commandBuffers.data());

    for (size_t i = 0; i < _commandBuffers.size(); ++i) {
      VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = 0,                   //VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, //VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT
        .pInheritanceInfo = nullptr,  //ignored for primary buffers.
      };
      //This is a union so only color/depthstencil is supplied
      VkClearValue clearColor = {
        .color = VkClearColorValue{ 0.0, 0.0, 0.0, 1.0 }
      };
      VkRenderPassBeginInfo rpbi = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = _renderPass,  //The renderpass we created above.
        .framebuffer = _swapChainFramebuffers[i],
        .renderArea = VkRect2D{
          .offset = VkOffset2D{ .x = 0, .y = 0 },
          .extent = _swapChainExtent },
        .clearValueCount = 1,
        .pClearValues = &clearColor,
      };
      CheckVKR(vkBeginCommandBuffer, "", _commandBuffers[i], &beginInfo);

      vkCmdBeginRenderPass(_commandBuffers[i], &rpbi, VK_SUBPASS_CONTENTS_INLINE /*VkSubpassContents*/);
      vkCmdBindPipeline(_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);
      vkCmdDraw(_commandBuffers[i], 9, 3, 0, 0);
      vkCmdEndRenderPass(_commandBuffers[i]);
      CheckVKR(vkEndCommandBuffer, "", _commandBuffers[i]);
    }
  }

  void createSemaphores() {
    BRLogInfo("Creating Rendering Semaphores.");
    _imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    _renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    _inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    _imagesInFlight.resize(_swapChainImages.size(), VK_NULL_HANDLE);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
      };
      CheckVKR(vkCreateSemaphore, "", _device, &semaphoreInfo, nullptr, &_imageAvailableSemaphores[i]);
      CheckVKR(vkCreateSemaphore, "", _device, &semaphoreInfo, nullptr, &_renderFinishedSemaphores[i]);

      VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,  //Fences must always be created in a signaled state.
      };

      CheckVKR(vkCreateFence, "", _device, &fenceInfo, nullptr, &_inFlightFences[i]);
    }
  }
#pragma endregion

#pragma region Vulkan Rendering

  void drawFrame() {
    vkWaitForFences(_device, 1, &_inFlightFences[_currentFrame], VK_TRUE, UINT64_MAX);
    //one command buffer per framebuffer,
    //acquire the image form teh swapchain (which is our framebuffer)
    //signal the semaphore.
    uint32_t imageIndex = 0;
    //Returns an image in the swapchain that we can draw to.
    VkResult res;
    res = vkAcquireNextImageKHR(_device, _swapChain, UINT64_MAX, _imageAvailableSemaphores[_currentFrame], VK_NULL_HANDLE, &imageIndex);
    if (res != VK_SUCCESS) {
      if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        _bSwapChainOutOfDate = true;
        return;
      }
      else if (res == VK_SUBOPTIMAL_KHR) {
        _bSwapChainOutOfDate = true;
        return;
      }
      else {
        validateVkResult(res, "", "vkAcquireNextImageKHR");
      }
    }

    //There is currently a frame that is using this image. So wait for this image.
    if (_imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
      vkWaitForFences(_device, 1, &_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    _imagesInFlight[imageIndex] = _inFlightFences[_currentFrame];

    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    //aquire next image
    VkSubmitInfo submitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,  //VkStructureType
      .pNext = nullptr,                        //const void*
      //Note: Eadch entry in waitStages corresponds to the semaphore in pWaitSemaphores - we can wait for multiple stages
      //to finish rendering, or just wait for the framebuffer output.
      .waitSemaphoreCount = 1,                                       //uint32_t
      .pWaitSemaphores = &_imageAvailableSemaphores[_currentFrame],  //const VkSemaphore*
      .pWaitDstStageMask = waitStages,                               //const VkPipelineStageFlags*
      .commandBufferCount = 1,                                       //uint32_t
      .pCommandBuffers = &_commandBuffers[imageIndex],               //const VkCommandBuffer*
      //The semaphore is signaled when the queue has completed the requested wait stages.
      .signalSemaphoreCount = 1,                                       //uint32_t
      .pSignalSemaphores = &_renderFinishedSemaphores[_currentFrame],  //const VkSemaphore*
    };

    vkResetFences(_device, 1, &_inFlightFences[_currentFrame]);

    vkQueueSubmit(_graphicsQueue, 1, &submitInfo, _inFlightFences[_currentFrame]);

    VkPresentInfoKHR presentinfo = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,                   //VkStructureType
      .pNext = nullptr,                                              //const void*
      .waitSemaphoreCount = 1,                                       //uint32_t
      .pWaitSemaphores = &_renderFinishedSemaphores[_currentFrame],  //const VkSemaphore*
      .swapchainCount = 1,                                           //uint32_t
      .pSwapchains = &_swapChain,                                    //const VkSwapchainKHR*
      .pImageIndices = &imageIndex,                                  //const uint32_t*
      .pResults = nullptr                                            //VkResult*   //multiple swapchains
    };
    res = vkQueuePresentKHR(_presentQueue, &presentinfo);
    if (res != VK_SUCCESS) {
      if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        _bSwapChainOutOfDate = true;
        return;
      }
      else if (res == VK_SUBOPTIMAL_KHR) {
        _bSwapChainOutOfDate = true;
        return;
      }
      else {
        validateVkResult(res, "", "vkAcquireNextImageKHR");
      }
    }

    //**CONTINUE TUTORIAL AFTER THIS POINT
    //https://vulkan-tutorial.com/en/Drawing_a_triangle/Drawing/Rendering_and_presentation
    // We add additional threads for async the rendering.
    //vkQueueWaitIdle(_presentQueue);  //Waits for operations to complete to prevent overfilling the command buffers .
  }
#pragma endregion

  void cleanupSwapChain() {
    for (auto& v : _swapChainFramebuffers) {
      vkDestroyFramebuffer(_device, v, nullptr);
    }
    _swapChainFramebuffers.clear();

    vkFreeCommandBuffers(_device, _commandPool, static_cast<uint32_t>(_commandBuffers.size()), _commandBuffers.data());
    _commandBuffers.clear();

    vkDestroyPipeline(_device, _graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
    vkDestroyRenderPass(_device, _renderPass, nullptr);

    for (size_t i = 0; i < _swapChainImageViews.size(); i++) {
      vkDestroyImageView(_device, _swapChainImageViews[i], nullptr);
    }
    _swapChainImageViews.clear();

    vkDestroySwapchainKHR(_device, _swapChain, nullptr);
  }
  void cleanup() {
    // All child objects created using instance must have been destroyed prior
    // to destroying instance

    cleanupSwapChain();

    for (int iFrame = 0; iFrame < MAX_FRAMES_IN_FLIGHT; iFrame++) {
      vkDestroySemaphore(_device, _renderFinishedSemaphores[iFrame], nullptr);
      vkDestroySemaphore(_device, _imageAvailableSemaphores[iFrame], nullptr);
      vkDestroyFence(_device, _inFlightFences[iFrame], nullptr);
    }
    _renderFinishedSemaphores.clear();
    _imageAvailableSemaphores.clear();
    _inFlightFences.clear();

    vkDestroyCommandPool(_device, _commandPool, nullptr);

    vkDestroyShaderModule(_device, _vertShaderModule, nullptr);
    vkDestroyShaderModule(_device, _fragShaderModule, nullptr);
    vkDestroyDevice(_device, nullptr);
    if (_bEnableValidationLayers) {
      vkDestroyDebugUtilsMessengerEXT(_instance, debugMessenger, nullptr);
    }
    vkDestroyInstance(_instance, nullptr);
    SDL_DestroyWindow(_pSDLWindow);
  }
};  // namespace VG

//////////////////////

SDLVulkan::SDLVulkan() {
  _pInt = std::make_unique<SDLVulkan_Internal>();
}
SDLVulkan::~SDLVulkan() {
  _pInt->cleanup();
  _pInt = nullptr;
}
void SDLVulkan::init() {
  try {
    _pInt->init();
  }
  catch (...) {
    _pInt->cleanup();
  }
}
void SDLVulkan::renderLoop() {
  bool exit = false;
  while (!exit) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        exit = true;
        break;
      }
      else if (event.type == SDL_WINDOWEVENT) {
        if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
          _pInt->recreateSwapChain();
          break;
        }
      }
      else if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
          exit = true;
          break;
        }
      }
    }
    _pInt->drawFrame();
  }
  //This stops all threads before we cleanup.
  vkDeviceWaitIdle(_pInt->_device);
  _pInt->cleanup();
}

}  // namespace VG
