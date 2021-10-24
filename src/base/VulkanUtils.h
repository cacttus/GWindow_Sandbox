/**
*  @file VulkanDebug.h
*  @date 20201004
*  @author Derek Page
*/
#pragma once
#ifndef __VULKANUTILS_16018075671561130827_H__
#define __VULKANUTILS_16018075671561130827_H__

#include "./VulkanHeader.h"
#include "./VulkanClasses.h"
namespace VG {

class VulkanUtils {
public:
  static string_t VkGraphicsPipelineCreateInfo_toString();
  static string_t VkResult_toString(VkResult r);
  static string_t VkColorSpaceKHR_toString(VkColorSpaceKHR sp);
  static string_t VkFormat_toString(VkFormat fmt);
  static string_t VkMemoryPropertyFlags_toString(VkMemoryPropertyFlags r);
  static string_t VkRenderPassBeginInfo_toString();
  static string_t VkDescriptorType_toString(VkDescriptorType t);
  static string_t OutputMRT_toString(OutputMRT t);
  static int SampleCount_ToInt(MSAA c);
  static string_t vkShaderStageFlagBits_toString(VkShaderStageFlagBits flag);
  static string_t ShaderStage_toString(ShaderStage stage);
  static VkShaderStageFlagBits ShaderStage_to_VkShaderStageFlagBits(ShaderStage input);
  static VkShaderStageFlagBits spvReflectShaderStageFlagBitsToVk(SpvReflectShaderStageFlagBits b);
  static ShaderStage spvReflectShaderStageFlagBits_To_ShaderStage(SpvReflectShaderStageFlagBits input);

};

}  // namespace VG

#endif
