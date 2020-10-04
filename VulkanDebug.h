/**
*  @file VulkanDebug.h
*  @date 20201004
*  @author MetalMario971
*/
#pragma once
#ifndef __VULKANDEBUG_16018075671561130827_H__
#define __VULKANDEBUG_16018075671561130827_H__

#include "./SandboxHeader.h"

namespace VG {
/**
*  @class VulkanDebug
*  @brief
* A lot of vulkan errors are simple access violations relevant to a struct.
* This class that maps out the memory offsets of vulkan structs.
*/
class VulkanDebug {
public:
  static string_t get_VkGraphicsPipelineCreateInfo();
};

}//ns Game



#endif
