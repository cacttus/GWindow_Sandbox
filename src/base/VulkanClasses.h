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
class VulkanObject : public std::enable_shared_from_this<VulkanObject> {
  std::shared_ptr<Vulkan> _vulkan = nullptr;

protected:
  std::shared_ptr<Vulkan> vulkan() { return _vulkan; }

  template <typename Ty>
  std::shared_ptr<Ty> getThis() {
    return std::dynamic_pointer_cast<Ty>(this->shared_from_this());
  }

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
/**
 * @class VulkanShaderModule
 * @brief Shader module with reflection tanks to Spirv-Reflect.
 * */
class ShaderModule_Internal;
class ShaderModule : VulkanObject {
public:
  ShaderModule(std::shared_ptr<Vulkan> v, const string_t& base_name, const string_t& file);
  virtual ~ShaderModule() override;

  VkPipelineShaderStageCreateInfo getPipelineStageCreateInfo();
  SpvReflectShaderModule* reflectionData();
  const string_t& name();

private:
  std::unique_ptr<ShaderModule_Internal> _pInt;
};
class Descriptor {
public:
  string_t _name = "";
  VkDescriptorType _type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
  uint32_t _binding = 0;  //The acutal binding index.
  uint32_t _arraySize = 0;
  uint32_t _blockSize = 0;
  VkShaderStageFlags _stage;
  bool _isBound = false;
};
//TODO: this may not be necessary - BR2::VertexFormat
class VertexAttribute {
public:
  string_t _name = "";
  uint32_t _componentSizeBytes = 0;  //Size of EACH component
  uint32_t _componentCount = 0;
  uint32_t _matrixSize = 0;  //Number of entries 4, 9, 16 ..
  //VkFormat _format; // FYI Attribute formats use colors.
  VkVertexInputAttributeDescription _desc;
  SpvReflectTypeFlags _typeFlags;
  size_t _totalSizeBytes = 0;
  BR2::VertexUserType _userType;
};
/**
 * 
 * */
//class VulkanPipelineShader_Internal;
class PipelineShader : public VulkanObject {
public:
  PipelineShader(std::shared_ptr<Vulkan> v, const string_t& name, const std::vector<string_t>& files);
  virtual ~PipelineShader() override;

  std::vector<VkPipelineShaderStageCreateInfo> getShaderStageCreateInfos();
  bool bindUBO(const string_t& name, uint32_t swapchainImageIndex, std::shared_ptr<VulkanBuffer> buf, VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE);  //buf =  Optionally, update.
  bool bindSampler(const string_t& name, uint32_t swapchainImageIndex, std::shared_ptr<VulkanTextureImage> texture, uint32_t arrayIndex = 0);
  void bindDescriptorSets(std::shared_ptr<CommandBuffer> cmdBuf, uint32_t swapchainImageIndex, VkPipelineLayout pipeline);

  const string_t& name() { return _name; }

  VkDescriptorSetLayout getVkDescriptorSetLayout() { return _descriptorSetLayout; }
  VkPipelineVertexInputStateCreateInfo getVertexInputInfo(std::shared_ptr<BR2::VertexFormat> fmt);
  VkPipelineInputAssemblyStateCreateInfo getInputAssembly(VkPrimitiveTopology topo = VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

private:
  void createInputs();
  void createDescriptors();
  void cleanupDescriptors();
  BR2::VertexUserType parseUserType(const string_t& err);

  string_t _name = "*undefined*";
  std::shared_ptr<Descriptor> getDescriptor(const string_t& name);

  VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;
  VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> _descriptorSets;

  //VkPipelineVertexInputStateCreateInfo  _vkPipelineInputState
  //VkPipelineInputAssemblyStateCreateInfo

  std::vector<VkVertexInputAttributeDescription> _attribDescriptions;
  VkVertexInputBindingDescription _bindingDesc;

  std::vector<std::shared_ptr<ShaderModule>> _modules;
  std::unordered_map<string_t, std::shared_ptr<Descriptor>> _descriptors;
  std::vector<std::shared_ptr<VertexAttribute>> _attributes;
  bool _bInstanced = false;  //True if we find gl_InstanceIndex (gl_instanceID) in the shader - and we will bind vertexes per instance.
};

/**
 * @class Pipeline
 * Essentially, a GL ShaderProgram with VAO state.
 * */
// class Pipeline : public VulkanObject {
// public:
//   Pipeline(std::shared_ptr<Vulkan> v, std::shared_ptr<RenderFrame> sw, std::shared_ptr<Mesh> geom, std::shared_ptr<PipelineShader> shader);
//   virtual ~Pipeline() override;
//
//   void getOrLoadPipeline(std::shared_ptr<RenderFrame> sw, std::shared_ptr<PipelineShader> shader, std::shared_ptr<Mesh> geom);
//
//   void setViewport(std::shared_ptr<Pipeline> p);
//
//   void cleanupPipeline();
//   void createRenderPass(std::shared_ptr<RenderFrame> sw);
//   void getOrLoadPipeline(std::shared_ptr<RenderFrame> sw);
//
//   VkRenderPass _renderPass = VK_NULL_HANDLE;
//   VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;
//   VkPipeline _graphicsPipeline = VK_NULL_HANDLE;
// };
/**


 /* 
 * @class FrameData
 * Shader data copied and made available to all frames
 * */
// class FrameData : public VulkanObject {
// public:
//   //Shader, Vertex Format, Primitive Type
//
//   FrameData();
//   virtual ~FrameData() override;
//
//   //VkCommandBuffer commandBuffer() { return _commandBuffer; }
//
//   //UBOs
//   //FBOs
//
// private:
//   VkFramebuffer _framebuffer = VK_NULL_HANDLE;  // _swapChainFramebuffers;
// };

class Pipeline {
public:
};
//Dummy
class ShaderData {
public:
  //Dummy data, for now
  BR2::vec4 _clearColor = { 0, 0, 0, 1 };
  VkFramebuffer _framebuffer = VK_NULL_HANDLE;
  VkRenderPass _renderPass = VK_NULL_HANDLE;
  VkPipeline _pipeline = VK_NULL_HANDLE;  //_graphicsPipeline
};
/**
 * @class CommandBuffer
 * */

class CommandBuffer : public VulkanObject {
public:
  CommandBuffer(std::shared_ptr<Vulkan> ob, std::shared_ptr<RenderFrame> frame);
  virtual ~CommandBuffer() override;

  VkCommandBuffer getVkCommandBuffer() { return _commandBuffer; }
  void cmdSetViewport(const BR2::uext2& size);
  void beginPass(ShaderData& shaderData);
  void endPass();

private:
  CommandBufferState _state = CommandBufferState::Unset;
  std::shared_ptr<RenderFrame> _pRenderFrame = nullptr;
  VkCommandPool _sharedPool = VK_NULL_HANDLE;       //Do not free
  VkCommandBuffer _commandBuffer = VK_NULL_HANDLE;  //_commandBuffers;
};
/**
 * @class RenderFrame
 * */
class RenderFrame : public VulkanObject {
public:
  RenderFrame(std::shared_ptr<Vulkan> v, std::shared_ptr<Swapchain> ps, uint32_t frameIndex, VkImage img);
  virtual ~RenderFrame() override;

  const BR2::uext2& imageSize();
  VkFormat imageFormat();
  std::shared_ptr<Swapchain> getSwapchain() { return _pSwapchain; }
  VkImageView getVkImageView() { return _imageView; }
  std::shared_ptr<CommandBuffer> commandBuffer() { return _pCommandBuffer; } //Possible to have multiple buffers as vkQUeueSubmit allows for multiple. Need?
  uint32_t currentRenderingImageIndex() { return _currentRenderingImageIndex; }  //TODO: remove later
  uint32_t frameIndex() { return _frameIndex;}  //Image index in the swapchain array

  void init();
  void beginFrame();
  void endFrame();

private:
  std::shared_ptr<Swapchain> _pSwapchain = nullptr;
  std::shared_ptr<CommandBuffer> _pCommandBuffer = nullptr;

  //TODO: Framedata for shaders.
  //<VertexFormat, Shader, Primitive Type>
  //std::vector<FrameData> _pipelines;  //Pipelines

  uint32_t _frameIndex = 0;  
  VkImage _image = VK_NULL_HANDLE;
  VkImageView _imageView = VK_NULL_HANDLE;
  VkFence _inFlightFence = VK_NULL_HANDLE;       
  VkFence _imageInFlightFence = VK_NULL_HANDLE;  
  VkSemaphore _imageAvailable = VK_NULL_HANDLE;  
  VkSemaphore _renderFinished = VK_NULL_HANDLE;  
  uint32_t _currentRenderingImageIndex = 0;

  void createSyncObjects();
  void cleanupSyncObjects();
};
/**
 * @class Swapchain
 * */
class Swapchain : public VulkanObject {
public:
  Swapchain(std::shared_ptr<Vulkan> v);
  virtual ~Swapchain() override;

  //Get the next available frame if one is available. Non-Blocking
  std::shared_ptr<RenderFrame> acquireFrame();

  void beginFrame();  //Remove
  void endFrame();    //Remove

  const BR2::uext2& imageSize();
  VkFormat imageFormat();
  void outOfDate() { _bSwapChainOutOfDate = true; }
  bool isOutOfDate() { return _bSwapChainOutOfDate; }
  void waitImage(uint32_t imageIndex, VkFence myFence);
  void initSwapchain(const BR2::uext2& window_size);
  void updateSwapchain(const BR2::uext2& window_size);
  VkSwapchainKHR getVkSwapchain() { return _swapChain; }
  std::vector<std::shared_ptr<RenderFrame>>& frames() { return _frames; }

private:
  void createSwapChain(const BR2::uext2& window_size);
  void cleanupSwapChain();
  bool findValidSurfaceFormat(std::vector<VkFormat> fmts, VkSurfaceFormatKHR& fmt_out);
  bool findValidPresentMode(VkPresentModeKHR& pm_out);

  std::vector<std::shared_ptr<RenderFrame>> _frames;
  size_t _currentFrame = 0;
  std::vector<VkFence> _imagesInFlight;  //Shared handle do not delete
  VkSwapchainKHR _swapChain = VK_NULL_HANDLE;
  BR2::uext2 _swapChainExtent;
  VkFormat _swapChainImageFormat;
  bool _bSwapChainOutOfDate = false;
};

}  // namespace VG

#endif
