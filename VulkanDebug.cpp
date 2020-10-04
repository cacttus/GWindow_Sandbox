#include "./VulkanDebug.h"

namespace VG {

//We could use some kind of file processing gimmick to process all vulkan structures.
//Perhaps sigtrap the access violations.
string_t VulkanDebug::get_VkGraphicsPipelineCreateInfo() {
  string_t str = "[ hex] [decimal] member_name \r\n";
  size_t off = 0;

#define ADD_VK_MEM(x, y)                 \
  str += Stz " [" + App::toHex(off, true) + "][" +off+ "]" + std::string(#y) + "\r\n"; \
  off += sizeof(x::y);

  ADD_VK_MEM(VkGraphicsPipelineCreateInfo, sType);
  ADD_VK_MEM(VkGraphicsPipelineCreateInfo, sType);
  ADD_VK_MEM(VkGraphicsPipelineCreateInfo, pNext);
  ADD_VK_MEM(VkGraphicsPipelineCreateInfo, flags);
  ADD_VK_MEM(VkGraphicsPipelineCreateInfo, stageCount);
  ADD_VK_MEM(VkGraphicsPipelineCreateInfo, pStages);
  ADD_VK_MEM(VkGraphicsPipelineCreateInfo, pVertexInputState);
  ADD_VK_MEM(VkGraphicsPipelineCreateInfo, pInputAssemblyState);
  ADD_VK_MEM(VkGraphicsPipelineCreateInfo, pTessellationState);
  ADD_VK_MEM(VkGraphicsPipelineCreateInfo, pViewportState);
  ADD_VK_MEM(VkGraphicsPipelineCreateInfo, pRasterizationState);
  ADD_VK_MEM(VkGraphicsPipelineCreateInfo, pMultisampleState);
  ADD_VK_MEM(VkGraphicsPipelineCreateInfo, pDepthStencilState);
  ADD_VK_MEM(VkGraphicsPipelineCreateInfo, pColorBlendState);
  ADD_VK_MEM(VkGraphicsPipelineCreateInfo, pDynamicState);
  ADD_VK_MEM(VkGraphicsPipelineCreateInfo, layout);
  ADD_VK_MEM(VkGraphicsPipelineCreateInfo, renderPass);
  ADD_VK_MEM(VkGraphicsPipelineCreateInfo, subpass);
  ADD_VK_MEM(VkGraphicsPipelineCreateInfo, basePipelineHandle);
  ADD_VK_MEM(VkGraphicsPipelineCreateInfo, basePipelineIndex);

  return str;
}

}  // namespace VG
