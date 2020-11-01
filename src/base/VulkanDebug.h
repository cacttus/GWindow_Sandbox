/**
*  @file VulkanDebug.h
*  @date 20201004
*  @author MetalMario971
*/
#pragma once
#ifndef __VULKANDEBUG_16018075671561130827_H__
#define __VULKANDEBUG_16018075671561130827_H__

#include "./SandboxHeader.h"
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

  static string_t VkGraphicsPipelineCreateInfo_toString();
  static string_t VkResult_toString(VkResult r);
  static string_t VkColorSpaceKHR_toString(VkColorSpaceKHR sp);
  static string_t VkFormat_toString(VkFormat fmt);
  static string_t VkMemoryPropertyFlags_toString(VkMemoryPropertyFlags r);
  static string_t VkRenderPassBeginInfo_toString();
  static string_t VkDescriptorType_toString(VkDescriptorType t);
  static string_t OutputMRT_toString(OutputMRT t);
  static int SampleCount_ToInt(MSAA c);


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
