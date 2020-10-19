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
class Vulkan {
public:
  Vulkan(const string_t& title, SDL_Window* win);
  virtual ~Vulkan();

  //Props
  VkSurfaceKHR& windowSurface();
  VkPhysicalDevice& physicalDevice();
  VkDevice& device();
  VkInstance& instance();
  VkCommandPool& commandPool();
  VkQueue& graphicsQueue();
  VkQueue& presentQueue();
  VkSurfaceCapabilitiesKHR& surfaceCaps();
  uint32_t swapchainImageCount();

  //Errors
  void checkErrors();
  void validateVkResult(VkResult res, const string_t& fname);
  void errorExit(const string_t&);

  //Helpers
  VkSampleCountFlagBits getMaxUsableSampleCount();
  VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels = 1);
  VkFormat findDepthFormat() ;
  VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
  uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
  VkCommandBuffer beginOneTimeGraphicsCommands();
  void endOneTimeGraphicsCommands(VkCommandBuffer commandBuffer);

  VkPhysicalDeviceProperties deviceProperties();
  VkPhysicalDeviceFeatures deviceFeatures();

private:
  std::unique_ptr<Vulkan_Internal> _pInt;
};

}  // namespace VG

#endif
