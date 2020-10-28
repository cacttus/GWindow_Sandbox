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
namespace VG {

/**
*  @class VulkanDebug
*  @brief A lot of vulkan errors are simple access violations relevant to a struct.
*         Outputs memory offsets of vulkan structs for debugging.
*         Provides enum stringification.
*/
class VulkanDebug {
public:
  static string_t VkGraphicsPipelineCreateInfo_toString();
  static string_t VkResult_toString(VkResult r);
  static string_t VkColorSpaceKHR_toString(VkColorSpaceKHR sp);
  static string_t VkFormat_toString(VkFormat fmt);
  static string_t VkMemoryPropertyFlags_toString(VkMemoryPropertyFlags r);
  static string_t VkRenderPassBeginInfo_toString();
  static string_t VkDescriptorType_toString(VkDescriptorType t);
  static string_t OutputMRT_toString(OutputMRT t);
  static int SampleCount_ToInt(SampleCount c);
  
};

}  // namespace VG

#endif
