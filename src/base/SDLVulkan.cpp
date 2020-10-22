#include "./SDLVulkan.h"
#include "./VulkanHeader.h"
#include "./Vulkan.h"
#include "./VulkanDebug.h"
#include "./VulkanClasses.h"
#include "./GameClasses.h"

namespace VG {

static MipmapMode g_mipmap_mode = MipmapMode::Linear;  //**TESTING**
static bool g_samplerate_shading = true;

#pragma region SDLVulkan_Internal

//Data from Vulkan-Tutorial
class SDLVulkan_Internal {
public:
#pragma region Members
  SDL_Window* _pSDLWindow = nullptr;
  std::shared_ptr<Vulkan> _vulkan = nullptr;
  std::shared_ptr<Vulkan> vulkan() { return _vulkan; }

  std::shared_ptr<VulkanTextureImage> _testTexture1 = nullptr;
  std::shared_ptr<VulkanTextureImage> _testTexture2 = nullptr;
  std::shared_ptr<VulkanDepthImage> _depthTexture = nullptr;  //This is not implemented

  std::shared_ptr<PipelineShader> _pShader = nullptr;
  std::shared_ptr<GameDummy> _game = nullptr;
  std::shared_ptr<Swapchain> _pSwapchain = nullptr;

  std::vector<std::shared_ptr<VulkanBuffer>> _viewProjUniformBuffers;   //One per swapchain image since theres multiple frames in flight.
  std::vector<std::shared_ptr<VulkanBuffer>> _instanceUniformBuffers1;  //One per swapchain image since theres multiple frames in flight.
  std::vector<std::shared_ptr<VulkanBuffer>> _instanceUniformBuffers2;  //One per swapchain image since theres multiple frames in flight.
  int32_t _numInstances = 3;

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
    _pSwapchain = std::make_shared<Swapchain>(_vulkan);
    vulkan()->setSwapchain(_pSwapchain);
    _pSwapchain->initSwapchain(getWindowDims().size);

    _game = std::make_shared<GameDummy>();
    _game->_mesh1 = std::make_shared<Mesh>(_vulkan);
    _game->_mesh1->makeBox();
    _game->_mesh2 = std::make_shared<Mesh>(_vulkan);
    _game->_mesh2->makeBox();

    //Make Shader.
    _pShader = PipelineShader::create(_vulkan, "Vulkan-Tutorial-Test-Shader",
                                      std::vector{ App::rootFile("test_vs.spv"), App::rootFile("test_fs.spv") });
    allocateShaderMemory();

    BRLogInfo("Showing window..");
    SDL_ShowWindow(_pSDLWindow);
  }
  //std::shared_ptr<Pipeline> _pipe = nullptr;
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
  BR2::urect2 getWindowDims() {
    int win_w = 0, win_h = 0;
    int pos_x = 0, pos_y = 0;
    SDL_GetWindowSize(_pSDLWindow, &win_w, &win_h);
    SDL_GetWindowPosition(_pSDLWindow, &pos_x, &pos_y);
    return {
      static_cast<uint32_t>(pos_x),
      static_cast<uint32_t>(pos_y),
      static_cast<uint32_t>(win_w),
      static_cast<uint32_t>(win_h)
    };
  }
  void createUniformBuffers() {
    for (size_t i = 0; i < _pSwapchain->frames().size(); ++i) {
      _viewProjUniformBuffers.push_back(std::make_shared<VulkanBuffer>(
        vulkan(),
        VulkanBufferType::UniformBuffer,
        false,  //not on GPU
        sizeof(ViewProjUBOData), nullptr, 0));
      _instanceUniformBuffers1.push_back(std::make_shared<VulkanBuffer>(
        vulkan(),
        VulkanBufferType::UniformBuffer,
        false,  //not on GPU
        sizeof(InstanceUBOData) * _numInstances, nullptr, 0));
      _instanceUniformBuffers2.push_back(std::make_shared<VulkanBuffer>(
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

  void init_rand() {
  }
  //auto now = std::chrono::system_clock::now().time_since_epoch().count();
  std::mt19937 _rnd_engine;
  std::uniform_real_distribution<double> _rnd_distribution;  //0,1
//#define fr01() (((double)rand() / (double)RAND_MAX))
#define fr01() (_rnd_distribution(_rnd_engine))
#define rnd(a, b) ((a) + ((b) - (a)) * (fr01()))
#define rr ((float)rnd(-3, 3))
  std::vector<BR2::vec3> offsets1;
  std::vector<BR2::vec3> offsets2;
  std::vector<float> rots_delta1;
  std::vector<float> rots_delta2;
  std::vector<float> rots_ini1;
  std::vector<float> rots_ini2;
  void tryInitializeOffsets(std::vector<BR2::vec3>& offsets, std::vector<float>& rots_delta, std::vector<float>& rots_ini) {
    if (offsets.size() == 0) {
      for (size_t i = 0; i < _numInstances; ++i) {
        offsets.push_back({ rr, rr, rr });
        rots_delta.push_back(rnd(-M_2PI, M_2PI));  //rotation delta.
        rots_ini.push_back(rnd(-M_2PI, M_2PI));    // Initial rotation, and also the value of current rotation.
      }
    }
  }

  void updateViewProjUniformBuffer(std::shared_ptr<VulkanBuffer> viewProjBuffer) {
    //Push constants are faster.
    float t01 = pingpong_t01(10000);

    float d = 20;
    BR2::vec3 campos = { d, d, d };
    BR2::vec3 lookAt = { 0, 0, 0 };
    BR2::vec3 wwf = (lookAt - campos) * 0.1f;
    BR2::vec3 trans = campos + wwf + (lookAt - campos - wwf) * t01;
    ViewProjUBOData ub = {
      .view = BR2::mat4::getLookAt(campos, lookAt, BR2::vec3(0.0f, 0.0f, 1.0f)),
      .proj = BR2::mat4::projection(BR2::MathUtils::radians(45.0f), (float)_pSwapchain->imageSize().width, -(float)_pSwapchain->imageSize().height, 0.1f, 100.0f)
    };
    viewProjBuffer->writeData((void*)&ub, 0, sizeof(ViewProjUBOData));
  }
  void updateInstanceUniformBuffer(std::shared_ptr<VulkanBuffer> instanceBuffer, std::vector<BR2::vec3>& offsets, std::vector<float>& rots_delta, std::vector<float>& rots_ini, float dt) {
    float t01 = pingpong_t01(10000);
    float t02 = pingpong_t01(10000);
    tryInitializeOffsets(offsets, rots_delta, rots_ini);

    float d = 20;
    BR2::vec3 campos = { d, d, d };
    BR2::vec3 lookAt = { 0, 0, 0 };
    BR2::vec3 wwf = (lookAt - campos) * 0.1f;
    BR2::vec3 trans = campos + wwf + (lookAt - campos - wwf) * t01;

    BR2::vec3 origin = { -0.5, -0.5, -0.5 };  //cube origin
    std::vector<BR2::mat4> mats(_numInstances);
    for (uint32_t i = 0; i < _numInstances; ++i) {
      if (i < offsets.size()) {
        rots_ini[i] += rots_delta[i] * dt;
        mats[i] = BR2::mat4::translation(origin) *
                  BR2::mat4::rotation(rots_ini[i], BR2::vec3(0, 0, 1)) *
                  BR2::mat4::translation(trans + offsets[i]);
      }
    }
    instanceBuffer->writeData(mats.data(), 0, sizeof(mats[0]) * mats.size());
    // ub.proj._m22 *= -1;
  }

#pragma endregion

#pragma region Vulkan Rendering

  uint64_t g_iFrameNumber = 0;
  void drawFrame(double dt) {
    _pSwapchain->beginFrame();
    {
      std::shared_ptr<RenderFrame> frame = _pSwapchain->acquireFrame();
      if (frame != nullptr) {
        uint32_t frameIndex = frame->frameIndex();

        //* game logic
        updateViewProjUniformBuffer(_viewProjUniformBuffers[frameIndex]);
        updateInstanceUniformBuffer(_instanceUniformBuffers1[frameIndex], offsets1, rots_delta1, rots_ini1, dt);
        updateInstanceUniformBuffer(_instanceUniformBuffers2[frameIndex], offsets2, rots_delta2, rots_ini2, dt);

        //Simplfy
        //if(!_pShader->draw({game->mesh1, game->mesh2})){
        // std::cout<<Mesh wan't compatible with shader<<std::endl
        //}
        /*

      //Script
        Shader s  = Shader({base_mesh.vs, base_mesh.fs});
        Mesh m = loadMesh("Character")
        m.setShader(s)

        auto ob = Game::createObject("MyChar");
        ob.addComponent(m);

        Camera c;
        c.lookAt({0,0,0});
        c.position({10,10,10});

        Toolbar tb;
        tb.addChild({
          Button{.width=100, .text="Wave" .push=[](){ Game::getOb("Character").animate("Wave"); },
          Button{.width=100, .text="Dance" .push=[](){ Game::getOb("Character").animate("Dance"); },
          });
        
        game_loop{
          shader->begin()
          shader->draw(mesh)
          shader->end()
        }

        */

        // * Draw
        recordCommandBuffer(frame);
      }
    }
    _pSwapchain->endFrame();
    g_iFrameNumber++;
  }
  void recordCommandBuffer(std::shared_ptr<RenderFrame> frame) {
    uint32_t frameIndex = frame->frameIndex();

    auto cmd = frame->commandBuffer();
    cmd->begin();
    {
      if (cmd->beginPass(_pShader, frame)) {
        //I'm not sure if you have to bind pipeline first, but if you dont we can move this back into shader::bind.
        auto pipe = _pShader->getPipeline(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_FILL);
        pipe->bind(cmd);

        cmd->cmdSetViewport({ { 0, 0 }, _pSwapchain->imageSize() });

        //** TODO:
        //_pShader->bindUniforms({"_uboViewProj", "_uboInstanceData"});

        //Mesh 1
        _pShader->bindSampler("_ufTexture0", frameIndex, _testTexture1);
        _pShader->bindUBO("_uboViewProj", frameIndex, _viewProjUniformBuffers[frameIndex]);
        _pShader->bindUBO("_uboInstanceData", frameIndex, _instanceUniformBuffers1[frameIndex]);
        _pShader->bindDescriptors(cmd, pipe, frameIndex);
        pipe->drawIndexed(cmd, _game->_mesh1, _numInstances);

        _pShader->bindSampler("_ufTexture0", frameIndex, _testTexture2);
        _pShader->bindUBO("_uboInstanceData", frameIndex, _instanceUniformBuffers2[frameIndex]);
        _pShader->bindDescriptors(cmd, pipe, frameIndex);
        pipe->drawIndexed(cmd, _game->_mesh2, _numInstances);

        cmd->endPass();
      }
    }
    cmd->end();
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
  void createTextureImages() {
    // auto img = loadImage(App::rootFile("test.png"));
    auto img = loadImage(App::rootFile("TexturesCom_MetalBare0253_2_M.png"));
    if (img) {
      _testTexture1 = std::make_shared<VulkanTextureImage>(vulkan(), img, g_mipmap_mode);
    }
    else {
      vulkan()->errorExit("Could not load test image 1.");
    }
    auto img2 = loadImage(App::rootFile("test.png"));
    if (img2) {
      _testTexture2 = std::make_shared<VulkanTextureImage>(vulkan(), img2, g_mipmap_mode);
    }
    else {
      vulkan()->errorExit("Could not load test image 2.");
    }
    // _depthTexture = = std::make_shared<VulkanImage>(vulkan(), img);
  }
#pragma endregion

  void allocateShaderMemory() {
    cleanupShaderMemory();

    createUniformBuffers();  // - create once to not be recreated when we genericize swapchain
    createTextureImages();   // - create once - single creation
  }
  void cleanupShaderMemory() {
    _viewProjUniformBuffers.resize(0);
    _instanceUniformBuffers1.resize(0);
    _instanceUniformBuffers2.resize(0);
    _testTexture1 = nullptr;
    _testTexture2 = nullptr;
    _depthTexture = nullptr;
  }
  void cleanupSwapRenderPipe() {
    // if (_graphicsPipeline != VK_NULL_HANDLE) {
    //   vkDestroyPipeline(vulkan()->device(), _graphicsPipeline, nullptr);
    // }
    // if (_pipelineLayout != VK_NULL_HANDLE) {
    //   vkDestroyPipelineLayout(vulkan()->device(), _pipelineLayout, nullptr);
    // }
    // if (_renderPass != VK_NULL_HANDLE) {
    //   vkDestroyRenderPass(vulkan()->device(), _renderPass, nullptr);
    // }
  }
  void cleanup() {
    // All child objects created using instance must have been destroyed prior to destroying instance - Vulkan Spec.

    _pSwapchain = nullptr;
    cleanupSwapRenderPipe();

    //cleanupSyncObjects();
    cleanupShaderMemory();

    _pShader = nullptr;
    _vulkan = nullptr;

    SDL_DestroyWindow(_pSDLWindow);
  }
};  // namespace VG

#pragma endregion

#pragma region SDLVulkan

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
  catch (std::exception& ex) {
    std::cout << ex.what() << std::endl;
    _pInt->cleanup();
    throw ex;
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
        _pInt->_pSwapchain->outOfDate();
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
        _pInt->_pSwapchain->outOfDate();
        break;
      }
      else if (event.key.keysym.scancode == SDL_SCANCODE_F2) {
        g_samplerate_shading = !g_samplerate_shading;
        _pInt->_pSwapchain->outOfDate();
        break;
      }
    }
  }
  return false;
}
void SDLVulkan::renderLoop() {
  bool exit = false;
  auto last_time = std::chrono::high_resolution_clock::now();
  while (!exit) {
    exit = doInput();
    double t01ms = std::chrono::duration<double, std::chrono::seconds::period>(std::chrono::high_resolution_clock::now() - last_time).count();
    last_time = std::chrono::high_resolution_clock::now();

    if (_pInt->_pSwapchain->isOutOfDate()) {
      _pInt->_pSwapchain->initSwapchain(_pInt->getWindowDims().size);
    }

    //TODO: replace this when we relocate the whole pipeline.
    //_pInt->_pSwapchain->updateSwapchain(_pInt->getWindowDims());
    _pInt->_pSwapchain->beginFrame();
    _pInt->_pSwapchain->endFrame();

    _pInt->drawFrame(t01ms);
  }
  //This stops all threads before we cleanup.
  vkDeviceWaitIdle(_pInt->vulkan()->device());
  _pInt->cleanup();
}

#pragma endregion

}  // namespace VG
