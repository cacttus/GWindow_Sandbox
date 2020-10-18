#include "./SDLVulkan.h"
#include "./VulkanHeader.h"
#include "./Vulkan.h"
#include "./VulkanDebug.h"
#include "./VulkanClasses.h"

namespace VG {

static MipmapMode g_mipmap_mode = MipmapMode::Linear;  //**TESTING**
static bool g_samplerate_shading = true;

//Data from Vulkan-Tutorial
class SDLVulkan_Internal {
public:
#pragma region Members
  SDL_Window* _pSDLWindow = nullptr;
  std::shared_ptr<Vulkan> _vulkan = nullptr;

  std::shared_ptr<Vulkan> vulkan() { return _vulkan; }

  std::shared_ptr<VulkanTextureImage> _testTexture = nullptr;
  std::shared_ptr<VulkanDepthImage> _depthTexture = nullptr;  //This is not implemented
  std::shared_ptr<VulkanBuffer> _vertexBuffer;
  std::shared_ptr<VulkanBuffer> _indexBuffer;

  //**TODO:Pipeline
  //**TODO:Pipeline
  VkRenderPass _renderPass = VK_NULL_HANDLE;
  VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;
  VkPipeline _graphicsPipeline = VK_NULL_HANDLE;
  //VkShaderModule _vertShaderModule = VK_NULL_HANDLE;
  //VkShaderModule _fragShaderModule = VK_NULL_HANDLE;
  std::shared_ptr<VulkanPipelineShader> _pShader = nullptr;

  //**TODO:Pipeline
  //**TODO:Pipeline

  //**TODO:Swapchain
  //**TODO:Swapchain
  //Asynchronous computing depends on the swapchain count.
  //_iConcurrentFrames should be set to the swapchain count as vkAcquireNExtImageKHR gets the next usable image.
  //You can't have more than #swapchain images rendering at a time.
  int32_t _iConcurrentFrames = 3;
  size_t _currentFrame = 0;
  std::vector<VkFence> _inFlightFences;
  std::vector<VkFence> _imagesInFlight;
  std::vector<VkSemaphore> _imageAvailableSemaphores;
  std::vector<VkSemaphore> _renderFinishedSemaphores;
  std::vector<std::shared_ptr<VulkanBuffer>> _viewProjUniformBuffers;  //One per swapchain image since theres multiple frames in flight.
  std::vector<std::shared_ptr<VulkanBuffer>> _instanceUniformBuffers;  //One per swapchain image since theres multiple frames in flight.
  int32_t _numInstances = 3;
  VkSwapchainKHR _swapChain = VK_NULL_HANDLE;
  VkExtent2D _swapChainExtent;
  VkFormat _swapChainImageFormat;
  std::vector<VkImage> _swapChainImages;
  std::vector<VkImageView> _swapChainImageViews;
  std::vector<VkFramebuffer> _swapChainFramebuffers;
  std::vector<VkCommandBuffer> _commandBuffers;

  VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;
  std::vector<VkDescriptorSetLayout> _layouts;
  std::vector<VkDescriptorSet> _descriptorSets;

  bool _bSwapChainOutOfDate = false;
  //**TODO:Swapchain
  //**TODO:Swapchain

#pragma endregion

#pragma region SDL
  SDL_Window* makeSDLWindow(const GraphicsWindowCreateParameters& params,
                            int render_system, bool show) {
    string_t title;
    bool bFullscreen = false;
    SDL_Window* ret = nullptr;

    int style_flags = SDL_WINDOW_ALLOW_HIGHDPI;
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

    return ret;
  }
#pragma endregion

#pragma region Vulkan Initialization

  string_t base_title = "Press F1 to toggle Mipmaps";

  void init() {
    // Make the window.
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == -1) {
      vulkan()->errorExit(Stz "SDL could not be initialized: " + SDL_GetError());
    }

    string_t title = base_title;
    GraphicsWindowCreateParameters params(
      title, 100, 100, 500, 500,
      GraphicsWindowCreateParameters::Wintype_Desktop, false, true, false,
      nullptr);
    _pSDLWindow = makeSDLWindow(params, SDL_WINDOW_VULKAN, false);

    sdl_PrintVideoDiagnostics();

    _vulkan = std::make_shared<Vulkan>(title, _pSDLWindow);

    //TODO: we'll move this stuff out later.
    createVertexBuffer();  //Mesh class
    //Make Shader.
    _pShader = std::make_shared<VulkanPipelineShader>(_vulkan,
                                                      "Vulkan-Tutorial-Test-Shader",
                                                      std::vector{
                                                        App::rootFile("test_vs.spv"),
                                                        App::rootFile("test_fs.spv") });

    recreateSwapChain();

    BRLogInfo("Showing window..");
    SDL_ShowWindow(_pSDLWindow);
  }
  void sdl_PrintVideoDiagnostics() {
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
  }
  void recreateSwapChain() {
    vkDeviceWaitIdle(vulkan()->device());

    cleanupSwapChain();
    vkDeviceWaitIdle(vulkan()->device());

    createSwapChain();       // * stays - recreate
    createUniformBuffers();  // - create once to not be recreated when we genericize swapchain
    createTextureImage();    // - create once - single creation

    //* Dynamic shaders
    createDescriptorPool();       // - create once - single creation
    createDescriptorSetLayout();  // - create once - single creation
    createDescriptorSets();       // - create once - single creation (vkCmdBindDescriptorSets)
                                  //references texture &c

    createSwapchainImageViews();  // * stays - recreate
    createGraphicsPipeline();
    // --> createShaderForPipeline - Move out - create once
    createFramebuffers();    // *stays - recreate
    createCommandBuffers();  // * stays
    createSyncObjects();     // **Possibly create once

    _bSwapChainOutOfDate = false;
  }
  VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;
  void createDescriptorPool() {
    //One pool per shader with all descriptors needed in the shader.
    //Uniform buffer descriptor pool.
    VkDescriptorPoolSize uboPoolSize = {
      .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,                          //VkDescriptorType
      .descriptorCount = static_cast<uint32_t>(_swapChainImages.size()),  //uint32_t
    };
    VkDescriptorPoolSize samplerPoolSize = {
      .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,                  //VkDescriptorType
      .descriptorCount = static_cast<uint32_t>(_swapChainImages.size()),  //uint32_t
    };
    std::array<VkDescriptorPoolSize, 2> poolSizes{ uboPoolSize, samplerPoolSize };
    VkDescriptorPoolCreateInfo poolInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,     // VkStructureType
      .pNext = nullptr,                                           // const void*
      .flags = 0,                                                 // VkDescriptorPoolCreateFlags
      .maxSets = static_cast<uint32_t>(_swapChainImages.size()),  // uint32_t
      .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),   // uint32_t
      .pPoolSizes = poolSizes.data(),                             // const VkDescriptorPoolSize*
    };
    CheckVKR(vkCreateDescriptorPool, vulkan()->device(), &poolInfo, nullptr, &_descriptorPool);
  }
  void createDescriptorSetLayout() {
    //One per swapchain image. Copied to for set.
    //VkDescriptorPoolCreateInfo::maxSets
    VkDescriptorSetLayoutBinding uboLayoutBinding = {
      .binding = 0,                                         //uint32_t
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  //VkDescriptorType      Can put SSBO here.
      .descriptorCount = 1,                                 //uint32_t
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,             //VkShaderStageFlags
      .pImmutableSamplers = nullptr,                        //const VkSampler*    for VK_DESCRIPTOR_TYPE_SAMPLER or VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    };
    VkDescriptorSetLayoutBinding uboInstanceLayoutBinding = {
      .binding = 1,                                         //uint32_t
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  //VkDescriptorType      Can put SSBO here.
      .descriptorCount = _numInstances,                                 //uint32_t //descriptorCount specifies the number of values in the array
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,             //VkShaderStageFlags
      .pImmutableSamplers = nullptr,                        //const VkSampler*    for VK_DESCRIPTOR_TYPE_SAMPLER or VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    };
    VkDescriptorSetLayoutBinding samplerLayoutBinding = {
      .binding = 2,                                                 // uint32_t
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  // VkDescriptorType
      .descriptorCount = 1,                                         // uint32_t
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,                   // VkShaderStageFlags
      .pImmutableSamplers = nullptr,                                // const VkSampler*
    };

    std::array<VkDescriptorSetLayoutBinding, 3> bindings = { uboLayoutBinding, uboInstanceLayoutBinding, samplerLayoutBinding };

    //This is duplicated for each UBO bound to the shader program (pipeline) for each swapchain image.
    // This is duplicated in the vkAllocateDescriptorSets area. It essentially should be run for each UBO binding.
    VkDescriptorSetLayoutCreateInfo layoutInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,  //VkStructureType
      .pNext = nullptr,                                              //const void*
      .flags = 0,                                                    //VkDescriptorSetLayoutCreateFlags   Some cool stuff can be put here to modify UBO updates.
      .bindingCount = static_cast<uint32_t>(bindings.size()),        //uint32_t
      .pBindings = bindings.data(),                                  //const VkDescriptorSetLayoutBinding*
    };
    CheckVKR(vkCreateDescriptorSetLayout, vulkan()->device(), &layoutInfo, nullptr, &_descriptorSetLayout);
  }
  void createDescriptorSets() {
    //This is for vkCmdBindDescriptorSets
    _layouts.resize(_swapChainImages.size(), _descriptorSetLayout);

    //We are duplicating the descriptor layout we created in createDescriptorSetLayout. In fact we could just duplicate it there..
    VkDescriptorSetAllocateInfo allocInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,               // VkStructureType
      .pNext = nullptr,                                                      // const void*
      .descriptorPool = _descriptorPool,                                     // VkDescriptorPool
      .descriptorSetCount = static_cast<uint32_t>(_swapChainImages.size()),  // uint32_t
      .pSetLayouts = _layouts.data(),                                        // const VkDescriptorSetLayout*
    };

    //Descriptor sets are automatically freed when the descriptor pool is destroyed.
    _descriptorSets.resize(_swapChainImages.size());
    CheckVKR(vkAllocateDescriptorSets, vulkan()->device(), &allocInfo, _descriptorSets.data());

    for (size_t i = 0; i < _swapChainImages.size(); ++i) {
      //ShaderSampler.
      //UBO descriptor
      VkDescriptorBufferInfo viewProjBufferInfo = {
        .buffer = _viewProjUniformBuffers[i]->hostBuffer()->buffer(),  // VkBuffer
        .offset = 0,                                                   // VkDeviceSize
        .range = sizeof(UniformBufferObject),                          // VkDeviceSize OR VK_WHOLE_SIZE
      };
      VkWriteDescriptorSet viewProjUboDescriptorWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,      //VkStructureType
        .pNext = nullptr,                                     //const void*
        .dstSet = _descriptorSets[i],                         //VkDescriptorSet
        .dstBinding = 0,                                      //uint32_t
        .dstArrayElement = 0,                                 //uint32_t
        .descriptorCount = 1,                                 //uint32_t
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  //VkDescriptorType
        .pImageInfo = nullptr,                                //const VkDescriptorImageInfo*
        .pBufferInfo = &viewProjBufferInfo,                   //const VkDescriptorBufferInfo*
        .pTexelBufferView = nullptr,                          //const VkBufferView*
      };

      //UBO descriptor
      VkDescriptorBufferInfo instanceBufferInfo = {
        .buffer = _instanceUniformBuffers[i]->hostBuffer()->buffer(),  // VkBuffer
        .offset = 0,                                                   // VkDeviceSize
        .range = sizeof(InstanceUBOData) * _numInstances,              // VkDeviceSize OR VK_WHOLE_SIZE
      };
      VkWriteDescriptorSet instanceUBODescriptorWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,      //VkStructureType
        .pNext = nullptr,                                     //const void*
        .dstSet = _descriptorSets[i],                         //VkDescriptorSet
        .dstBinding = 1,                                      //uint32_t
        .dstArrayElement = 0,                                 //uint32_t
        .descriptorCount = 1,                                 //uint32_t
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  //VkDescriptorType
        .pImageInfo = nullptr,                                //const VkDescriptorImageInfo*
        .pBufferInfo = &instanceBufferInfo,                   //const VkDescriptorBufferInfo*
        .pTexelBufferView = nullptr,                          //const VkBufferView*
      };

      //Sampler descriptor
      VkDescriptorImageInfo imageInfo = {
        .sampler = _testTexture->sampler(),                       //VkSampler
        .imageView = _testTexture->imageView(),                   //VkImageView
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,  //VkImageLayout
      };
      VkWriteDescriptorSet samplerDescriptorWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,              //VkStructureType
        .pNext = nullptr,                                             //const void*
        .dstSet = _descriptorSets[i],                                 //VkDescriptorSet
        .dstBinding = 2,                                              //uint32_t
        .dstArrayElement = 0,                                         //uint32_t
        .descriptorCount = 1,                                         //uint32_t
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  //VkDescriptorType
        .pImageInfo = &imageInfo,                                     //const VkDescriptorImageInfo*
        .pBufferInfo = nullptr,                                       //const VkDescriptorBufferInfo*
        .pTexelBufferView = nullptr,                                  //const VkBufferView*
      };

      std::array<VkWriteDescriptorSet, 3> descriptorWrites{ viewProjUboDescriptorWrite, instanceUBODescriptorWrite, samplerDescriptorWrite };
      vkUpdateDescriptorSets(vulkan()->device(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
  }
  void createUniformBuffers() {
    for (auto& img : _swapChainImages) {
      _viewProjUniformBuffers.push_back(std::make_shared<VulkanBuffer>(
        vulkan(),
        VulkanBufferType::UniformBuffer,
        false,  //not on GPU
        sizeof(UniformBufferObject), nullptr, 0));
      _instanceUniformBuffers.push_back(std::make_shared<VulkanBuffer>(
        vulkan(),
        VulkanBufferType::UniformBuffer,
        false,  //not on GPU
        sizeof(InstanceUBOData) * _numInstances, nullptr, 0));

    }
  }
  float pingpong_t01(int durationMs = 5000) {
    //returns the [0,1] pingpong time.
    //The duration of each PING and PONG is durationMs / 2
    // 0...1 ... 0 ... 1
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
    auto t01ms = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
    float t01 = 0;
    if (t01ms % durationMs > (durationMs / 2)) {
      t01 = 1.0f - ((float)(t01ms % durationMs - durationMs / 2) / (float)(durationMs / 2));
    }
    else {
      t01 = ((float)(t01ms % durationMs) / (float)(durationMs / 2));
    }
    return t01;
  }
  std::vector<BR2::vec3> offsets;
  void tryInitializeOffsets() {
    #define fr01() (-0.5 + ((double)rand() / (double)RAND_MAX))
    #define rnd(a,b) ((a) + ((b)-(a))*(fr01()))
    #define rr ((float)rnd(-3,3))
    if (offsets.size() == 0) {
      for (size_t i = 0; i < _numInstances; ++i) {
        offsets.push_back({rr,rr,rr});
      }
    }
  }

  void updateUniformBuffer(uint32_t currentImage) {
    //Push constants are faster.
    float t01 = pingpong_t01(10000);
    float t02 = pingpong_t01(10000);

    float d = 20;
    BR2::vec3 campos = { d, d, d };
    BR2::vec3 lookAt = { 0, 0, 0 };
    BR2::vec3 wwf = (lookAt - campos) * 0.1f;
    BR2::vec3 trans = campos + wwf + (lookAt - campos - wwf) * t01;
    UniformBufferObject ub = {
      .view = BR2::mat4::getLookAt(campos, lookAt, BR2::vec3(0.0f, 0.0f, 1.0f)),
      .proj = BR2::mat4::projection(BR2::MathUtils::radians(45.0f), (float)_swapChainExtent.width, -(float)_swapChainExtent.height, 0.1f, 100.0f)
    };
    _viewProjUniformBuffers[currentImage]->writeData((void*)&ub, 0, sizeof(UniformBufferObject));

    tryInitializeOffsets();

    BR2::vec3 origin = { -0.5, -0.5, -0.5 };  //cube origin
    std::vector<BR2::mat4> mats(_numInstances);
    for (uint32_t i = 0; i < _numInstances; ++i) {
      if (i < offsets.size()) {
        mats[i] = BR2::mat4::translation(origin) *
                  BR2::mat4::rotation(BR2::MathUtils::radians(360) * t02, BR2::vec3(0, 0, 1)) * 
                  BR2::mat4::translation(trans + offsets[i]);
      }
    }
    _instanceUniformBuffers[currentImage]->writeData(mats.data(), 0, sizeof(mats[0]) * mats.size());
    // ub.proj._m22 *= -1;
  }
  void createSwapChain() {
    BRLogInfo("Creating Swapchain.");

    uint32_t formatCount;
    CheckVKR(vkGetPhysicalDeviceSurfaceFormatsKHR, vulkan()->physicalDevice(), vulkan()->windowSurface(), &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount != 0) {
      CheckVKR(vkGetPhysicalDeviceSurfaceFormatsKHR, vulkan()->physicalDevice(), vulkan()->windowSurface(), &formatCount, formats.data());
    }
    string_t fmts = "Surface formats: " + Os::newline();
    for (int i = 0; i < formats.size(); ++i) {
      fmts += " Format " + i;
      fmts += "  Color space: " + VulkanDebug::VkColorSpaceKHR_toString(formats[i].colorSpace);
      fmts += "  Format: " + VulkanDebug::VkFormat_toString(formats[i].format);
    }

    // How the surfaces are presented from the swapchain.
    uint32_t presentModeCount;
    CheckVKR(vkGetPhysicalDeviceSurfacePresentModesKHR, vulkan()->physicalDevice(), vulkan()->windowSurface(), &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    if (presentModeCount != 0) {
      CheckVKR(vkGetPhysicalDeviceSurfacePresentModesKHR, vulkan()->physicalDevice(), vulkan()->windowSurface(), &presentModeCount, presentModes.data());
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
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& availablePresentMode : presentModes) {
      if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
        presentMode = availablePresentMode;
        break;
      }
    }
    if (presentMode == VK_PRESENT_MODE_FIFO_KHR) {
      BRLogWarn("Mailbox present mode was not found for presenting swapchain.");
    }

    VkSurfaceCapabilitiesKHR caps;
    CheckVKR(vkGetPhysicalDeviceSurfaceCapabilitiesKHR, vulkan()->physicalDevice(), vulkan()->windowSurface(), &caps);

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
      .surface = vulkan()->windowSurface(),
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

    CheckVKR(vkCreateSwapchainKHR, vulkan()->device(), &swapChainCreateInfo, nullptr, &_swapChain);
    CheckVKR(vkGetSwapchainImagesKHR, vulkan()->device(), _swapChain, &imageCount, nullptr);

    _swapChainImages.resize(imageCount);
    CheckVKR(vkGetSwapchainImagesKHR, vulkan()->device(), _swapChain, &imageCount, _swapChainImages.data());

    _swapChainImageFormat = surfaceFormat.format;
  }
  void createSwapchainImageViews() {
    BRLogInfo("Creating Image Views.");
    for (size_t i = 0; i < _swapChainImages.size(); i++) {
      auto view = vulkan()->createImageView(_swapChainImages[i], _swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
      _swapChainImageViews.push_back(view);
    }
  }
  void createColorResources() {
    //*So here we need to branch off VulkanImage and create the depth buffer format and the color target format.
    //  This will be similar to the render formats in the renderPipe.
    //  This will tie in to the generic RenderPipe attachments.
    // The final attachment for an MSAA down-sampling is called a "resolve" attachment.
    //class RenderTarget
    //_colorTarget = std::make_shared<VulkanImage>();
  }
  void createGraphicsPipeline() {
    //This is essentially what in GL was the shader program.
    BRLogInfo("Creating Graphics Pipeline.");

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,  // VkStructureType
      //These are the buffer descriptors - UBO - SSBO -
      .setLayoutCount = 1,                   // uint32_t
      .pSetLayouts = &_descriptorSetLayout,  // VkDescriptorSetLayout
      //Constants to pass to shaders.
      .pushConstantRangeCount = 0,    // uint32_t
      .pPushConstantRanges = nullptr  // VkPushConstantRange
    };

    CheckVKR(vkCreatePipelineLayout, vulkan()->device(), &pipelineLayoutInfo, nullptr, &_pipelineLayout);

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages = _pShader->getShaderStageCreateInfos();
    //std::vector<VkPipelineShaderStageCreateInfo> shaderStages = createShaderForPipeline();

    auto binding_desc = VertType::getBindingDescription();
    auto attr_desc = VertType::getAttributeDescriptions();

    //This is basically a glsl attribute specifying a layout identifier
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &binding_desc,
      .vertexAttributeDescriptionCount = static_cast<uint32_t>(attr_desc.size()),
      .pVertexAttributeDescriptions = attr_desc.data(),
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,  //VkStructureType
      .pNext = nullptr,                                                      //const void*
      .flags = 0,                                                            //VkPipelineInputAssemblyStateCreateFlags
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,                       //VkPrimitiveTopology
      .primitiveRestartEnable = VK_FALSE,                                    //VkBool32
    };

    VkViewport viewport = {
      .x = 0,
      .y = 0,
      .width = (float)_swapChainExtent.width,
      .height = (float)_swapChainExtent.height,
      .minDepth = 0,
      .maxDepth = 1,
    };

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = _swapChainExtent;

    VkPipelineViewportStateCreateInfo viewportState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,  //VkStructureType
      .pNext = nullptr,                                                //const void*
      .flags = 0,                                                      //VkPipelineViewportStateCreateFlags
      .viewportCount = 1,                                              //uint32_t
      .pViewports = &viewport,                                         //const VkViewport*
      .scissorCount = 1,                                               //uint32_t
      .pScissors = &scissor,                                           //const VkRect2D*
    };

    //Create render pass for pipeline
    createRenderPass();

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
      .blendEnable = VK_TRUE,                                                                                                       //VkBool32
      .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,                                                                             //VkBlendFactor
      .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,                                                                   //VkBlendFactor
      .colorBlendOp = VK_BLEND_OP_ADD,                                                                                              //VkBlendOp
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,                                                                                   //VkBlendFactor
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,                                                                                  //VkBlendFactor
      .alphaBlendOp = VK_BLEND_OP_ADD,                                                                                              //VkBlendOp
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,  //VkColorComponentFlags
    };
    VkPipelineColorBlendStateCreateInfo colorBlending = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .attachmentCount = 1,
      .pAttachments = &colorBlendAttachment,
    };
    VkPipelineRasterizationStateCreateInfo rasterizer = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,  //VkStructureType
      .pNext = nullptr,                                                     //const void*
      .flags = 0,                                                           //VkPipelineRasterizationStateCreateFlags
      .depthClampEnable = VK_FALSE,                                         //VkBool32
      .rasterizerDiscardEnable = VK_FALSE,                                  //VkBool32
      .polygonMode = VK_POLYGON_MODE_FILL,                                  //VkPolygonMode
      .cullMode = VK_CULL_MODE_BACK_BIT,                                    //VkCullModeFlags
      .frontFace = VK_FRONT_FACE_CLOCKWISE,                                 //VkFrontFace
      .depthBiasEnable = VK_FALSE,                                          //VkBool32
      .depthBiasConstantFactor = 0,                                         //float
      .depthBiasClamp = 0,                                                  //float
      .depthBiasSlopeFactor = 0,                                            //float
      .lineWidth = 1,                                                       //float
    };
    //Pipeline dynamic state. - Change states in the pipeline without rebuilding the pipeline.
    std::vector<VkDynamicState> dynamicStates = {
      //Note: if viewport is dynamic then the viewports below are ignored.
      //VK_DYNAMIC_STATE_VIEWPORT,
      //VK_DYNAMIC_STATE_LINE_WIDTH
    };
    VkPipelineDynamicStateCreateInfo dynamicState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,  //VkStructureType
      .pNext = nullptr,                                               //const void*
      .flags = 0,                                                     //VkPipelineDynamicStateCreateFlags
      .dynamicStateCount = (uint32_t)dynamicStates.size(),            //uint32_t
      .pDynamicStates = dynamicStates.data(),                         //const VkDynamicState*
    };

    //*Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,  //VkStructureType
      .pNext = nullptr,                                                   //const void*
      .flags = 0,                                                         //VkPipelineMultisampleStateCreateFlags
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,                      //VkSampleCountFlagBits ** Change for MSAA

      //**Note:
      //I think this is only available if multisampling is enabled.
      //OpenGL: glEnable(GL_SAMPLE_SHADING); glMinSampleShading(0.2f); glGet..(GL_MIN_SAMPLE_SHADING)
      .sampleShadingEnable = (VkBool32)(g_samplerate_shading ? VK_TRUE : VK_FALSE),  //VkBool32
      .minSampleShading = 1.0f,                                                      //float
      .pSampleMask = nullptr,                                                        //const VkSampleMask*
      .alphaToCoverageEnable = false,                                                //VkBool32
      .alphaToOneEnable = false,                                                     //VkBool32
    };
    VkGraphicsPipelineCreateInfo pipelineInfo = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,  //VkStructureType
      .pNext = nullptr,                                          //const void*
      .flags = 0,                                                //VkPipelineCreateFlags
      .stageCount = 2,                                           //uint32_t
      .pStages = shaderStages.data(),                            //const VkPipelineShaderStageCreateInfo*
      .pVertexInputState = &vertexInputInfo,                     //const VkPipelineVertexInputStateCreateInfo*
      .pInputAssemblyState = &inputAssembly,                     //const VkPipelineInputAssemblyStateCreateInfo*
      .pTessellationState = nullptr,                             //const VkPipelineTessellationStateCreateInfo*
      .pViewportState = &viewportState,                          //const VkPipelineViewportStateCreateInfo*
      .pRasterizationState = &rasterizer,                        //const VkPipelineRasterizationStateCreateInfo*
      .pMultisampleState = &multisampling,                       //const VkPipelineMultisampleStateCreateInfo*
      .pDepthStencilState = nullptr,                             //const VkPipelineDepthStencilStateCreateInfo*
      .pColorBlendState = &colorBlending,                        //const VkPipelineColorBlendStateCreateInfo*
      .pDynamicState = nullptr,                                  //const VkPipelineDynamicStateCreateInfo*
      .layout = _pipelineLayout,                                 //VkPipelineLayout
      .renderPass = _renderPass,                                 //VkRenderPass
      .subpass = 0,                                              //uint32_t
      .basePipelineHandle = VK_NULL_HANDLE,                      //VkPipeline
      .basePipelineIndex = -1,                                   //int32_t
    };

    // BRLogInfo(VulkanDebug::get_VkGraphicsPipelineCreateInfo());

    CheckVKR(vkCreateGraphicsPipelines, vulkan()->device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_graphicsPipeline);
  }
  void createRenderPass() {
    BRLogInfo("Creating Render Pass.");

    //This is essentially a set of data that we supply to VkCmdRenderPass
    //VkCmdRenderPass -> RenderPassInfo.colorAttachment -> framebuffer index.
    // VkAttachmentDescription depthAttachment = {
    //   .flags = 0,                                                       //VkAttachmentDescriptionFlags
    //   .format = findDepthFormat(),                                      //VkFormat
    //   .samples = VK_SAMPLE_COUNT_1_BIT,                                 //VkSampleCountFlagBits
    //   .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,                            //VkAttachmentLoadOp
    //   .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,                      //VkAttachmentStoreOp
    //   .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,                 //VkAttachmentLoadOp
    //   .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,               //VkAttachmentStoreOp
    //   .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,                       //VkImageLayout - discard initial data.
    //   .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,  //VkImageLayout
    // };
    // VkAttachmentReference depthAttachmentRef = {
    //   .attachment = 1,
    //   .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    // };

    VkAttachmentDescription colorAttachment = {
      .flags = 0,                                          //VkAttachmentDescriptionFlags
      .format = _swapChainImageFormat,                     //VkFormat
      .samples = VK_SAMPLE_COUNT_1_BIT,                    //VkSampleCountFlagBits
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,               //VkAttachmentLoadOp
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,             //VkAttachmentStoreOp
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,    //VkAttachmentLoadOp
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,  //VkAttachmentStoreOp
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,          //VkImageLayout - discard initial data.
      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,      //VkImageLayout
    };

    //Framebuffer attachments.
    VkAttachmentReference colorAttachmentRef = {
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    //Subpass.
    VkSubpassDescription subpass = {
      .flags = 0,                                                  //VkSubpassDescriptionFlags
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,        //VkPipelineBindPoint
      .inputAttachmentCount = 0,                                   //uint32_t
      .pInputAttachments = nullptr,                                //const VkAttachmentReference*
      .colorAttachmentCount = 1,                                   //uint32_t
      .pColorAttachments = &colorAttachmentRef,                    //const VkAttachmentReference*
      .pResolveAttachments = nullptr,                              //const VkAttachmentReference* - For MSAA downsampling to final buffer.
      .pDepthStencilAttachment = nullptr /*&depthAttachmentRef*/,  //const VkAttachmentReference*
      .preserveAttachmentCount = 0,                                //uint32_t
      .pPreserveAttachments = nullptr,                             //const uint32_t*
    };

    VkRenderPassCreateInfo renderPassInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,  //VkStructureType
      .pNext = nullptr,                                    //const void*
      .flags = 0,                                          //VkRenderPassCreateFlags
      .attachmentCount = 1,                                //uint32_t
      .pAttachments = &colorAttachment,                    //const VkAttachmentDescription*
      .subpassCount = 1,                                   //uint32_t
      .pSubpasses = &subpass,                              //const VkSubpassDescription*
      .dependencyCount = 0,                                //uint32_t
      .pDependencies = nullptr,                            //const VkSubpassDependency*
    };
    CheckVKR(vkCreateRenderPass, vulkan()->device(), &renderPassInfo, nullptr, &_renderPass);
  }

  void createFramebuffers() {
    BRLogInfo("Creating Framebuffers.");

    _swapChainFramebuffers.resize(_swapChainImageViews.size());

    for (size_t i = 0; i < _swapChainImageViews.size(); i++) {
      VkImageView attachments[] = { _swapChainImageViews[i] };

      VkFramebufferCreateInfo framebufferInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,  //VkStructureType
        .pNext = nullptr,                                    //const void*
        .flags = 0,                                          //VkFramebufferCreateFlags
        .renderPass = _renderPass,                           //VkRenderPass
        .attachmentCount = 1,                                //uint32_t
        .pAttachments = attachments,                         //const VkImageView*
        .width = _swapChainExtent.width,                     //uint32_t
        .height = _swapChainExtent.height,                   //uint32_t
        .layers = 1,                                         //uint32_t
      };

      CheckVKR(vkCreateFramebuffer, vulkan()->device(), &framebufferInfo, nullptr, &_swapChainFramebuffers[i]);
    }
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
      .commandPool = vulkan()->commandPool(),
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = (uint32_t)_commandBuffers.size(),
    };
    CheckVKR(vkAllocateCommandBuffers, vulkan()->device(), &allocInfo, _commandBuffers.data());

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
      VkRenderPassBeginInfo rpbi{};
      rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
      rpbi.pNext = nullptr;
      rpbi.renderPass = _renderPass;  //The renderpass we created above.
      rpbi.framebuffer = _swapChainFramebuffers[i];
      //rpbi.renderArea{};
      rpbi.renderArea.offset = VkOffset2D{ .x = 0, .y = 0 };
      rpbi.renderArea.extent = _swapChainExtent;
      rpbi.clearValueCount = 1;
      rpbi.pClearValues = &clearColor;
      CheckVKR(vkBeginCommandBuffer, _commandBuffers[i], &beginInfo);

      vkCmdBeginRenderPass(_commandBuffers[i], &rpbi, VK_SUBPASS_CONTENTS_INLINE /*VkSubpassContents*/);

      //VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
      //vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
      //VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
      //vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

      //You can bind multiple pipelines for multiple render passes.
      //Binding one does not disturb the others.
      vkCmdBindPipeline(_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);
      //You can have multiple vertex layouts with layout(binding=x)
      VkBuffer vertexBuffers[] = { _vertexBuffer->hostBuffer()->buffer() };
      VkDeviceSize offsets[] = { 0 };
      vkCmdBindVertexBuffers(_commandBuffers[i], 0, 1, vertexBuffers, offsets);
      vkCmdBindIndexBuffer(_commandBuffers[i], _indexBuffer->hostBuffer()->buffer(), 0, VK_INDEX_TYPE_UINT32);  // VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16
      vkCmdBindDescriptorSets(_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout,
                              0,  //Layout ID
                              1,
                              &_descriptorSets[i], 0, nullptr);

      //*The 1 is instances - for instanced rendering
      vkCmdDrawIndexed(_commandBuffers[i], static_cast<uint32_t>(_boxInds.size()), _numInstances, 0, 0, 0);
      //vkCmdDrawIndexedIndirect

      //This is called once when all pipeline rendering is complete
      vkCmdEndRenderPass(_commandBuffers[i]);
      CheckVKR(vkEndCommandBuffer, _commandBuffers[i]);
    }
  }
  void createSyncObjects() {
    BRLogInfo("Creating Rendering Semaphores.");
    _imageAvailableSemaphores.resize(_iConcurrentFrames);
    _renderFinishedSemaphores.resize(_iConcurrentFrames);
    _inFlightFences.resize(_iConcurrentFrames);
    _imagesInFlight.resize(_swapChainImages.size(), VK_NULL_HANDLE);

    for (int i = 0; i < _iConcurrentFrames; ++i) {
      VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
      };
      CheckVKR(vkCreateSemaphore, vulkan()->device(), &semaphoreInfo, nullptr, &_imageAvailableSemaphores[i]);
      CheckVKR(vkCreateSemaphore, vulkan()->device(), &semaphoreInfo, nullptr, &_renderFinishedSemaphores[i]);

      VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,  //Fences must always be created in a signaled state.
      };

      CheckVKR(vkCreateFence, vulkan()->device(), &fenceInfo, nullptr, &_inFlightFences[i]);
    }
  }
#pragma endregion

#pragma region Vulkan Rendering

  void drawFrame() {
    vkWaitForFences(vulkan()->device(), 1, &_inFlightFences[_currentFrame], VK_TRUE, UINT64_MAX);
    //one command buffer per framebuffer,
    //acquire the image form teh swapchain (which is our framebuffer)
    //signal the semaphore.
    uint32_t imageIndex = 0;
    //Returns an image in the swapchain that we can draw to.
    VkResult res;
    res = vkAcquireNextImageKHR(vulkan()->device(), _swapChain, UINT64_MAX, _imageAvailableSemaphores[_currentFrame], VK_NULL_HANDLE, &imageIndex);
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
        vulkan()->validateVkResult(res, "vkAcquireNextImageKHR");
      }
    }

    //There is currently a frame that is using this image. So wait for this image.
    if (_imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
      vkWaitForFences(vulkan()->device(), 1, &_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    _imagesInFlight[imageIndex] = _inFlightFences[_currentFrame];

    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    //E.G. game logic
    updateUniformBuffer(imageIndex);

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

    vkResetFences(vulkan()->device(), 1, &_inFlightFences[_currentFrame]);
    vkQueueSubmit(vulkan()->graphicsQueue(), 1, &submitInfo, _inFlightFences[_currentFrame]);

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
    res = vkQueuePresentKHR(vulkan()->presentQueue(), &presentinfo);
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
        vulkan()->validateVkResult(res, "vkAcquireNextImageKHR");
      }
    }

    //**CONTINUE TUTORIAL AFTER THIS POINT
    //https://vulkan-tutorial.com/en/Drawing_a_triangle/Drawing/Rendering_and_presentation
    // We add additional threads for async the rendering.
    //vkQueueWaitIdle(_presentQueue);  //Waits for operations to complete to prevent overfilling the command buffers .

    _currentFrame = (_currentFrame + 1) % _iConcurrentFrames;
  }

  typedef v_v3c4x2 VertType;

  std::vector<v_v3c4x2> _boxVerts;
  std::vector<uint32_t> _boxInds;
  std::vector<v_v2c4> _planeVerts;
  std::vector<uint32_t> _planeInds;

  void makePlane() {
    //    vec2(0.0, -.5),
    //vec2(.5, .5),
    //vec2(-.5, .5)
    _planeVerts = {
      { { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f, 1 } },
      { { 0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f, 1 } },
      { { 0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f, 1 } },
      { { -0.5f, 0.5f }, { 1.0f, 1.0f, 1.0f, 1 } }
    };
    _planeInds = {
      0, 1, 2, 2, 3, 0
    };

    // _planeVerts = {
    //   { { 0.0f, -0.5f }, { 1, 0, 0, 1 } },
    //   { { 0.5f, 0.5f }, { 0, 1, 0, 1 } },
    //   { { -0.5f, 0.5f }, { 0, 0, 1, 1 } }
    // };

    size_t v_datasize = sizeof(v_v2c4) * _planeVerts.size();

    _vertexBuffer = std::make_shared<VulkanBuffer>(
      vulkan(),
      VulkanBufferType::VertexBuffer,
      true,
      v_datasize,
      _planeVerts.data(), v_datasize);

    size_t i_datasize = sizeof(uint32_t) * _planeInds.size();
    _indexBuffer = std::make_shared<VulkanBuffer>(
      vulkan(),
      VulkanBufferType::IndexBuffer,
      true,
      i_datasize,
      _planeInds.data(), i_datasize);
  }
  void makeBox() {
    //v_v3c4
    // _boxVerts = {
    //   { { 0, 0, 0 }, { 1, 1, 1, 1 } },
    //   { { 1, 0, 0 }, { 0, 0, 1, 1 } },
    //   { { 0, 1, 0 }, { 1, 0, 1, 1 } },
    //   { { 1, 1, 0 }, { 1, 1, 0, 1 } },
    //   { { 0, 0, 1 }, { 0, 0, 1, 1 } },
    //   { { 1, 0, 1 }, { 1, 0, 0, 1 } },
    //   { { 0, 1, 1 }, { 0, 1, 0, 1 } },
    //   { { 1, 1, 1 }, { 1, 0, 1, 1 } },
    // };
    // //      6     7
    // //  2      3
    // //      4     5
    // //  0      1
    // _boxInds = {
    //   0, 3, 1, /**/ 0, 2, 3,  //
    //   1, 3, 7, /**/ 1, 7, 5,  //
    //   5, 7, 6, /**/ 5, 6, 4,  //
    //   4, 6, 2, /**/ 4, 2, 0,  //
    //   2, 6, 7, /**/ 2, 7, 3,  //
    //   4, 0, 1, /**/ 4, 1, 5,  //
    // };

    //      6     7
    //  2      3
    //      4     5
    //  0      1
    std::vector<v_v3c4> bv = {
      { { 0, 0, 0 }, { 1, 1, 1, 1 } },
      { { 1, 0, 0 }, { 1, 1, 1, 1 } },
      { { 0, 1, 0 }, { 1, 1, 1, 1 } },
      { { 1, 1, 0 }, { 1, 1, 1, 1 } },
      { { 0, 0, 1 }, { 1, 1, 1, 1 } },
      { { 1, 0, 1 }, { 1, 1, 1, 1 } },
      { { 0, 1, 1 }, { 1, 1, 1, 1 } },
      { { 1, 1, 1 }, { 1, 1, 1, 1 } },
    };
    //Construct box from the old box coordintes (no texture)
    //Might be wrong - opengl coordinates.
#define BV_VFACE(bl, br, tl, tr)              \
  { bv[bl]._pos, bv[bl]._color, { 0, 1 } },   \
    { bv[br]._pos, bv[br]._color, { 1, 1 } }, \
    { bv[tl]._pos, bv[tl]._color, { 0, 0 } }, \
  {                                           \
    bv[tr]._pos, bv[tr]._color, { 1, 0 }      \
  }

    _boxVerts = {
      BV_VFACE(0, 1, 2, 3),  //F
      BV_VFACE(1, 5, 3, 7),  //R
      BV_VFACE(5, 4, 7, 6),  //A
      BV_VFACE(4, 0, 6, 2),  //L
      BV_VFACE(4, 5, 0, 1),  //B
      BV_VFACE(2, 3, 6, 7)   //T
    };

//   CW
//  2------>3
//  |    /
//  | /
//  0------>1
#define BV_IFACE(idx) ((idx * 4) + 0), ((idx * 4) + 3), ((idx * 4) + 1), ((idx * 4) + 0), ((idx * 4) + 2), ((idx * 4) + 3)
    _boxInds = {
      BV_IFACE(0),
      BV_IFACE(1),
      BV_IFACE(2),
      BV_IFACE(3),
      BV_IFACE(4),
      BV_IFACE(5),
    };
    size_t v_datasize = sizeof(VertType) * _boxVerts.size();

    _vertexBuffer = std::make_shared<VulkanBuffer>(
      vulkan(),
      VulkanBufferType::VertexBuffer,
      true,
      v_datasize,
      _boxVerts.data(), v_datasize);

    size_t i_datasize = sizeof(uint32_t) * _boxInds.size();
    _indexBuffer = std::make_shared<VulkanBuffer>(
      vulkan(),
      VulkanBufferType::IndexBuffer,
      true,
      i_datasize,
      _boxInds.data(), i_datasize);
  }

  void createVertexBuffer() {
    makeBox();

    //makePlane();
  }
  std::shared_ptr<Img32> loadImage(const string_t& img) {
    unsigned char* data = nullptr;  //the raw pixels
    unsigned int width, height;

    std::vector<char> image_data = Gu::readFile(img);

    int err = lodepng_decode32(&data, &width, &height, (unsigned char*)image_data.data(), image_data.size());

    int required_bytes = 4;

    if (err != 0) {
      vulkan()->errorExit("LodePNG could not load image, error: " + err);
      return nullptr;
    }
    std::shared_ptr<Img32> ret = std::make_shared<Img32>();
    ret->_width = width;
    ret->_height = height;
    ret->_data = data;
    ret->data_len_bytes = width * height * required_bytes;

    return ret;
  }
  void createTextureImage() {
    // auto img = loadImage(App::rootFile("test.png"));
    auto img = loadImage(App::rootFile("TexturesCom_MetalBare0253_2_M.png"));
    if (img) {
      _testTexture = std::make_shared<VulkanTextureImage>(vulkan(), img, g_mipmap_mode);
    }
    else {
      vulkan()->errorExit("Could not load test image.");
    }

    // _depthTexture = = std::make_shared<VulkanImage>(vulkan(), img);
  }
#pragma endregion

  void cleanupSwapChain() {
    _viewProjUniformBuffers.resize(0);
    _layouts.resize(0);
    _descriptorSets.resize(0);
    _testTexture = nullptr;
    _depthTexture = nullptr;
    vkDestroyDescriptorSetLayout(vulkan()->device(), _descriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(vulkan()->device(), _descriptorPool, nullptr);

    if (_swapChainFramebuffers.size() > 0) {
      for (auto& v : _swapChainFramebuffers) {
        if (v != VK_NULL_HANDLE) {
          vkDestroyFramebuffer(vulkan()->device(), v, nullptr);
        }
      }
      _swapChainFramebuffers.clear();
    }

    if (_commandBuffers.size() > 0) {
      vkFreeCommandBuffers(vulkan()->device(), vulkan()->commandPool(),
                           static_cast<uint32_t>(_commandBuffers.size()), _commandBuffers.data());
      _commandBuffers.clear();
    }

    if (_graphicsPipeline != VK_NULL_HANDLE) {
      vkDestroyPipeline(vulkan()->device(), _graphicsPipeline, nullptr);
    }
    if (_pipelineLayout != VK_NULL_HANDLE) {
      vkDestroyPipelineLayout(vulkan()->device(), _pipelineLayout, nullptr);
    }
    if (_renderPass != VK_NULL_HANDLE) {
      vkDestroyRenderPass(vulkan()->device(), _renderPass, nullptr);
    }

    for (size_t i = 0; i < _swapChainImageViews.size(); i++) {
      vkDestroyImageView(vulkan()->device(), _swapChainImageViews[i], nullptr);
    }
    _swapChainImageViews.clear();

    if (_swapChain != VK_NULL_HANDLE) {
      vkDestroySwapchainKHR(vulkan()->device(), _swapChain, nullptr);
    }
    //We don't need to re-create sync objects on window resize but since this data depends on the swapchain, it makes a little cleaner.
    for (auto& sem : _renderFinishedSemaphores) {
      vkDestroySemaphore(vulkan()->device(), sem, nullptr);
    }
    _renderFinishedSemaphores.clear();
    for (auto& sem : _imageAvailableSemaphores) {
      vkDestroySemaphore(vulkan()->device(), sem, nullptr);
    }
    _imageAvailableSemaphores.clear();
    for (auto& fence : _inFlightFences) {
      vkDestroyFence(vulkan()->device(), fence, nullptr);
    }
    _inFlightFences.clear();
    _imagesInFlight.clear();
  }
  void cleanup() {
    // All child objects created using instance must have been destroyed prior to destroying instance - Vulkan Spec.

    //_pSwapchain = nullptr
    cleanupSwapChain();

    _pShader = nullptr;

    _vulkan = nullptr;

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

bool SDLVulkan::doInput() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      return true;
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
        return true;
        break;
      }
      else if (event.key.keysym.scancode == SDL_SCANCODE_F1) {
        g_mipmap_mode = (MipmapMode)(((int)g_mipmap_mode + 1) % ((int)MipmapMode::MipmapMode_Count));

        string_t mmm = (g_mipmap_mode == MipmapMode::Linear) ? ("Linear") : ((g_mipmap_mode == MipmapMode::Nearest) ? ("Nearest") : ((g_mipmap_mode == MipmapMode::Disabled) ? ("Disabled") : ("Undefined-Error")));

        string_t mode = _pInt->base_title + " (" + mmm + ") ";

        SDL_SetWindowTitle(_pInt->_pSDLWindow, mode.c_str());
        _pInt->recreateSwapChain();
        break;
      }
      else if (event.key.keysym.scancode == SDL_SCANCODE_F2) {
        g_samplerate_shading = !g_samplerate_shading;
        _pInt->recreateSwapChain();
        break;
      }
    }
  }
  return false;
}
void SDLVulkan::renderLoop() {
  bool exit = false;
  while (!exit) {
    exit = doInput();
    _pInt->drawFrame();
  }
  //This stops all threads before we cleanup.
  vkDeviceWaitIdle(_pInt->vulkan()->device());
  _pInt->cleanup();
}

}  // namespace VG
