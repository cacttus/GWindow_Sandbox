/**
*  @file Vulkan.h
*  @date 09/15/2020
*  @author MetalMario971
*/
#pragma once
#ifndef __VULKAN_16027956112915737602086235753_H__
#define __VULKAN_16027956112915737602086235753_H__

#include "./VulkanHeader.h"

namespace VG {
/**
 * @class Vulkan 
 * @brief Root class for the vulkan device api.
 */
class Vulkan_Internal;
class Vulkan : public SharedObject<Vulkan> {
public:
  struct QueueFamilies {
    std::optional<uint32_t> _graphicsFamily;
    std::optional<uint32_t> _computeFamily;
    std::optional<uint32_t> _presentFamily;
  };

public:
  static constexpr const char* c_strErrDeviceLost = "VK_ERROR_DEVICE_LOST"; //TODO; move this somewhere else.
  Vulkan();
  virtual ~Vulkan();
    
  static std::shared_ptr<Vulkan> create(const string_t& title, SDL_Window* win, bool vsync_enabled, bool wait_fences, bool enableDebug);

  const VkSurfaceKHR& windowSurface() { return _windowSurface; }
  const VkPhysicalDevice& physicalDevice() { return _physicalDevice; }
  const VkDevice& device() { return _device; }
  const VkInstance& instance() { return _instance; }
  const VkCommandPool& commandPool() { return _commandPool; }
  const VkQueue& graphicsQueue() { return _graphicsQueue; }
  const VkQueue& presentQueue() { return _presentQueue; }
  bool vsyncEnabled() { return _vsync_enabled; }
  bool waitFences() { return _wait_fences; }
  const VkSurfaceCapabilitiesKHR& surfaceCaps();
  const VkPhysicalDeviceProperties& deviceProperties();
  const VkPhysicalDeviceLimits& deviceLimits();
  const VkPhysicalDeviceFeatures& deviceFeatures();
  uint32_t swapchainImageCount();
  MSAA maxMSAA();
  float maxAF();

  void waitIdle();
  void checkErrors();
  void validateVkResult(VkResult res, const string_t& fname);
  void errorExit(const string_t&);
  bool extensionEnabled(const string_t& in_ext);
  VkFormat findDepthFormat();
  uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
  VkCommandBuffer beginOneTimeGraphicsCommands();
  void endOneTimeGraphicsCommands(VkCommandBuffer commandBuffer);
  std::shared_ptr<Swapchain> swapchain();

private:
  void init(const string_t& title, SDL_Window* win, bool vsync_enabled, bool wait_fences, bool enableDebug);
  void initVulkan(const string_t& title, SDL_Window* win, bool enableDebug);

  void createInstance(const string_t& title, SDL_Window* win);

  void init2();
  void setupDebug();
  std::vector<const char*> getRequiredExtensionNames(SDL_Window* win);
  std::vector<const char*> getValidationLayers();
  bool isValidationLayerSupported(const string_t& name);
  void debugPrintSupportedExtensions();
  void pickPhysicalDevice();
  std::unordered_map<string_t, VkExtensionProperties>& getDeviceExtensions();
  bool isExtensionSupported(const string_t& extName);
  std::vector<const char*> getEnabledDeviceExtensions();
  void createLogicalDevice();
  Vulkan::QueueFamilies* findQueueFamilies();
  void createCommandPool();
  void getDeviceProperties();
  VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

  std::unique_ptr<VulkanDebug> _pDebug = nullptr;

  std::unique_ptr<QueueFamilies> _pQueueFamilies = nullptr;
  std::shared_ptr<Swapchain> _pSwapchain = nullptr;

  VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
  VkDevice _device = VK_NULL_HANDLE;
  VkInstance _instance = VK_NULL_HANDLE;
  VkCommandPool _commandPool = VK_NULL_HANDLE;
  VkQueue _graphicsQueue = VK_NULL_HANDLE;  // Device queues are implicitly cleaned up when the device is destroyed, so we don't need to do anything in cleanup.
  VkQueue _presentQueue = VK_NULL_HANDLE;
  VkSurfaceKHR _windowSurface;
  bool _bEnableValidationLayers = true;  // TODO: set this in settings
  std::unordered_map<string_t, VkExtensionProperties> _deviceExtensions;
  std::unordered_map<std::string, VkLayerProperties> supported_validation_layers;
  VkSurfaceCapabilitiesKHR _surfaceCaps;
  uint32_t _swapchainImageCount = 0;
  bool _bPhysicalDeviceAcquired = false;
  bool _vsync_enabled = false;
  bool _wait_fences = false;
  std::unordered_set<std::string> _enabledExtensions;
  VkPhysicalDeviceProperties _deviceProperties;
  VkPhysicalDeviceLimits _physicalDeviceLimits;
  VkPhysicalDeviceFeatures _deviceFeatures;
};

}  // namespace VG

#endif
