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
  static constexpr const char* c_strErrDeviceLost = "VK_ERROR_DEVICE_LOST"; //TODO; move this somewhere else.
  Vulkan();
  virtual ~Vulkan();
    
  static std::shared_ptr<Vulkan> create(const string_t& title, SDL_Window* win, bool vsync_enabled, bool wait_fences, SampleCount samples);

  //Props
  VkSurfaceKHR& windowSurface();
  VkPhysicalDevice& physicalDevice();
  VkDevice& device();
  VkInstance& instance();
  VkCommandPool& commandPool();
  VkQueue& graphicsQueue();
  VkQueue& presentQueue();
  const VkSurfaceCapabilitiesKHR& surfaceCaps();
  const VkPhysicalDeviceProperties& deviceProperties();
  const VkPhysicalDeviceLimits& deviceLimits();
  const VkPhysicalDeviceFeatures& deviceFeatures();
  uint32_t swapchainImageCount();
  bool vsyncEnabled();
  bool waitFences();
  //Errors
  void checkErrors();
  void validateVkResult(VkResult res, const string_t& fname);
  void errorExit(const string_t&);
  bool extensionEnabled(const string_t& in_ext);

  //Helpers
  SampleCount maxSampleCount();
  VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels = 1);
  VkFormat findDepthFormat() ;
  VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
  uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
  VkCommandBuffer beginOneTimeGraphicsCommands();
  void endOneTimeGraphicsCommands(VkCommandBuffer commandBuffer);
  std::shared_ptr<Swapchain> swapchain();

private:
  void init(const string_t& title, SDL_Window* win, bool vsync_enabled, bool wait_fences, SampleCount samples);

  std::unique_ptr<Vulkan_Internal> _pInt;
};

}  // namespace VG

#endif
