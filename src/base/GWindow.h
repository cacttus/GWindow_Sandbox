/**
/**
 *  @file GVulkan.h
 *  @date 09/02/2020
 *  @author Derek Page
 *  @brief Test window for multiple Vulkan windows
 */
#pragma once
#ifndef __SDLVULKAN_160169401317456691411060057473_H__
#define __SDLVULKAN_160169401317456691411060057473_H__

#include "./GWindowHeader.h"
#include "./VulkanHeader.h"
#include "./GWorld.h"

namespace VG {

//Testing out multiple vulkan windows, single instance.
class GWindow : public VulkanObject {
public:
  GWindow(Vulkan* v);
  virtual ~GWindow() override;
  bool doInput();
  void init();
  void renderLoop();
};
class SettingsFile {
public:
  void load();
  bool _debugEnabled = false;
};
/**
 *  @class GSDL
 *  @brief Vulkan windowing main class
 */
class GSDL {
public:
  GSDL();
  virtual ~GSDL();

  bool doInput();
  void init();
  void renderLoop();
  void start();
  GWindow* createWindow();

private:
  std::vector<std::unique_ptr<GWindow>> _windows;

  void cycleValue(float& value, const std::vector<double>& values);
  bool fueq(float x, float y, float e = 0.0001);
  void cleanup();
  void cleanupShaderMemory();
  void allocateShaderMemory();
  void createTextureImages();
  void cmd_simpleCubes(RenderFrame* frame, double dt);
  void cmd_RenderToTexture(RenderFrame* frame, double dt);
  void drawFrame();
  void tryInitializeOffsets(std::vector<BR2::vec3>& offsets, std::vector<float>& rots_delta, std::vector<float>& rots_ini, std::vector<BR2::vec3>& axes_ini);
  void updateInstanceUniformBuffer(std::shared_ptr<VulkanBuffer> instanceBuffer, std::vector<BR2::vec3>& offsets, std::vector<float>& rots_delta, std::vector<float>& rots_ini, float dt, std::vector<BR2::vec3>& axes);
  void updateLights(std::shared_ptr<VulkanBuffer> lightsBuffer, float dt);
  void updateViewProjUniformBuffer(std::shared_ptr<VulkanBuffer> viewProjBuffer);
  float pingpong_t01(int durationMs = 5000);
  void createUniformBuffers();
  void sdl_PrintVideoDiagnostics();
  SDL_Window* makeSDLWindow(const GraphicsWindowCreateParameters& params, int render_system, bool show);
  void destroyDebugWindow();
  void makeDebugWindow();
  void drawDebugWindow();
  void makeDebugTexture(int w, int h);
  void handleCamera();
  void test_overlay();
  void createGameAndShaderTest();
  BR2::urect2 getWindowDims();
  std::shared_ptr<Img32> loadImage(const string_t& img);
  Vulkan* vulkan() { return _vulkan.get(); }
  
  SDL_Window* _pSDLWindow = nullptr;
  std::unique_ptr<SettingsFile> _settings;

  std::unique_ptr<Vulkan> _vulkan = nullptr;
  std::shared_ptr<TextureImage> _testTexture1 = nullptr;
  std::shared_ptr<TextureImage> _testTexture2 = nullptr;
  std::unique_ptr<PipelineShader> _pShader = nullptr;
  std::shared_ptr<GameDummy> _game = nullptr;

  string_t c_viewProjUBO = "c_viewProjUBO";
  string_t c_instanceUBO_1 = "c_instanceUBO_1";
  string_t c_instanceUBO_2 = "c_instanceUBO_2";
  string_t c_lightsUBO = "c_lightsUBO";
  string_t base_title = "Press F1 to toggle Mipmaps";

  uint32_t _numInstances = 25;
  uint32_t _numLights = 3;
  uint32_t _maxLights = 10;  // **TODO: we can automatically set this via the shader's metadata
  FpsMeter _fpsMeter_Render;
  FpsMeter _fpsMeter_Update;
  uint64_t g_iFrameNumber = 0;

  //Debug window
  SDL_Window* _pDebugWindow = nullptr;
  SDL_Surface* _pDebugWindow_Surface = nullptr;
  SDL_Renderer* _pDebugRenderer = nullptr;
  SDL_Texture* _pDebugTexture = nullptr;
  std::shared_ptr<Img32> _debugImageData = nullptr;
  SDL_Renderer* _vulkan_renderer = nullptr;
  //std::weak_ptr<RenderTexture> test_render_texture ;
  int _debugImg = 0;

  //Input
  float campos_d = 10;
  BR2::vec3 campos = { campos_d, campos_d, campos_d };
  bool mouse_down = false;
  BR2::vec2 last_mouse_pos{ 0, 0 };
  float mouse_wheel = 0;
  bool _initial_cam_rot_set = false;
  float theta = 0;
  float phi = 0;
  float min_radius = 2;

  //Temps & shader
  std::vector<GPULight> lights;
  std::vector<float> lights_speed;
  std::vector<float> lights_r;
  std::mt19937 _rnd_engine;
  std::uniform_real_distribution<double> _rnd_distribution;  //0,1
  std::vector<BR2::vec3> offsets1;
  std::vector<BR2::vec3> offsets2;
  std::vector<float> rots_delta1;
  std::vector<float> rots_delta2;
  std::vector<float> rots_ini1;
  std::vector<float> rots_ini2;
  std::vector<BR2::vec3> axes1;
  std::vector<BR2::vec3> axes2;
};

}  // namespace VG

#endif
