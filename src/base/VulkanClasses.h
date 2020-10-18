/**
*  @file VulkanClasses.h
*  @date 09/15/2020
*  @author MetalMario971
*  @brief Vulkan class defs go here for now. We may break this up in the future.
*/
#pragma once
#ifndef __VULKANCLASSES_160279603410138410445489970863_H__
#define __VULKANCLASSES_160279603410138410445489970863_H__

#include "./VulkanHeader.h"

namespace VG {
/**
 * @class VulkanObject
 * @brief Base class for objects that use the Vulkan API.
 */
class VulkanObject {
  std::shared_ptr<Vulkan> _vulkan = nullptr;

protected:
  std::shared_ptr<Vulkan> vulkan() { return _vulkan; }

public:
  VulkanObject(std::shared_ptr<Vulkan> dev) { _vulkan = dev; }
  virtual ~VulkanObject() {}
};
/**
 * @class VulkanDeviceBuffer
 * @brief Represents a buffer that can reside on the host or on the gpu.
 * */
class VulkanDeviceBuffer_Internal;
class VulkanDeviceBuffer : public VulkanObject {
public:
  VkBuffer& buffer();
  VkDeviceMemory bufferMemory();
  VkDeviceSize totalSizeBytes();

  VulkanDeviceBuffer(std::shared_ptr<Vulkan> pvulkan, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
  virtual ~VulkanDeviceBuffer() override;
  void copy_host(void* host_buf, size_t host_bufsize,
                 size_t host_buf_offset = 0,
                 size_t gpu_buf_offset = 0,
                 size_t copy_count = std::numeric_limits<size_t>::max());
  void copy_gpu(std::shared_ptr<VulkanDeviceBuffer> host_buf,
                size_t host_buf_offset = 0,
                size_t gpu_buf_offset = 0,
                size_t copy_count = std::numeric_limits<size_t>::max());

private:
  void cleanup();
  std::unique_ptr<VulkanDeviceBuffer_Internal> _pInt;
};
/**
 * @class VulkanBuffer
 * @brief Interface for Vulkan buffers used for vertex and index data.
 *        You're supposed to use buffer pools as there is a maximum buffer allocation limit on the GPU, however this is just a demo.
 *        4096 = Nvidia GPU allocation limit.
 */
class VulkanBuffer_Internal;
class VulkanBuffer : public VulkanObject {
public:
  VulkanBuffer(std::shared_ptr<Vulkan> dev, VulkanBufferType eType, bool bStaged);
  VulkanBuffer(std::shared_ptr<Vulkan> dev, VulkanBufferType eType, bool bStaged, VkDeviceSize bufsize, void* data = nullptr, size_t datasize = 0);
  virtual ~VulkanBuffer() override;
  void writeData(void* data, size_t off, size_t datasize);
  std::shared_ptr<VulkanDeviceBuffer> hostBuffer();
  std::shared_ptr<VulkanDeviceBuffer> gpuBuffer();

private:
  std::unique_ptr<VulkanBuffer_Internal> _pInt;
};

class VulkanImage : public VulkanObject {
public:
  VulkanImage(std::shared_ptr<Vulkan> pvulkan);
  virtual ~VulkanImage() override;

  VkImageView imageView() { return _textureImageView; }
  VkFormat format() { return _format; }
  VkImage image() { return _image; }

  void allocateMemory(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                      VkImageUsageFlags usage, VkMemoryPropertyFlags properties, uint32_t mipLevels,
                      VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);

protected:
  VkImage _image = VK_NULL_HANDLE;  // If this is a VulkanBufferType::Image
  VkDeviceMemory _textureImageMemory = VK_NULL_HANDLE;
  VkImageView _textureImageView = VK_NULL_HANDLE;
  uint32_t _width = 0;
  uint32_t _height = 0;
  VkFormat _format = (VkFormat)0;  //Invalid format
};
class VulkanDepthImage : public VulkanImage {
public:
  VulkanDepthImage(std::shared_ptr<Vulkan> pvulkan) : VulkanImage(pvulkan) {
  }
};

//We should say like RenderPipe->createAttachment .. or sth. It uses the swapchain images.
class VulkanAttachment : public VulkanImage {
public:
  VulkanAttachment(std::shared_ptr<Vulkan> v, AttachmentType type) : VulkanImage(v) {
  }
};

//Used for graphics commands.
class VulkanCommands : public VulkanObject {
public:
  VulkanCommands(std::shared_ptr<Vulkan> v);
  void begin();
  void end();
  void blitImage(VkImage srcImg,
                 VkImage dstImg,
                 const BR2::iext2& srcRegion,
                 const BR2::iext2& dstRegion,
                 VkImageLayout srcLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 VkImageLayout dstLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 uint32_t srcMipLevel = 0,
                 uint32_t dstMipLevel = 0,
                 VkImageAspectFlagBits aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT, VkFilter filter = VK_FILTER_LINEAR);
  void imageTransferBarrier(VkImage image,
                            VkAccessFlagBits srcAccessFlags, VkAccessFlagBits dstAccessFlags,
                            VkImageLayout oldLayout, VkImageLayout newLayout,
                            uint32_t baseMipLevel = 0,
                            VkImageAspectFlagBits subresourceMask = VK_IMAGE_ASPECT_COLOR_BIT);

private:
  VkCommandBuffer _buf = VK_NULL_HANDLE;
};
/**
*  @class VulkanTextureImage
*  @brief Image residing on the GPU.
*/
class VulkanTextureImage : public VulkanImage {
public:
  VulkanTextureImage(std::shared_ptr<Vulkan> pvulkan, std::shared_ptr<Img32> pimg, MipmapMode mipmaps);
  virtual ~VulkanTextureImage() override;
  VkSampler sampler();
  
  void recreateMipmaps(MipmapMode mipmaps);

private:
  std::shared_ptr<VulkanDeviceBuffer> _host = nullptr;
  VkSampler _textureSampler = VK_NULL_HANDLE;
  uint32_t _mipLevels = 1;
  MipmapMode _mipmap = MipmapMode::Linear;

  bool mipmappingSupported();
  void generateMipmaps();
  void copyImageToGPU(std::shared_ptr<Img32> pimg, VkFormat img_fmt);
  void copyBufferToImage(std::shared_ptr<Img32> pimg);
  void transitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
  void flipImage20161206(uint8_t* image, int width, int height);

};  // namespace VG
//class VulkanShaderModule
//You can bind multiple pipelines in one command buffer.
class VulkanPipeline {
public:
  //Shader Pipeline.
  //VkPipeline
  //Shaders
};
class MeshComponent {
public:
};
/**
 * @class VulkanShaderModule
 * @brief Shader module with reflection tanks to Spirv-Reflect.
 * */
class VulkanShaderModule_Internal;
class VulkanShaderModule : VulkanObject {
public:
  VulkanShaderModule(std::shared_ptr<Vulkan> v, const string_t& base_name, const string_t& file);
  virtual ~VulkanShaderModule() override;

  VkPipelineShaderStageCreateInfo getPipelineStageCreateInfo();

private:
  std::unique_ptr<VulkanShaderModule_Internal> _pInt;
};
/**
 * 
 * */
//class VulkanPipelineShader_Internal;
class VulkanPipelineShader : public VulkanObject {
public:
  VulkanPipelineShader(std::shared_ptr<Vulkan> v, const string_t& name, const std::vector<string_t>& files);
  virtual ~VulkanPipelineShader() override;

  std::vector<VkPipelineShaderStageCreateInfo> getShaderStageCreateInfos();

private:
  std::vector<std::shared_ptr<VulkanShaderModule>> _modules;
  string_t _name = "*undefined*";
};

//Doing it this way we can dynamically configure the swapchain.
//But it isn't feasible. We would need to reallocate the descriptor pools since we pool based on swapchain images.
// class VulkanSwapchain{
//   public:
//   int32_t _iConcurrentFrames = 3;
//   size_t _currentFrame = 0;
//   std::vector<VkFence> _inFlightFences;
//   std::vector<VkFence> _imagesInFlight;
//   std::vector<VkSemaphore> _imageAvailableSemaphores;
//   std::vector<VkSemaphore> _renderFinishedSemaphores;
//   std::vector<std::shared_ptr<VulkanBuffer>> _viewProjUniformBuffers;  //One per swapchain image since theres multiple frames in flight.
//   std::vector<std::shared_ptr<VulkanBuffer>> _instanceUniformBuffers;  //One per swapchain image since theres multiple frames in flight.
//   int32_t _numInstances = 3;
//   VkSwapchainKHR _swapChain = VK_NULL_HANDLE;
//   VkExtent2D _swapChainExtent;
//   VkFormat _swapChainImageFormat;
//   std::vector<VkImage> _swapChainImages;
//   std::vector<VkImageView> _swapChainImageViews;
//   std::vector<VkFramebuffer> _swapChainFramebuffers;
//   std::vector<VkCommandBuffer> _commandBuffers;
//   bool _bSwapChainOutOfDate = false;
// 
// 
// 
//   void createSyncObjects() {
//     BRLogInfo("Creating Rendering Semaphores.");
//     _imageAvailableSemaphores.resize(_iConcurrentFrames);
//     _renderFinishedSemaphores.resize(_iConcurrentFrames);
//     _inFlightFences.resize(_iConcurrentFrames);
//     _imagesInFlight.resize(_swapChainImages.size(), VK_NULL_HANDLE);
// 
//     for (int i = 0; i < _iConcurrentFrames; ++i) {
//       VkSemaphoreCreateInfo semaphoreInfo = {
//         .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
//         .pNext = nullptr,
//         .flags = 0,
//       };
//       CheckVKR(vkCreateSemaphore, vulkan()->device(), &semaphoreInfo, nullptr, &_imageAvailableSemaphores[i]);
//       CheckVKR(vkCreateSemaphore, vulkan()->device(), &semaphoreInfo, nullptr, &_renderFinishedSemaphores[i]);
// 
//       VkFenceCreateInfo fenceInfo = {
//         .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
//         .pNext = nullptr,
//         .flags = VK_FENCE_CREATE_SIGNALED_BIT,  //Fences must always be created in a signaled state.
//       };
// 
//       CheckVKR(vkCreateFence, vulkan()->device(), &fenceInfo, nullptr, &_inFlightFences[i]);
//     }
//   }
// };

}  // namespace VG

#endif
