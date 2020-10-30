#include "./SDLVulkan.h"
#include "./VulkanHeader.h"
#include "./Vulkan.h"
#include "./VulkanDebug.h"
#include "./VulkanClasses.h"
#include "./GameClasses.h"

namespace VG {
//**Testing
static MipmapMode g_mipmap_mode = MipmapMode::Disabled;
static TexFilter g_min_filter = TexFilter::Linear;
static TexFilter g_mag_filter = TexFilter::Linear;
static bool g_poly_line = false;
static bool g_use_rtt = true;
static int g_pass_test_idx = 3;
static float g_anisotropy = 1;
static MSAA g_multisample = MSAA::Disabled;
bool g_test_img1 = true;
VkCullModeFlags g_cullmode = VK_CULL_MODE_BACK_BIT;

const bool g_wait_fences = false;
const bool g_vsync_enable = false;

#pragma region SDLVulkan_Internal

//Data from Vulkan-Tutorial
class SDLVulkan::SDLVulkan_Internal {
public:
#pragma region Members
  SDL_Window* _pSDLWindow = nullptr;
  std::shared_ptr<Vulkan> _vulkan = nullptr;
  std::shared_ptr<Vulkan> vulkan() { return _vulkan; }

  std::shared_ptr<TextureImage> _testTexture1 = nullptr;
  std::shared_ptr<TextureImage> _testTexture2 = nullptr;

  std::shared_ptr<PipelineShader> _pShader = nullptr;
  std::shared_ptr<GameDummy> _game = nullptr;

  string_t c_viewProjUBO = "c_viewProjUBO";
  string_t c_instanceUBO_1 = "c_instanceUBO_1";
  string_t c_instanceUBO_2 = "c_instanceUBO_2";

  uint32_t _numInstances = 25;
  FpsMeter _fpsMeter_Render;
  FpsMeter _fpsMeter_Update;

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

    bool enableDebug = false;
    #ifdef _DEBUG
    enableDebug = true;
    BRLogInfo("GPU debugging enabled.");
#else
    BRLogInfo("GPU debugging disabled.");
    #endif

    _vulkan = Vulkan::create(title, _pSDLWindow, g_wait_fences, g_vsync_enable, enableDebug);

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
    _pShader->createUBO(c_viewProjUBO, "_uboViewProj");
    _pShader->createUBO(c_instanceUBO_1, "_uboInstanceData", sizeof(InstanceUBOData) * _numInstances);
    _pShader->createUBO(c_instanceUBO_2, "_uboInstanceData", sizeof(InstanceUBOData) * _numInstances);
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
  std::mt19937 _rnd_engine;
  std::uniform_real_distribution<double> _rnd_distribution;  //0,1
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
        rots_delta.push_back((float)rnd(-M_2PI, M_2PI));  //rotation delta.
        rots_ini.push_back((float)rnd(-M_2PI, M_2PI));    // Initial rotation, and also the value of current rotation.
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
      .proj = BR2::mat4::projection((float)BR2::MathUtils::radians(45.0f), (float)_vulkan->swapchain()->windowSize().width, -(float)_vulkan->swapchain()->windowSize().height, 0.1f, 100.0f)
    };
    viewProjBuffer->writeData((void*)&ub, 0, sizeof(ViewProjUBOData));
  }
  void updateInstanceUniformBuffer(std::shared_ptr<VulkanBuffer> instanceBuffer, std::vector<BR2::vec3>& offsets, std::vector<float>& rots_delta, std::vector<float>& rots_ini, float dt) {
    float t01 = pingpong_t01(10000);
    float t02 = pingpong_t01(10000);
    tryInitializeOffsets(offsets, rots_delta, rots_ini);

    //This slowdown (e.g. 2500fps -> 80fps) is my code mat4 multiplies.
    // We could dispatch the "instance update" and also any additional physics into a compute shader to get the result more quickily.
    // Will we really have 1000's of dynamic instances updating per frame? Probably not, but it's fun to think about.
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
  void drawFrame() {
    if (_vulkan->swapchain()->beginFrame(getWindowDims().size)) {
      static auto last_time = std::chrono::high_resolution_clock::now();
      double t01 = std::chrono::duration<double, std::chrono::seconds::period>(std::chrono::high_resolution_clock::now() - last_time).count();
      last_time = std::chrono::high_resolution_clock::now();

      std::shared_ptr<RenderFrame> frame = _vulkan->swapchain()->currentFrame();
      if (frame != nullptr) {
        recordCommandBuffer(frame, t01);
      }
      _vulkan->swapchain()->endFrame();
      _fpsMeter_Render.update();
      g_iFrameNumber++;
    }
  }
  std::shared_ptr<RenderTexture> test_render_texture = nullptr;
  void recordCommandBuffer(std::shared_ptr<RenderFrame> frame, double dt) {
    uint32_t frameIndex = frame->frameIndex();

    auto viewProj = _pShader->getUBO(c_viewProjUBO, frame);
    auto inst1 = _pShader->getUBO(c_instanceUBO_1, frame);
    auto inst2 = _pShader->getUBO(c_instanceUBO_2, frame);
    updateViewProjUniformBuffer(viewProj);
    //  if (_fpsMeter_Update.frameMod(50)) {
    updateInstanceUniformBuffer(inst1, offsets1, rots_delta1, rots_ini1, (float)dt);
    updateInstanceUniformBuffer(inst2, offsets2, rots_delta2, rots_ini2, (float)dt);
    // }
    if (test_render_texture == nullptr) {
      test_render_texture = vulkan()->swapchain()->createRenderTexture("Test_RenderTExture", vulkan()->swapchain()->imageFormat(), g_multisample,
                                                                       FilterData{ SamplerType::Sampled, MipmapMode::Disabled, vulkan()->maxAF(),
                                                                                   TexFilter::Linear, TexFilter::Linear, MipLevels::Unset });
    }

    auto cmd = frame->commandBuffer();
    cmd->begin();
    {
      //Testing clear color
      float speed = 0.001f;
      static float c_r = speed;
      static float c_g = 0;
      static float c_b = 0;
      if (c_r > 0 && c_r < 1 && c_g == 0 && c_b == 0) {
        c_r += speed;
        if (c_r >= 1) {
          c_r = 0;
          c_g = speed;
        }
      }
      else if (c_r == 0 && c_g > 0 && c_g < 1 && c_b == 0) {
        c_g += speed;
        if (c_g >= 1) {
          c_g = 0;
          c_b = speed;
        }
      }
      else if (c_r == 0 && c_g == 0 && c_b > 0 && c_b < 1) {
        c_b += speed;
        if (c_b >= 1) {
          c_r = c_g = c_b = 0;
          c_r += speed;
        }
      }

      //Testing Polygon Mode
      auto mode = g_poly_line ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
      auto topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
      //Testing multiple Render Passes.

      //if x=0 test a simple pass
      //otherwise test the complex pass.
      if (g_pass_test_idx == 4) {
        auto simple_pass = _pShader->getPass(frame, g_multisample, BlendFunc::AlphaBlend, FramebufferBlendMode::Independent);
        simple_pass->setOutput(OutputDescription::getColorDF());
        simple_pass->setOutput(OutputDescription::getDepthDF());
        if (_pShader->beginRenderPass(cmd, simple_pass)) {
          if (_pShader->bindPipeline(cmd, nullptr, mode, topo, g_cullmode)) {
            _pShader->bindViewport(cmd, { { 0, 0 }, _vulkan->swapchain()->windowSize() });
            _pShader->bindUBO("_uboViewProj", viewProj);

            _pShader->bindSampler("_ufTexture0", _testTexture1);
            _pShader->bindUBO("_uboInstanceData", inst1);
            _pShader->bindDescriptors(cmd);
            _pShader->drawIndexed(cmd, _game->_mesh1, _numInstances);  //Changed from pipe::drawIndexed
          }
          _pShader->endRenderPass(cmd);
        }
      }
      else {
        bool pass1_success = false;
        if (g_pass_test_idx == 0 || g_pass_test_idx == 1 || g_pass_test_idx == 3) {
          auto pass1 = _pShader->getPass(frame, g_multisample, BlendFunc::AlphaBlend, FramebufferBlendMode::Independent);
          if (g_use_rtt) {
            //The OutputMRT is a problem because it specifies BOTH the shader's bind point AND a type of texture image.
            //Fix this.
            pass1->setOutput("test_render_texture", OutputMRT::RT_DefaultColor, test_render_texture, BlendFunc::AlphaBlend, true, c_r, c_g, c_b);
          }
          else {
            //Todo: Remove colorDF and DepthDF and just use the pass to create the description.
            pass1->setOutput(OutputDescription::getColorDF(nullptr, true, c_r, c_g, c_b));
          }
          pass1->setOutput(OutputDescription::getDepthDF(true));
          if (_pShader->beginRenderPass(cmd, pass1)) {
            if (_pShader->bindPipeline(cmd, nullptr, mode, topo, g_cullmode)) {
              pass1_success = true;
              if (g_pass_test_idx != 0) {
                _pShader->bindViewport(cmd, { { 0, 0 }, _vulkan->swapchain()->windowSize() });
                _pShader->bindUBO("_uboViewProj", viewProj);

                //white face Smiley
                _pShader->bindSampler("_ufTexture0", _testTexture1);
                _pShader->bindUBO("_uboInstanceData", inst2);
                _pShader->bindDescriptors(cmd);
                _pShader->drawIndexed(cmd, _game->_mesh1, _numInstances);  //Changed from pipe::drawIndexed
              }
            }

            _pShader->endRenderPass(cmd);
          }
        }

        if (g_pass_test_idx == 2 || g_pass_test_idx == 3) {
          //black face Smiley
          auto tex = g_use_rtt ? test_render_texture->texture(g_multisample, frame->frameIndex()) : _testTexture2;
          auto pass2 = _pShader->getPass(frame, g_multisample, BlendFunc::AlphaBlend, FramebufferBlendMode::Independent);

          if (pass1_success) {
            pass2->setOutput(OutputDescription::getColorDF(nullptr, g_pass_test_idx == 2 || (g_pass_test_idx == 3 && g_use_rtt)));
            pass2->setOutput(OutputDescription::getDepthDF(g_pass_test_idx == 2 || (g_pass_test_idx == 3 && g_use_rtt)));
          }
          else {
            pass2->setOutput(OutputDescription::getColorDF());
            pass2->setOutput(OutputDescription::getDepthDF());
          }

          if (_pShader->beginRenderPass(cmd, pass2)) {
            if (_pShader->bindPipeline(cmd, nullptr, mode, topo, g_cullmode)) {
              _pShader->bindViewport(cmd, { { 0, 0 }, _vulkan->swapchain()->windowSize() });
              _pShader->bindUBO("_uboViewProj", viewProj);

              //black face smiley
              _pShader->bindSampler("_ufTexture0", pass1_success ? tex : _testTexture2);
              _pShader->bindUBO("_uboInstanceData", inst1);
              _pShader->bindDescriptors(cmd);
              _pShader->drawIndexed(cmd, _game->_mesh2, _numInstances);  //Changed from pipe::drawIndexed
            }
            _pShader->endRenderPass(cmd);
          }
        }

      }  //if x != 0
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
    ret->_size = { width, height };
    ret->_data = data;
    ret->data_len_bytes = width * height * required_bytes;
    ret->_name = img;
    return ret;
  }
  void createTextureImages() {
    // auto img = loadImage(App::rootFile("test.png"));
    auto img = loadImage(App::rootFile(g_test_img1 ? "char-1.png" : "TexturesCom_MetalBare0253_2_M.png"));
    if (img) {
      _testTexture1 = std::make_shared<TextureImage>(vulkan(), img->_name, TextureType::ColorTexture, MSAA::Disabled, img,
                                                     FilterData{ SamplerType::Sampled, g_mipmap_mode, g_anisotropy, g_min_filter,
                                                                 g_mag_filter, MipLevels::Unset });
    }
    else {
      vulkan()->errorExit("Could not load test image 1.");
    }
    auto img2 = loadImage(App::rootFile("char-2.png"));
    if (img2) {
      _testTexture2 = std::make_shared<TextureImage>(vulkan(), img->_name, TextureType::ColorTexture, MSAA::Disabled, img2,
                                                     FilterData{ SamplerType::Sampled, g_mipmap_mode, g_anisotropy, g_min_filter,
                                                                 g_mag_filter, MipLevels::Unset });
    }
    else {
      vulkan()->errorExit("Could not load test image 2.");
    }
  }
#pragma endregion

  void allocateShaderMemory() {
    cleanupShaderMemory();

    createUniformBuffers();
    createTextureImages();
  }
  void cleanupShaderMemory() {
    _testTexture1 = nullptr;
    _testTexture2 = nullptr;
  }
  void cleanup() {
    //This stops all threads before we cleanup.
    vkDeviceWaitIdle(vulkan()->device());

    // All child objects created using instance must have been destroyed prior to destroying instance - Vulkan Spec.
    cleanupShaderMemory();
    _pShader = nullptr;
    _vulkan = nullptr;

    SDL_DestroyWindow(_pSDLWindow);
  }
};

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
        _pInt->_vulkan->swapchain()->outOfDate();
        break;
      }
    }
    else if (event.type == SDL_KEYDOWN) {
      if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
        return true;
        break;
      }
      else if (event.key.keysym.scancode == SDL_SCANCODE_F1) {
        //g_mipmap_mode = (MipmapMode)(((int)g_mipmap_mode + 1) % ((int)MipmapMode::MipmapMode_Count));
        TextureImage::testCycleFilters(g_min_filter, g_mag_filter, g_mipmap_mode);
        _pInt->createTextureImages();
        break;
      }
      else if (event.key.keysym.scancode == SDL_SCANCODE_F2) {
        if (g_cullmode == VK_CULL_MODE_BACK_BIT) {
          g_cullmode = VK_CULL_MODE_FRONT_BIT;
        }
        else if (g_cullmode == VK_CULL_MODE_FRONT_BIT) {
          g_cullmode = VK_CULL_MODE_FRONT_AND_BACK;
        }
        else if (g_cullmode == VK_CULL_MODE_FRONT_AND_BACK) {
          g_cullmode = VK_CULL_MODE_NONE;
        }
        else if (g_cullmode == VK_CULL_MODE_NONE) {
          g_cullmode = VK_CULL_MODE_BACK_BIT;
        }
        break;
      }
      else if (event.key.keysym.scancode == SDL_SCANCODE_F3) {
        g_poly_line = !g_poly_line;
        break;
      }
      else if (event.key.keysym.scancode == SDL_SCANCODE_F4) {
        g_use_rtt = !g_use_rtt;
        break;
      }
      else if (event.key.keysym.scancode == SDL_SCANCODE_F8) {
        g_pass_test_idx++;
        if (g_pass_test_idx > 4) {
          g_pass_test_idx = 0;
        }
        break;
      }
      else if (event.key.keysym.scancode == SDL_SCANCODE_F9) {
        g_anisotropy += 0.5;
        float max = _pInt->vulkan()->deviceLimits().maxSamplerAnisotropy;
        if (g_anisotropy > max) {
          g_anisotropy = 0;
        }
        _pInt->createTextureImages();
        break;
      }
      else if (event.key.keysym.scancode == SDL_SCANCODE_F10) {
        auto c = _pInt->vulkan()->maxMSAA();

        if (g_multisample == c) {
          g_multisample = MSAA::Disabled;
        }
        else {
          g_multisample = c;
        }

        break;
      }
      else if (event.key.keysym.scancode == SDL_SCANCODE_F11) {
        g_test_img1 = !g_test_img1;
        _pInt->createTextureImages();

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

    try {
      //FPS
      _pInt->_fpsMeter_Update.update();
      if (_pInt->_fpsMeter_Update.getFrameNumber() % 2 == 0) {
        float f_upd = _pInt->_fpsMeter_Update.getFps();
        string_t fp_upd = std::to_string((int)f_upd);
        float f_r = _pInt->_fpsMeter_Render.getFps();
        string_t fp_r = std::to_string((int)f_r);

        string_t mmip_mode = (g_mipmap_mode == MipmapMode::Linear) ? ("L") : ((g_mipmap_mode == MipmapMode::Nearest) ? ("N") : ((g_mipmap_mode == MipmapMode::Disabled) ? ("D") : ("Undefined-Error")));
        string_t min_f = (g_min_filter == TexFilter::Linear) ? ("L") : ((g_min_filter == TexFilter::Nearest) ? ("N") : ((g_min_filter == TexFilter::Cubic) ? ("C") : ("Undefined-Error")));
        string_t mag_f = (g_mag_filter == TexFilter::Linear) ? ("L") : ((g_mag_filter == TexFilter::Nearest) ? ("N") : ((g_mag_filter == TexFilter::Cubic) ? ("C") : ("Undefined-Error")));

        string_t fps = "FPS:(upd=" + fp_upd + "fps,rend=" + fp_r + std::string("fps") + std::string(",frame:") + std::to_string(_pInt->g_iFrameNumber) + ")";
        string_t mode = " F1=mipmap:(Tex=" + mmip_mode + ",Minf=" + min_f + ",Magf=" + mag_f + ")";
        string_t culm = " F2=cull:(" + std::to_string((int)g_cullmode) + ")";
        string_t line = " F3=Line:(" + std::to_string((int)g_poly_line) + ")";
        string_t rtt = " F4=RTT:(" + std::to_string((int)g_use_rtt) + ")";
        string_t pass = " F8=pass:(" + std::to_string(g_pass_test_idx) + ")";
        string_t aniso = " F9=AF:(" + std::to_string(g_anisotropy) + ")";
        string_t msaa = " F10=MSAA:(" + std::to_string((int)g_multisample) + ")";
        string_t img = " F11=img";

        string_t out = fps + mode + culm + line + rtt + pass + aniso + msaa + img;

        SDL_SetWindowTitle(_pInt->_pSDLWindow, out.c_str());
      }

      _pInt->drawFrame();
    }
    catch (std::runtime_error& err) {
      bool device_lost = !strcmp(err.what(), Vulkan::c_strErrDeviceLost);
      // https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#devsandqueues-lost-device
      // You must destroy everything and reinitialize it on the GPU. This would be super complex to implement.
      if (device_lost) {
        exit = true;
      }
    }
  }
}

#pragma endregion

}  // namespace VG
