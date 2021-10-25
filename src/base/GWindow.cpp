#include "./GWindow.h"

namespace VG {
#define fr01() ((float)_rnd_distribution(_rnd_engine))
#define rnd(a, b) ((a) + ((b) - (a)) * (fr01()))
#define rr ((float)rnd(-3, 3))

//**Testing
static MipmapMode g_mipmap_mode = MipmapMode::Disabled;
static TexFilter g_min_filter = TexFilter::Linear;
static TexFilter g_mag_filter = TexFilter::Linear;
static bool g_poly_line = false;
static bool g_use_rtt = true;
static int g_pass_test_idx = 4;
static float g_anisotropy = 1;
static MSAA g_multisample = MSAA::Disabled;
bool g_test_img1 = true;
VkCullModeFlags g_cullmode = VK_CULL_MODE_BACK_BIT;
bool g_lighting = true;
float g_spec_hard = 20;      //exponent
float g_spec_intensity = 1;  //mix value
const bool g_wait_fences = false;
const bool g_vsync_enable = false;

#pragma region GWindow

GWindow::GWindow(Vulkan* v) : VulkanObject(v) {
}
GWindow::~GWindow() {
}

#pragma endregion GWindow

#pragma region GSDL

GSDL::GSDL() {
}
GSDL::~GSDL() {
  cleanup();
}
void GSDL::start() {
}
std::unique_ptr<GWindow> GSDL::createWindow() {
  std::unique_ptr<GWindow> win = nullptr;

  return nullptr;
}

void GSDL::makeDebugTexture(int w, int h) {
  if (_pDebugTexture != nullptr) {
    SDL_DestroyTexture(_pDebugTexture);
    _pDebugTexture = nullptr;
  }
  _pDebugTexture = SDL_CreateTexture(_pDebugRenderer, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STREAMING, w, h);
  SDLUtils::checkSDLErr();
}
void GSDL::makeDebugWindow() {
  destroyDebugWindow();
  _pDebugWindow = SDL_CreateWindow("Debug", 700, 100, 500, 500, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
  SDLUtils::checkSDLErr();

  int flags = 0;
#if defined(BR2_OS_LINUX)
//flags = SDL_RENDERER_ACCELERATED;
#elif defined(BR2_OS_WINDOWS)
#endif

  //SDL Doc  You may not combine this with 3D or the rendering API on this window.
  // But what if we make a new window? it shouldnot mess with up new window

  _pDebugRenderer = SDL_CreateRenderer(_pDebugWindow, -1, flags);
  SDLUtils::checkSDLErr();

  _pDebugWindow_Surface = SDL_GetWindowSurface(_pDebugWindow);
  SDLUtils::checkSDLErr();
}
void GSDL::drawDebugWindow() {
  if (_debugImageData != nullptr) {
    void* img_pixels = _debugImageData->_data;
    int img_w = _debugImageData->_size.width;
    int img_h = _debugImageData->_size.height;
    int img_BPP = 4;
    int img_pitch = img_BPP * _debugImageData->_size.width;

    SDL_Rect trect;
    trect.x = 0;
    trect.y = 0;
    trect.w = img_w;
    trect.h = img_h;

    //create texture if  not created
    bool make_tex = false;
    //Check texture size is correct
    if (_pDebugTexture != nullptr) {
      int tex_access, tex_w, tex_h;
      unsigned int tex_fmt;
      SDL_QueryTexture(_pDebugTexture, &tex_fmt, &tex_access, &tex_w, &tex_h);
      if (tex_w != img_w || tex_h != img_h) {
        make_tex = true;
      }
    }
    else {
      make_tex = true;
    }
    if (make_tex) {
      makeDebugTexture(img_w, img_h);
    }

    int tex_pitch = 0;
    void* tex_px = 0;
    if (!SDL_LockTexture(_pDebugTexture, &trect, &tex_px, &tex_pitch)) {
      SDLUtils::checkSDLErr();
      if (tex_pitch != img_pitch) {
        //Error - texture size difference
        SDL_UnlockTexture(_pDebugTexture);
        BRLogError("Debug window pitch (size or BPP) mismatch:tex=" + std::to_string(tex_pitch) + ", img=" + std::to_string(img_pitch));
        SDLUtils::checkSDLErr();
        Gu::debugBreak();
        return;
      }
      SDLUtils::checkSDLErr();
      memcpy(tex_px, img_pixels, (size_t)img_w * (size_t)img_h * (size_t)img_BPP);  //  * pitch .. testing
      SDL_UnlockTexture(_pDebugTexture);
    }
    SDLUtils::checkSDLErr();

    int dw, dh;
    SDL_GetWindowSize(_pDebugWindow, &dw, &dh);
    SDLUtils::checkSDLErr();

    SDL_Rect wrect;
    wrect.x = 0;   //Extreme left of the window
    wrect.y = 0;   //Very bottom of the window
    wrect.w = dw;  //100 pixels width
    wrect.h = dh;  //100 pixels height

    //Copying the texture on to the window using renderer and rectangle
    SDL_RenderCopy(_pDebugRenderer, _pDebugTexture, &trect, &wrect);
    SDLUtils::checkSDLErr();
  }

  SDL_RenderPresent(_pDebugRenderer);
  SDLUtils::checkSDLErr();

  //Reset image dtaa for next copy
  _debugImageData = nullptr;
}
void GSDL::destroyDebugWindow() {
  if (_pDebugTexture != nullptr) {
    SDL_DestroyTexture(_pDebugTexture);
    _pDebugTexture = nullptr;
  }
  if (_pDebugRenderer != nullptr) {
    SDL_DestroyRenderer(_pDebugRenderer);
    _pDebugRenderer = nullptr;
  }
  if (_pDebugWindow != nullptr) {
    SDL_DestroyWindow(_pDebugWindow);
    _pDebugWindow = nullptr;
  }
  _debugImageData = nullptr;
  _debugImg = 0;
}
void GSDL::test_overlay() {
  //Mixing SDL with Vulkan - no but.. yes.
  if (_vulkan_renderer == nullptr) {
    _vulkan_renderer = SDL_CreateRenderer(_pSDLWindow, -1, SDL_RENDERER_ACCELERATED);
    SDLUtils::checkSDLErr();
  }
  SDL_SetRenderDrawColor(_vulkan_renderer, 255, 255, 0, 255);
  // auto surface = SDL_GetWindowSurface(_pSDLWindow);
  SDLUtils::checkSDLErr();
  SDL_Rect button = {
    .x = 10,
    .y = 10,
    .w = 100,
    .h = 30
  };
  //SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 255, 0, 0));
  SDL_SetRenderDrawColor(_vulkan_renderer, 255, 255, 255, 255);
  SDL_RenderFillRect(_vulkan_renderer, &button);
  SDL_RenderPresent(_vulkan_renderer);

  SDLUtils::checkSDLErr();
}
SDL_Window* GSDL::makeSDLWindow(const GraphicsWindowCreateParameters& params, int render_system, bool show) {
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
void SettingsFile::load() {
  //load file
  std::fstream f;
  f.open("./settings.dat", std::ios::in | std::ios::binary);
  if (!f.good()) {
    BRLogError("Settings file could not be found.");
    return;
  }
  f.seekg(0, std::ios::end);
  auto len = f.tellg();
  f.seekg(0, std::ios::beg);
  std::unique_ptr<char[]> buf(new char[len]);
  f.read(buf.get(), len);
  f.close();

  //tokenize file .. ANSI locale .. lazy today
  std::vector<std::vector<std::string>> kvps;
  {
    std::vector<std::string> cur_kvp;
    uint32_t line = 1;
    std::string tok = "";
    for (auto i = 0; i < len; i++, line++) {
      char c = buf[i];
      if (c == ' ' || c == '=' || c == '\n') {
        cur_kvp.push_back(tok);
        tok = "";
        if (c == '\n') {
          if (cur_kvp.size() == 2) {
            kvps.push_back(cur_kvp);
          }
          else {
            BRLogError("Invalid key-value-pair count, must be 2");
            Gu::debugBreak();
          }
          cur_kvp.clear();
        }
      }
      else if (c == '\r') {
      }
      else if (isalnum(c) || c == '.' || c == '_' || c == '-') {
        tok += c;
      }
      else {
        BRLogError("Unrecognized token " + std::string(c, 1) + " in settings file on line " + std::to_string(line));
        Gu::debugBreak();
      }
    }
  }
  std::function<bool(const std::string& a, const std::string& b)> scmpi = [](const std::string& a, const std::string& b) {
    return std::equal(a.begin(), a.end(),
                      b.begin(), b.end(),
                      [](char a, char b) {
                        return tolower(a) == tolower(b);
                      });
  };
  std::function<bool(const std::string&, bool)> readBool = [scmpi](const std::string& a, bool default_value) {
    if (scmpi(a, "true")) {
      return true;
    }
    else if (scmpi(a, "false")) {
      return false;
    }
    else if (scmpi(a, "1")) {
      return true;
    }
    else if (scmpi(a, "0")) {
      return false;
    }
    else if (scmpi(a, "t")) {
      return true;
    }
    else if (scmpi(a, "f")) {
      return false;
    }
    else {
    }
    return default_value;
  };
  //parse tokens
  {
    for (auto& kvp : kvps) {
      if (kvp.size() == 2) {
        if (scmpi(kvp[0], "enable_debug")) {
          _debugEnabled = readBool(kvp[1], false);
          BRLogDebug("enable_debug = " + std::to_string(_debugEnabled));
        }
        else {
          BRLogError("Unrecognized settings token " + kvp[0]);
          Gu::debugBreak();
        }
      }
    }
  }
}
void GSDL::init() {
  try {
    _settings = std::make_unique<SettingsFile>();
    _settings->load();

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

    _vulkan = Vulkan::create(title, _pSDLWindow, g_wait_fences, g_vsync_enable, _settings->_debugEnabled);

    createGameAndShaderTest();

    BRLogInfo("Showing window..");
    SDL_ShowWindow(_pSDLWindow);
  }
  catch (std::exception& ex) {
    std::cout << ex.what() << std::endl;
    cleanup();
    throw ex;
  }
}
void GSDL::createGameAndShaderTest() {
  _game = nullptr;
  _pShader = nullptr;
  _game = std::make_shared<GameDummy>();
  _game->_mesh1 = std::make_shared<Mesh>(_vulkan.get());
  _game->_mesh1->makeBox();
  _game->_mesh2 = std::make_shared<Mesh>(_vulkan.get());
  _game->_mesh2->makeBox();

  //Make Shader.
  _pShader = PipelineShader::create(_vulkan.get(), "Vulkan-Tutorial-Test-Shader",
                                    std::vector{ App::rootFile("test_vs.spv"), App::rootFile("test_fs.spv") });
  allocateShaderMemory();
}
void GSDL::sdl_PrintVideoDiagnostics() {
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
BR2::urect2 GSDL::getWindowDims() {
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
void GSDL::createUniformBuffers() {
  _pShader->createUBO(c_viewProjUBO, "_uboViewProj", sizeof(ViewProjUBOData), 1);
  _pShader->createUBO(c_instanceUBO_1, "_uboInstanceData", sizeof(InstanceUBOData), _numInstances);
  _pShader->createUBO(c_instanceUBO_2, "_uboInstanceData", sizeof(InstanceUBOData), _numInstances);
  _pShader->createUBO(c_lightsUBO, "_uboLights", sizeof(GPULight), _maxLights);
}
float GSDL::pingpong_t01(int durationMs) {
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
void GSDL::tryInitializeOffsets(std::vector<BR2::vec3>& offsets, std::vector<float>& rots_delta, std::vector<float>& rots_ini, std::vector<BR2::vec3>& axes_ini) {
  if (offsets.size() == 0) {
    for (size_t i = 0; i < _numInstances; ++i) {
      offsets.push_back({ rr, rr, rr });
      rots_delta.push_back((float)rnd(-M_2PI, M_2PI));                    //rotation delta.
      rots_ini.push_back((float)rnd(-M_2PI, M_2PI));                      // Initial rotation, and also the value of current rotation.
      axes_ini.push_back(BR2::vec3(rnd(-1, 1), rnd(-1, 1), rnd(-1, 1)));  // Initial rotation, and also the value of current rotation.
      axes_ini[axes_ini.size() - 1].normalize();
    }
  }
}
void GSDL::updateViewProjUniformBuffer(std::shared_ptr<VulkanBuffer> viewProjBuffer) {
  //Push constants are faster.
  float t01 = pingpong_t01(10000);

  BR2::vec3 lookAt = { 0, 0, 0 };
  BR2::vec3 wwf = (lookAt - campos);
  BR2::vec3 trans = campos + wwf + (lookAt - campos - wwf) * t01;
  ViewProjUBOData ub = {
    .view = BR2::mat4::getLookAt(campos, lookAt, BR2::vec3(0.0f, 1.0f, 0.0f)),
    .proj = BR2::mat4::projection((float)BR2::MathUtils::radians(45.0f), (float)_vulkan->swapchain()->windowSize().width, -(float)_vulkan->swapchain()->windowSize().height, 0.1f, 100.0f),
    .camPos = campos
  };
  viewProjBuffer->writeData((void*)&ub, 1);
}
void GSDL::updateLights(std::shared_ptr<VulkanBuffer> lightsBuffer, float dt) {
  //Must be lessthan or equal the shader array
  if (lights.size() == 0) {
    for (size_t i = 0; i < _numLights; ++i) {
      lights.push_back(GPULight());
      lights[lights.size() - 1].pos = BR2::vec3(0, 0, 0);
      // lights[lights.size() - 1].color = BR2::vec3(std::min(.3 + fr01(), 1.), std::min(.3 + fr01(), 1.), std::min(.3 + fr01(), 1.));
      if (i == 0) { lights[lights.size() - 1].color.construct(1, 0, 0); }
      if (i == 1) { lights[lights.size() - 1].color.construct(0, 1, 0); }
      if (i == 2) { lights[lights.size() - 1].color.construct(0, 0, 1); }
      //= BR2::vec3(std::min(.3 + fr01(), 1.), std::min(.3 + fr01(), 1.), std::min(.3 + fr01(), 1.));
      lights[lights.size() - 1].radius = 20 + fr01() * 10;
      lights[lights.size() - 1].rotation = fr01() * (float)M_2PI;
      lights[lights.size() - 1].specColor = BR2::vec3(1, 1, 1);
      lights[lights.size() - 1].specHardness = 1.0f;
      lights[lights.size() - 1].specIntensity = 1.0f;
      lights_speed.push_back(2 + fr01() * 8);
      lights_r.push_back(2 + fr01() * 10);
    }
    //Disable the rest
    for (size_t i = _numLights; i < _maxLights; ++i) {
      lights.push_back(GPULight());
      lights[lights.size() - 1].radius = 0;  //disable
      lights_speed.push_back(1);
      lights_r.push_back(1);
    }
  }

  for (size_t ilight = 0; ilight < lights.size(); ++ilight) {
    auto& light = lights[ilight];
    if (light.radius > 0) {
      light.rotation = fmodf(light.rotation + 6.28f * (dt / lights_speed[ilight]), 6.28f);
      light.pos = BR2::vec3(cosf(light.rotation) * lights_r[ilight], 4, sinf(light.rotation) * lights_r[ilight]);
      light.specHardness = g_spec_hard;
      light.specIntensity = g_spec_intensity;
    }
  }
  auto s = sizeof(lights[0]);
  auto sz = sizeof(lights[0]) * lights.size();
  lightsBuffer->writeData(lights.data(), lights.size());
}
void GSDL::updateInstanceUniformBuffer(std::shared_ptr<VulkanBuffer> instanceBuffer, std::vector<BR2::vec3>& offsets, std::vector<float>& rots_delta, std::vector<float>& rots_ini, float dt, std::vector<BR2::vec3>& axes) {
  float t01 = pingpong_t01(10000);
  float t02 = pingpong_t01(10000);
  tryInitializeOffsets(offsets, rots_delta, rots_ini, axes);

  //This slowdown (e.g. 2500fps -> 80fps) is my code mat4 multiplies.
  // We could dispatch the "instance update" and also any additional physics into a compute shader to get the result more quickily.
  // Will we really have 1000's of dynamic instances updating per frame? Probably not, but it's fun to think about.
  BR2::vec3 lookAt = { 0, 0, 0 };
  BR2::vec3 wwf = (lookAt - campos) * 0.1f;
  BR2::vec3 trans(0, 0, 0);                 // campos + wwf + (lookAt - campos - wwf) * t01;
  BR2::vec3 origin = { -0.5, -0.5, -0.5 };  //cube origin
  std::vector<BR2::mat4> mats(_numInstances);
  for (uint32_t i = 0; i < _numInstances; ++i) {
    if (i < offsets.size()) {
      rots_ini[i] += rots_delta[i] * dt;
      mats[i] = BR2::mat4::translation(origin) *
                BR2::mat4::rotation(rots_ini[i], axes[i]) *
                BR2::mat4::translation(trans + offsets[i]);
    }
  }
  instanceBuffer->writeData(mats.data(), mats.size());
  // ub.proj._m22 *= -1;
}
void GSDL::drawFrame() {
  AssertOrThrow2(_vulkan);
  AssertOrThrow2(_vulkan->swapchain());
  if (_vulkan->swapchain()->beginFrame(getWindowDims().size)) {
    static auto last_time = std::chrono::high_resolution_clock::now();
    double t01 = std::chrono::duration<double, std::chrono::seconds::period>(std::chrono::high_resolution_clock::now() - last_time).count();
    last_time = std::chrono::high_resolution_clock::now();

    RenderFrame* frame = _vulkan->swapchain()->currentFrame();
    if (frame != nullptr) {
      recordCommandBuffer(frame, t01);
    }
    _vulkan->swapchain()->endFrame();
    _fpsMeter_Render.update();
    g_iFrameNumber++;
  }
}
void GSDL::recordCommandBuffer(RenderFrame* frame, double dt) {
  uint32_t frameIndex = frame->frameIndex();

  auto viewProj = _pShader->getUBO(c_viewProjUBO, frame);
  auto inst1 = _pShader->getUBO(c_instanceUBO_1, frame);
  auto inst2 = _pShader->getUBO(c_instanceUBO_2, frame);
  auto lightsubo = _pShader->getUBO(c_lightsUBO, frame);
  updateViewProjUniformBuffer(viewProj);
  //  if (_fpsMeter_Update.frameMod(50)) {
  updateInstanceUniformBuffer(inst1, offsets1, rots_delta1, rots_ini1, (float)dt, axes1);
  updateInstanceUniformBuffer(inst2, offsets2, rots_delta2, rots_ini2, (float)dt, axes2);
  updateLights(lightsubo, (float)dt);
  // }

  //if (test_render_texture.lock() == nullptr) {
  auto test_render_texture = vulkan()->swapchain()->getRenderTexture("Test_RenderTExture", vulkan()->swapchain()->imageFormat(), g_multisample,
                                                                     FilterData{ SamplerType::Sampled, MipmapMode::Disabled, vulkan()->maxAF(),
                                                                                 TexFilter::Linear, TexFilter::Linear, MipLevels::Unset });
  //}

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

    // Tests
    // 0 Test begin/end without draw commands
    // 3. 2 passes with different geometry & textures.
    // 2. 2nd pass same fbo without clearing (rnder to same fbo)
    // 3+RTT. Rendering to texture, then using texture on mesh.
    // 4 simple pass

    //if x=0 test a simple pass
    //otherwise test the complex pass.
    if (g_pass_test_idx == 4) {
      auto simple_pass = _pShader->getPass(frame, g_multisample, BlendFunc::AlphaBlend, FramebufferBlendMode::Independent);
      simple_pass->setOutput(OutputDescription::getColorDF());
      simple_pass->setOutput(OutputDescription::getDepthDF());
      if (_pShader->beginRenderPass(cmd, std::move(simple_pass))) {
        if (_pShader->bindPipeline(cmd, nullptr, mode, topo, g_cullmode)) {
          _pShader->bindViewport(cmd, { { 0, 0 }, _vulkan->swapchain()->windowSize() });
          _pShader->bindUBO("_uboViewProj", viewProj);

          _pShader->bindSampler("_ufTexture0", _testTexture1);
          _pShader->bindUBO("_uboInstanceData", inst1);
          _pShader->bindUBO("_uboLights", lightsubo);
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
          pass1->setOutput("test_render_texture", OutputMRT::RT_DefaultColor, test_render_texture, BlendFunc::AlphaBlend, true, c_r, c_g, c_b);
        }
        else {
          //Todo: Remove colorDF and DepthDF and just use the pass to create the description.
          pass1->setOutput(OutputDescription::getColorDF(nullptr, true, c_r, c_g, c_b));
        }
        pass1->setOutput(OutputDescription::getDepthDF(true));

        if (_pShader->beginRenderPass(cmd, std::move(pass1))) {
          if (_pShader->bindPipeline(cmd, nullptr, mode, topo, g_cullmode)) {
            pass1_success = true;
            if (g_pass_test_idx != 0) {
              _pShader->bindViewport(cmd, { { 0, 0 }, _vulkan->swapchain()->windowSize() });
              _pShader->bindUBO("_uboViewProj", viewProj);

              //white face Smiley
              _pShader->bindSampler("_ufTexture0", _testTexture1);
              _pShader->bindUBO("_uboInstanceData", inst1);
              _pShader->bindUBO("_uboLights", lightsubo);
              _pShader->bindDescriptors(cmd);
              _pShader->drawIndexed(cmd, _game->_mesh1, _numInstances);  //Changed from pipe::drawIndexed
            }
          }

          _pShader->endRenderPass(cmd);
        }
      }

      if (g_pass_test_idx == 2 || g_pass_test_idx == 3) {
        //black Smiley
        auto tex = g_use_rtt ? test_render_texture->texture(MSAA::Disabled, frame->frameIndex()) : _testTexture2;

        auto pass2 = _pShader->getPass(frame, g_multisample, BlendFunc::AlphaBlend, FramebufferBlendMode::Independent);

        if (pass1_success) {
          //3 && rtt - clear - we're going to draw the texture from last frame - testing rendertextures
          //3 && !rtt - don't clear - we're going to attempt a second pass on the same data.
          bool clear = g_pass_test_idx == 2 || (g_pass_test_idx == 3 && g_use_rtt);
          pass2->setOutput(OutputDescription::getColorDF(nullptr, clear));
          pass2->setOutput(OutputDescription::getDepthDF(clear));
        }
        else {
          pass2->setOutput(OutputDescription::getColorDF());
          pass2->setOutput(OutputDescription::getDepthDF());
        }

        if (_pShader->beginRenderPass(cmd, std::move(pass2))) {
          if (_pShader->bindPipeline(cmd, nullptr, mode, topo, g_cullmode)) {
            _pShader->bindViewport(cmd, { { 0, 0 }, _vulkan->swapchain()->windowSize() });
            _pShader->bindUBO("_uboViewProj", viewProj);

            //black face smiley
            _pShader->bindSampler("_ufTexture0", pass1_success ? tex : _testTexture2);
            _pShader->bindUBO("_uboInstanceData", inst2);
            _pShader->bindUBO("_uboLights", lightsubo);
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
std::shared_ptr<Img32> GSDL::loadImage(const string_t& img) {
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
void GSDL::createTextureImages() {
  // auto img = loadImage(App::rootFile("test.png"));
  auto img = loadImage(App::rootFile(g_test_img1 ? "grass.png" : "TexturesCom_MetalBare0253_2_M.png"));
  if (img) {
    _testTexture1 = std::make_shared<TextureImage>(vulkan(), img->_name, TextureType::ColorTexture, MSAA::Disabled, img,
                                                   FilterData{ SamplerType::Sampled, g_mipmap_mode, g_anisotropy, g_min_filter,
                                                               g_mag_filter, MipLevels::Unset });
  }
  else {
    vulkan()->errorExit("Could not load test image 1.");
  }
  auto img2 = loadImage(App::rootFile("dirt.png"));
  if (img2) {
    _testTexture2 = std::make_shared<TextureImage>(vulkan(), img->_name, TextureType::ColorTexture, MSAA::Disabled, img2,
                                                   FilterData{ SamplerType::Sampled, g_mipmap_mode, g_anisotropy, g_min_filter,
                                                               g_mag_filter, MipLevels::Unset });
  }
  else {
    vulkan()->errorExit("Could not load test image 2.");
  }
}
void GSDL::allocateShaderMemory() {
  cleanupShaderMemory();

  createUniformBuffers();
  createTextureImages();
}
void GSDL::cleanupShaderMemory() {
  _testTexture1 = nullptr;
  _testTexture2 = nullptr;
}
void GSDL::cleanup() {
  destroyDebugWindow();

  //This stops all threads before we cleanup.
  CheckVKR(vkDeviceWaitIdle, vulkan()->device());

  // All child objects created using instance must have been destroyed prior to destroying instance - Vulkan Spec.
  cleanupShaderMemory();
  _pShader = nullptr;

  //TODO: all managers must be destroyed before we destroy Vulkan.
  //All buffers, and everything that uses Vulkan Instance.
  _testTexture1 = nullptr;
  _testTexture2 = nullptr;
  _pShader = nullptr;
  _game = nullptr;

  _vulkan = nullptr;

  SDL_DestroyWindow(_pSDLWindow);
}
bool GSDL::fueq(float x, float y, float e) {
  return (x - e) <= y && (x + e) >= y;
}
void GSDL::cycleValue(float& value, const std::vector<double>& values) {
  if (fueq((float)value, (float)values[values.size() - 1])) {
    value = (float)values[0];
  }
  else {
    for (size_t i = 0; i < values.size() - 1; ++i) {
      if (fueq(value, (float)values[i])) {
        value = (float)values[i + 1];
        break;
      }
    }
  }
}
void GSDL::handleCamera() {
  mouse_wheel = 0;
  //Rotate
  int dx, dy;
  SDL_GetMouseState(&dx, &dy);
  BR2::vec2 p{ (float)dx, (float)dy };
  if (mouse_down) {
    BR2::vec2 delta = p - last_mouse_pos;
    BR2::urect2 r = getWindowDims();
    float x = -delta.x / 300;
    float y = -delta.y / 300;
    if (x != 0 || y != 0) {
      float delta_rot_x = (float)(M_2PI * (double)x);
      float delta_rot_y = (float)(M_PI * (double)y);
      float zxradius = sqrt(campos.x * campos.x + campos.y * campos.y + campos.z * campos.z);

      if (_initial_cam_rot_set == false) {
        if (zxradius == 0) {
          zxradius = min_radius;
        }
        phi = acos(campos.y / zxradius);
        theta = acos((campos.z / zxradius) / sin(phi));
        _initial_cam_rot_set = true;
      }

      theta += delta_rot_x;
      theta = (float)fmod(theta, M_2PI);

      phi += delta_rot_y;
      if (phi < 0.001f) {
        phi = 0.001f;
      }
      if (phi >= (float)M_PI - 0.001f) {
        phi = (float)M_PI - 0.001f;
      }

      campos.x = sin(phi) * cos(theta) * zxradius;
      campos.z = sin(phi) * sin(theta) * zxradius;
      campos.y = cos(phi) * zxradius;
    }
  }
  last_mouse_pos = p;
}
bool GSDL::doInput() {
  SDL_Event event;

  handleCamera();

  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      return true;
      break;
    }
    else if (event.type == SDL_WINDOWEVENT) {
      if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
        _vulkan->swapchain()->outOfDate();
      }
      if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
        if (_pDebugWindow != nullptr && SDL_GetWindowID(_pDebugWindow) == event.window.windowID) {
          destroyDebugWindow();
        }
        else {
          return true;
        }
      }
    }
    else if (event.type == SDL_MOUSEMOTION) {
    }
    else if (event.type == SDL_MOUSEWHEEL) {
      if (event.wheel.y != 0) {
        mouse_wheel = (float)std::min(10, std::max(-10, event.wheel.y));

        auto n = campos.normalized() * -1.;
        if (campos.length() + (n * mouse_wheel).length() > 2) {
          campos += n * mouse_wheel;
        }
        if (campos.length() < min_radius) {
          campos = n * -min_radius;
        }
      }
    }
    else if (event.type == SDL_MOUSEBUTTONDOWN) {
      mouse_down = true;
    }
    else if (event.type == SDL_MOUSEBUTTONUP) {
      mouse_down = false;
    }
    else if (event.type == SDL_KEYDOWN) {
      if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
        return true;
      }
      else if (event.key.keysym.scancode == SDL_SCANCODE_1) {
        //g_mipmap_mode = (MipmapMode)(((int)g_mipmap_mode + 1) % ((int)MipmapMode::MipmapMode_Count));
        //TextureImage::testCycleFilters(g_min_filter, g_mag_filter, g_mipmap_mode);
        if (g_mipmap_mode == MipmapMode::Disabled) {
          g_mipmap_mode = MipmapMode::Nearest;
        }
        else if (g_mipmap_mode == MipmapMode::Nearest) {
          g_mipmap_mode = MipmapMode::Linear;
        }
        else if (g_mipmap_mode == MipmapMode::Linear) {
          g_mipmap_mode = MipmapMode::Disabled;
        }
        createTextureImages();
      }
      else if (event.key.keysym.scancode == SDL_SCANCODE_2) {
        //g_mipmap_mode = (MipmapMode)(((int)g_mipmap_mode + 1) % ((int)MipmapMode::MipmapMode_Count));
        //TextureImage::testCycleFilters(g_min_filter, g_mag_filter, g_mipmap_mode);
        if (g_min_filter == TexFilter::Nearest) {
          g_min_filter = TexFilter::Linear;
        }
        else if (g_min_filter == TexFilter::Linear) {
          g_min_filter = TexFilter::Cubic;
        }
        else if (g_min_filter == TexFilter::Cubic) {
          g_min_filter = TexFilter::Nearest;
        }
        createTextureImages();
      }
      else if (event.key.keysym.scancode == SDL_SCANCODE_3) {
        //g_mipmap_mode = (MipmapMode)(((int)g_mipmap_mode + 1) % ((int)MipmapMode::MipmapMode_Count));
        //TextureImage::testCycleFilters(g_min_filter, g_mag_filter, g_mipmap_mode);
        if (g_mag_filter == TexFilter::Nearest) {
          g_mag_filter = TexFilter::Linear;
        }
        else if (g_mag_filter == TexFilter::Linear) {
          g_mag_filter = TexFilter::Cubic;
        }
        else if (g_mag_filter == TexFilter::Cubic) {
          g_mag_filter = TexFilter::Nearest;
        }
        createTextureImages();
      }
      else if (event.key.keysym.scancode == SDL_SCANCODE_4) {
        cycleValue(g_spec_hard, { 0.0, 0.05, 0.5, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0, 200, 400, 800, 1600, 3200 });
      }
      else if (event.key.keysym.scancode == SDL_SCANCODE_5) {
        cycleValue(g_spec_intensity, { 0.005, 0.05, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 5.0 });
      }
      else if (event.key.keysym.scancode == SDL_SCANCODE_8) {
        _game->_mesh1->recopyData();
        _game->_mesh2->recopyData();
      }
      else if (event.key.keysym.scancode == SDL_SCANCODE_9) {
        if (_pDebugWindow == nullptr) {
          makeDebugWindow();
        }
        else {
          _debugImg++;
        }
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
      }
      else if (event.key.keysym.scancode == SDL_SCANCODE_F3) {
        g_poly_line = !g_poly_line;
      }
      else if (event.key.keysym.scancode == SDL_SCANCODE_F4) {
        g_use_rtt = !g_use_rtt;
      }
      else if (event.key.keysym.scancode == SDL_SCANCODE_F8) {
        g_pass_test_idx++;
        if (g_pass_test_idx > 4) {
          g_pass_test_idx = 0;
        }
      }
      else if (event.key.keysym.scancode == SDL_SCANCODE_F9) {
        g_anisotropy += 0.5;
        float max = vulkan()->deviceLimits().maxSamplerAnisotropy;
        if (g_anisotropy > max) {
          g_anisotropy = 0;
        }
        createTextureImages();
      }
      else if (event.key.keysym.scancode == SDL_SCANCODE_F10) {
        auto c = vulkan()->maxMSAA();

        if (g_multisample == c) {
          g_multisample = MSAA::Disabled;
        }
        else {
          g_multisample = c;
        }
      }
      else if (event.key.keysym.scancode == SDL_SCANCODE_F11) {
        g_test_img1 = !g_test_img1;
        createTextureImages();
      }
    }
  }
  return false;
}
void GSDL::renderLoop() {
  bool exit = false;
  while (!exit) {
    exit = doInput();

    try {
      //FPS
      _fpsMeter_Update.update();
      if (_fpsMeter_Update.getFrameNumber() % 2 == 0) {
        float f_upd = _fpsMeter_Update.getFps();
        float f_upd_a = _fpsMeter_Update.getFpsAvg();
        string_t fp_upd = Stz std::to_string((int)f_upd) + "/" + std::to_string((int)f_upd_a);
        float f_r = _fpsMeter_Render.getFps();
        float f_r_a = _fpsMeter_Render.getFpsAvg();
        string_t fp_r = std::to_string((int)f_r) + "/" + std::to_string((int)f_r_a);

        string_t zmmip_mode = (g_mipmap_mode == MipmapMode::Linear) ? ("L") : ((g_mipmap_mode == MipmapMode::Nearest) ? ("N") : ((g_mipmap_mode == MipmapMode::Disabled) ? ("D") : ("Error")));
        string_t zmin_f = (g_min_filter == TexFilter::Linear) ? ("L") : ((g_min_filter == TexFilter::Nearest) ? ("N") : ((g_min_filter == TexFilter::Cubic) ? ("C") : ("Error")));
        string_t zmag_f = (g_mag_filter == TexFilter::Linear) ? ("L") : ((g_mag_filter == TexFilter::Nearest) ? ("N") : ((g_mag_filter == TexFilter::Cubic) ? ("C") : ("Error")));

        string_t fps = "FPS(update=" + fp_upd + "fps,render=" + fp_r + std::string("fps") + std::string(",frame:") + std::to_string(g_iFrameNumber) + ")";
        string_t mip_f = " 1=TMip(" + zmmip_mode + ")";
        string_t min_f = " 2=TMinf(" + zmin_f + ")";
        string_t mag_f = " 3=TMagf(" + zmag_f + ")";
        string_t specg = " 4=specH(" + std::to_string(g_spec_hard) + ")";
        string_t speci = " 5=specI(" + std::to_string(g_spec_intensity) + ")";
        string_t savimg = " 9=shdbg";
        string_t culm = " F2=Cull(" + std::to_string((int)g_cullmode) + ")";
        string_t line = " F3=Line(" + std::to_string((int)g_poly_line) + ")";
        string_t rtt = " F4=RTT(" + std::to_string((int)g_use_rtt) + ")";
        string_t pass = " F8=pass(" + std::to_string(g_pass_test_idx) + ")";
        string_t aniso = " F9=AF(" + std::to_string(g_anisotropy) + ")";
        string_t msaa = " F10=MSAA(x" + std::to_string((int)TextureImage::msaa_to_int(g_multisample)) + ")";
        string_t img = " F11=chimg";

        string_t out = fps + mip_f + min_f + mag_f + specg + speci + savimg + culm + line + rtt + pass + aniso + msaa + img;

        SDL_SetWindowTitle(_pSDLWindow, out.c_str());
      }

      drawFrame();

      //  test_overlay(); silly

      if (_pDebugWindow) {
        int modd = (int)(std::max(_fpsMeter_Render.getFpsAvg() * 0.25, 1.0));
        if (_fpsMeter_Render.frameMod(modd)) {
          _debugImageData = vulkan()->swapchain()->grabImage(_debugImg);
          //If this returns nullptr, reset the image index.
          if (_debugImageData == nullptr) {
            _debugImg = 0;
          }
          drawDebugWindow();
        }
      }
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

#pragma endregion GSDL

}  // namespace VG
