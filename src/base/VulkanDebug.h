/**
*  @file VulkanDebug.h
*  @date 20201004
*  @author Derek Page
*  @brief Debug information for vulkan.
*/
#pragma once
#ifndef __VULKANDEBUG_16018075671561130827_H__
#define __VULKANDEBUG_16018075671561130827_H__

#include "./VulkanHeader.h"
#include "./VulkanClasses.h"
namespace VG {
class ValidationLayerExtension {
public:
  const char* _layer;
  std::vector<const char*> _extensions;
};
/**
*  @class VulkanDebug
*  @brief A lot of vulkan errors are simple access violations relevant to a struct.
*         Outputs memory offsets of vulkan structs for debugging.
*         Provides enum stringification.
*/
class VulkanDebug : public VulkanObject {
public:
  VulkanDebug(std::shared_ptr<Vulkan> v, bool enableValidationLayers);
  virtual ~VulkanDebug() override;

  void createDebugObjects();

private:
  void createDebugMessenger();
  void createDebugReport();

  bool _enableDebug = false;
  VkExtFn(vkCreateDebugUtilsMessengerEXT);
  VkExtFn(vkDestroyDebugUtilsMessengerEXT);
  VkExtFn(vkCreateDebugReportCallbackEXT);
  VkExtFn(vkDestroyDebugReportCallbackEXT);

  VkDebugUtilsMessengerEXT _debugMessenger = VK_NULL_HANDLE;
  VkDebugReportCallbackEXT _debugReporter = VK_NULL_HANDLE;
};

}  // namespace VG

#endif
