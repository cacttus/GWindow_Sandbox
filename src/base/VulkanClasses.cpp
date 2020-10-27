#include "./VulkanClasses.h"
#include "./Vulkan.h"
#include "./VulkanDebug.h"
#include "./GameClasses.h"

namespace VG {

#pragma region VulkanDeviceBuffer

class VulkanDeviceBuffer::VulkanDeviceBuffer_Internal {
public:
  VkBuffer _buffer = VK_NULL_HANDLE;  // If a UniformBuffer, VertexBuffer, or IndexBuffer
  VkDeviceMemory _bufferMemory = VK_NULL_HANDLE;
  VkDeviceSize _allocatedSize = 0;
  bool _isGpuBuffer = false;  //for staged buffers, this class represents the GPU side of the buffer. This buffer resides on the GPU and vkMapMemory can't be called on it.
};

VulkanDeviceBuffer::VulkanDeviceBuffer(std::shared_ptr<Vulkan> pvulkan, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) : VulkanObject(pvulkan) {
  _pInt = std::make_unique<VulkanDeviceBuffer_Internal>();

  BRLogInfo("Allocating vertex buffer: " + size + "B.");
  _pInt->_allocatedSize = size;

  if ((properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) > 0) {
    _pInt->_isGpuBuffer = true;
  }

  VkBufferCreateInfo buffer_info = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,  // VkStructureType
    .pNext = nullptr,                               // const void*
    .flags = 0,                                     // VkBufferCreateFlags    -- Sparse Binding Info: https://www.asawicki.info/news_1698_vulkan_sparse_binding_-_a_quick_overview
    .size = size,                                   // VkDeviceSize
    .usage = usage,                                 // VkBufferUsageFlags
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,       // VkSharingMode
    //Queue family is ony used if buffer is shared, VK_SHARING_MODE_CONCURRENT
    .queueFamilyIndexCount = 0,      // uint32_t
    .pQueueFamilyIndices = nullptr,  // const uint32_t*
  };
  CheckVKR(vkCreateBuffer, vulkan()->device(), &buffer_info, nullptr, &_pInt->_buffer);

  VkMemoryRequirements mem_inf;
  vkGetBufferMemoryRequirements(vulkan()->device(), _pInt->_buffer, &mem_inf);

  VkMemoryAllocateInfo allocInfo = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,  //     VkStructureType
    .pNext = nullptr,                                 // const void*
    .allocationSize = mem_inf.size,
    //host coherent: Memory is automatically "refreshed" between host/gpu when its updated.
    //host visible: memory is accessible on the CPU side
    .memoryTypeIndex = vulkan()->findMemoryType(mem_inf.memoryTypeBits, properties)  //     uint32_t
  };
  //Same
  CheckVKR(vkAllocateMemory, vulkan()->device(), &allocInfo, nullptr, &_pInt->_bufferMemory);
  CheckVKR(vkBindBufferMemory, vulkan()->device(), _pInt->_buffer, _pInt->_bufferMemory, 0);
}
VulkanDeviceBuffer::~VulkanDeviceBuffer() {
  cleanup();
  _pInt = nullptr;
}

VkBuffer& VulkanDeviceBuffer::buffer() { return _pInt->_buffer; }  //Returns the buffer for un-staged (host) buffers only.
VkDeviceMemory VulkanDeviceBuffer::bufferMemory() { return _pInt->_bufferMemory; }
VkDeviceSize VulkanDeviceBuffer::totalSizeBytes() { return _pInt->_allocatedSize; }

void VulkanDeviceBuffer::copy_host(void* host_buf, size_t host_bufsize,
                                   size_t host_buf_offset,
                                   size_t gpu_buf_offset,
                                   size_t copy_count) {
  //Copy data to the host buffer, this is required for both staged and host-only buffers.
  if (copy_count == std::numeric_limits<size_t>::max()) {
    copy_count = _pInt->_allocatedSize;
  }

  AssertOrThrow2(_pInt->_isGpuBuffer == false);
  AssertOrThrow2(host_buf_offset + copy_count <= host_bufsize);
  AssertOrThrow2(gpu_buf_offset + copy_count <= _pInt->_allocatedSize);

  void* gpu_data = nullptr;
  CheckVKR(vkMapMemory, vulkan()->device(), _pInt->_bufferMemory, gpu_buf_offset, _pInt->_allocatedSize, 0, &gpu_data);
  memcpy(gpu_data, host_buf, copy_count);
  vkUnmapMemory(vulkan()->device(), _pInt->_bufferMemory);
}
void VulkanDeviceBuffer::copy_gpu(std::shared_ptr<VulkanDeviceBuffer> host_buf,
                                  size_t host_buf_offset,
                                  size_t gpu_buf_offset,
                                  size_t copy_count) {
  //If this buffer resides on the GPU memory this copies data to GPU
  if (copy_count == std::numeric_limits<size_t>::max()) {
    copy_count = _pInt->_allocatedSize;
  }
  AssertOrThrow2(_pInt->_isGpuBuffer == true);
  AssertOrThrow2(host_buf != nullptr);
  AssertOrThrow2(host_buf_offset + copy_count <= host_buf->totalSizeBytes());
  AssertOrThrow2(gpu_buf_offset + copy_count <= _pInt->_allocatedSize);

  auto buf = vulkan()->beginOneTimeGraphicsCommands();

  VkBufferCopy copyRegion{};
  copyRegion.srcOffset = host_buf_offset;  // Optional
  copyRegion.dstOffset = gpu_buf_offset;   // Optional
  copyRegion.size = copy_count;

  vulkan()->endOneTimeGraphicsCommands(buf);
}
void VulkanDeviceBuffer::cleanup() {
  vkDestroyBuffer(vulkan()->device(), _pInt->_buffer, nullptr);
  vkFreeMemory(vulkan()->device(), _pInt->_bufferMemory, nullptr);
  _pInt->_buffer = VK_NULL_HANDLE;
  _pInt->_bufferMemory = VK_NULL_HANDLE;
}

#pragma endregion

#pragma region VulkanBuffer

class VulkanBuffer::VulkanBuffer_Internal {
public:
  std::shared_ptr<VulkanDeviceBuffer> _hostBuffer = nullptr;
  std::shared_ptr<VulkanDeviceBuffer> _gpuBuffer = nullptr;
  VulkanBufferType _eType = VulkanBufferType::VertexBuffer;
  bool _bUseStagingBuffer = false;
};
VulkanBuffer::VulkanBuffer(std::shared_ptr<Vulkan> dev, VulkanBufferType eType, bool bStaged) : VulkanObject(dev) {
  _pInt = std::make_unique<VulkanBuffer_Internal>();
  _pInt->_bUseStagingBuffer = bStaged;
  _pInt->_eType = eType;
}
VulkanBuffer::VulkanBuffer(std::shared_ptr<Vulkan> dev, VulkanBufferType eType, bool bStaged, VkDeviceSize bufsize, void* data, size_t datasize) : VulkanBuffer(dev, eType, bStaged) {
  //Enable staging for efficiency.
  //This buffer becomes a staging buffer, and creates a sub-buffer class that represents GPU memory.
  VkMemoryPropertyFlags bufType = 0;

  if (_pInt->_eType == VulkanBufferType::IndexBuffer) {
    bufType = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  }
  else if (_pInt->_eType == VulkanBufferType::VertexBuffer) {
    bufType = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  }
  else if (_pInt->_eType == VulkanBufferType::UniformBuffer) {
    bufType = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (_pInt->_bUseStagingBuffer == true) {
      BRLogWarn("Uniform buffer resides in GPU memory. This will cause a performance penalty if the buffer is updated often (per frame).");
    }
  }
  else {
    BRThrowException(Stz "Invalid buffer type '" + (int)_pInt->_eType + "'.");
  }

  if (_pInt->_bUseStagingBuffer) {
    //Create a staging buffer for efficiency operations
    //Staging buffers only make sense for data that resides on the GPu and doesn't get updated per frame.
    // Uniform data is not a goojd option for staging buffers, but mesh and bone data is a good option.
    _pInt->_hostBuffer = std::make_shared<VulkanDeviceBuffer>(vulkan(), bufsize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    _pInt->_gpuBuffer = std::make_shared<VulkanDeviceBuffer>(vulkan(), bufsize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | bufType, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  }
  else {
    //Allocate non-staged
    _pInt->_hostBuffer = std::make_shared<VulkanDeviceBuffer>(vulkan(), bufsize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  }

  if (data != nullptr) {
    writeData(data, 0, datasize);
  }
}
VulkanBuffer::~VulkanBuffer() {
  if (_pInt->_gpuBuffer) {
    //_pInt->_gpuBuffer->cleanup();
    _pInt->_gpuBuffer = nullptr;
  }
  if (_pInt->_hostBuffer) {
    // _pInt->_hostBuffer->cleanup();
    _pInt->_hostBuffer = nullptr;
  }

  _pInt = nullptr;
}

std::shared_ptr<VulkanDeviceBuffer> VulkanBuffer::hostBuffer() { return _pInt->_hostBuffer; }
std::shared_ptr<VulkanDeviceBuffer> VulkanBuffer::gpuBuffer() { return _pInt->_gpuBuffer; }

void VulkanBuffer::writeData(void* data, size_t off, size_t datasize) {
  //Copy data to the GPU

  if (_pInt->_hostBuffer) {
    _pInt->_hostBuffer->copy_host(data, datasize);
  }
  if (_pInt->_gpuBuffer) {
    _pInt->_gpuBuffer->copy_gpu(_pInt->_hostBuffer);
  }
}

#pragma endregion

#pragma region VulkanImage

VulkanImage::VulkanImage(std::shared_ptr<Vulkan> pvulkan) : VulkanObject(pvulkan) {
}
VulkanImage::VulkanImage(std::shared_ptr<Vulkan> pvulkan, VkImage img, uint32_t w, uint32_t h, VkFormat imgFormat) : VulkanObject(pvulkan) {
  //This is for passing the swapchain image in for render-to-texture system.
  _image = img;
  _format = imgFormat;
  _width = w;
  _height = h;
}
VulkanImage::~VulkanImage() {
  if (_image != VK_NULL_HANDLE) {
    vkDestroyImage(vulkan()->device(), _image, nullptr);
  }
  if (_imageMemory != VK_NULL_HANDLE) {
    vkFreeMemory(vulkan()->device(), _imageMemory, nullptr);
  }
  if (_imageView != VK_NULL_HANDLE) {
    vkDestroyImageView(vulkan()->device(), _imageView, nullptr);
  }
}
void VulkanImage::allocateMemory(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                                 VkImageUsageFlags usage, VkMemoryPropertyFlags properties, uint32_t mipLevels,
                                 VkSampleCountFlagBits samples) {
  if (_image != VK_NULL_HANDLE) {
    BRThrowException("Tried to reallocate an image that was already allocated.");
  }
  //Allocates Image memory and binds it.
  if (mipLevels < 1) {
    BRLogError("Miplevels was < 1 for image. Setting to 1");
    mipLevels = 1;
  }
  _width = width;
  _height = height;
  _format = format;
  VkImageCreateInfo imageInfo = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,  // VkStructureType
    .pNext = nullptr,                              // const void*
    .flags = 0,                                    // VkImageCreateFlags
    .imageType = VK_IMAGE_TYPE_2D,                 // VkImageType
    .format = format,                              //,         // VkFormat
    .extent = {
      .width = width,
      .height = height,
      .depth = 1 },                              // VkExtent3D
    .mipLevels = mipLevels,                      // uint32_t
    .arrayLayers = 1,                            // uint32_t
    .samples = samples,                          // VkSampleCountFlagBits
    .tiling = tiling,                            // VkImageTiling
    .usage = usage,                              // VkImageUsageFlags
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,    // VkSharingMode
    .queueFamilyIndexCount = 0,                  // uint32_t
    .pQueueFamilyIndices = nullptr,              // const uint32_t*
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,  // VkImageLayout
  };
  CheckVKR(vkCreateImage, vulkan()->device(), &imageInfo, nullptr, &_image);

  VkMemoryRequirements mem_req;
  vkGetImageMemoryRequirements(vulkan()->device(), _image, &mem_req);
  VkMemoryAllocateInfo allocInfo = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .pNext = nullptr,
    .allocationSize = mem_req.size,
    .memoryTypeIndex = vulkan()->findMemoryType(mem_req.memoryTypeBits, properties)
  };
  CheckVKR(vkAllocateMemory, vulkan()->device(), &allocInfo, nullptr, &_imageMemory);
  CheckVKR(vkBindImageMemory, vulkan()->device(), _image, _imageMemory, 0);
}
void VulkanImage::createView(VkFormat fmt, VkImageAspectFlagBits aspect, uint32_t mipLevel) {
  _imageView = vulkan()->createImageView(_image, fmt, aspect, mipLevel);
}

#pragma endregion

#pragma region VulkanCommands

VulkanCommands::VulkanCommands(std::shared_ptr<Vulkan> v) : VulkanObject(v) {
}
void VulkanCommands::begin() {
  _buf = vulkan()->beginOneTimeGraphicsCommands();
}
void VulkanCommands::end() {
  vulkan()->endOneTimeGraphicsCommands(_buf);
}
void VulkanCommands::blitImage(VkImage srcImg,
                               VkImage dstImg,
                               const BR2::irect2& srcRegion,
                               const BR2::irect2& dstRegion,
                               VkImageLayout srcLayout,
                               VkImageLayout dstLayout,
                               uint32_t srcMipLevel,
                               uint32_t dstMipLevel,
                               VkImageAspectFlagBits aspectFlags, VkFilter filter) {
  VkImageBlit blit = {
    .srcSubresource = {
      .aspectMask = (VkImageAspectFlags)aspectFlags,                                                                // VkImageAspectFlags
      .mipLevel = srcMipLevel,                                                                                      // uint32_t
      .baseArrayLayer = 0,                                                                                          // uint32_t
      .layerCount = 1,                                                                                              // uint32_t
    },                                                                                                              // VkImageSubresourceLayers
    .srcOffsets = { { srcRegion.pos.x, srcRegion.pos.y, 0 }, { srcRegion.size.width, srcRegion.size.height, 1 } },  // VkOffset3D
    .dstSubresource = {
      .aspectMask = (VkImageAspectFlags)aspectFlags,  // VkImageAspectFlags
      .mipLevel = dstMipLevel,                        // uint32_t
      .baseArrayLayer = 0,                            // uint32_t
      .layerCount = 1,                                // uint32_t
    },
    .dstOffsets = { { dstRegion.pos.x, dstRegion.pos.y, 0 }, { dstRegion.size.width, dstRegion.size.height, 1 } },  // VkOffset3D
  };
  vkCmdBlitImage(_buf,
                 srcImg, srcLayout,
                 dstImg, dstLayout,
                 1, &blit,
                 filter);
}
void VulkanCommands::imageTransferBarrier(VkImage image,
                                          VkAccessFlagBits srcAccessFlags, VkAccessFlagBits dstAccessFlags,
                                          VkImageLayout oldLayout, VkImageLayout newLayout,
                                          uint32_t baseMipLevel,
                                          VkImageAspectFlagBits subresourceMask) {
  VkImageMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .pNext = nullptr,
    .srcAccessMask = (VkAccessFlags)srcAccessFlags,
    .dstAccessMask = (VkAccessFlags)dstAccessFlags,
    .oldLayout = oldLayout,
    .newLayout = newLayout,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = image,
    .subresourceRange = {
      .aspectMask = (VkImageAspectFlags)subresourceMask,
      .baseMipLevel = baseMipLevel,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
  };
  vkCmdPipelineBarrier(_buf,
                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                       0,
                       0, nullptr,
                       0, nullptr,
                       1, &barrier);
}

#pragma endregion

#pragma region CommandBuffer

CommandBuffer::CommandBuffer(std::shared_ptr<Vulkan> v, std::shared_ptr<RenderFrame> pframe) : VulkanObject(v) {
  _sharedPool = vulkan()->commandPool();
  _pRenderFrame = pframe;

  //_commandBuffers.resize(_swapChainFramebuffers.size());
  VkCommandBufferAllocateInfo allocInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .pNext = nullptr,
    .commandPool = _sharedPool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,

  };
  CheckVKR(vkAllocateCommandBuffers, vulkan()->device(), &allocInfo, &_commandBuffer);
}
CommandBuffer::~CommandBuffer() {
  vkFreeCommandBuffers(vulkan()->device(), _sharedPool, 1, &_commandBuffer);
}
void CommandBuffer::validateState(bool b) {
  if (!b) {
    Gu::debugBreak();
    BRThrowException("Invalid Command Buffer State.");
  }
}
void CommandBuffer::begin() {
  validateState(_state == CommandBufferState::End || _state == CommandBufferState::Unset);

  //VkCommandBufferResetFlags
  CheckVKR(vkResetCommandBuffer, _commandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

  VkCommandBufferBeginInfo beginInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .pNext = nullptr,
    .flags = 0,  //VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    .pInheritanceInfo = nullptr,
  };
  CheckVKR(vkBeginCommandBuffer, _commandBuffer, &beginInfo);
  _state = CommandBufferState::Begin;
}
void CommandBuffer::end() {
  validateState(_state == CommandBufferState::Begin || _state == CommandBufferState::EndPass);

  CheckVKR(vkEndCommandBuffer, _commandBuffer);
  _state = CommandBufferState::End;
}
bool CommandBuffer::beginPass() {
  validateState(_state == CommandBufferState::Begin || _state == CommandBufferState::EndPass);

  _state = CommandBufferState::BeginPass;
  return true;
}
void CommandBuffer::cmdSetViewport(const BR2::urect2& extent) {
  validateState(_state == CommandBufferState::BeginPass);
  if (_state != CommandBufferState::BeginPass) {
    BRLogError("setViewport called on invalid command buffer state, state='" + std::to_string((int)_state) + "'");
    return;
  }
  VkViewport viewport = {
    .x = static_cast<float>(extent.pos.x),
    .y = static_cast<float>(extent.pos.y),
    .width = static_cast<float>(extent.size.width),
    .height = static_cast<float>(extent.size.height),
    .minDepth = 0,
    .maxDepth = 1,
  };

  VkRect2D scissor{};
  scissor.offset = { 0, 0 };
  scissor.extent = {
    .width = _pRenderFrame->getSwapchain()->imageSize().width,
    .height = _pRenderFrame->getSwapchain()->imageSize().height
  };
  vkCmdSetViewport(_commandBuffer, 0, 1, &viewport);
  vkCmdSetScissor(_commandBuffer, 0, 1, &scissor);
}
void CommandBuffer::endPass() {
  validateState(_state == CommandBufferState::BeginPass);
  //This is called once when all pipeline rendering is complete
  vkCmdEndRenderPass(_commandBuffer);
  _state = CommandBufferState::EndPass;
}

#pragma endregion

#pragma region VulkanTextureImage

VulkanTextureImage::VulkanTextureImage(std::shared_ptr<Vulkan> pvulkan, std::shared_ptr<Img32> pimg, MipmapMode mipmaps, VkSampleCountFlagBits samples) : VulkanImage(pvulkan) {
  BRLogInfo("Creating Vulkan image");
  VkFormat img_fmt = VK_FORMAT_R8G8B8A8_SRGB;  //VK_FORMAT_R8G8B8A8_UINT;

  _mipmap = mipmaps;

  VkImageUsageFlagBits transfer_src = (VkImageUsageFlagBits)0;
  if (_mipmap != MipmapMode::Disabled) {
    _mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(pimg->_width, pimg->_height)))) + 1;
    transfer_src = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;  //We need to use this image as a src transfer to fill each mip level.
  }
  else {
    _mipLevels = 1;
  }

  //Not called if we are passing in a VulaknImage alrady created.
  allocateMemory(pimg->_width, pimg->_height,
                 img_fmt,
                 VK_IMAGE_TILING_OPTIMAL,
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | transfer_src,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _mipLevels, samples);
  // this was for OpenGL may not be needed
  //flipImage20161206(pimg->_data, pimg->_width, pimg->_height);

  createView(img_fmt, VK_IMAGE_ASPECT_COLOR_BIT);

  copyImageToGPU(pimg, img_fmt);

  createSampler();

  if (_mipmap != MipmapMode::Disabled) {
    generateMipmaps();
  }
}
VulkanTextureImage::VulkanTextureImage(std::shared_ptr<Vulkan> pvulkan, uint32_t w, uint32_t h, MipmapMode mipmaps, VkSampleCountFlagBits samples) : VulkanImage(pvulkan) {

  //**This is a test constructor for MRT images.
  //**This is a test constructor for MRT images.
  //**This is a test constructor for MRT images.
  //**This is a test constructor for MRT images.
  BRLogInfo("Creating Vulkan image");
  VkFormat img_fmt = VK_FORMAT_B8G8R8A8_SRGB;  //VK_FORMAT_R8G8B8A8_UINT;

  _mipmap = mipmaps;

  VkImageUsageFlagBits transfer_src = (VkImageUsageFlagBits)0;
  if (_mipmap != MipmapMode::Disabled) {
    _mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(w, h)))) + 1;
    transfer_src = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;  //We need to use this image as a src transfer to fill each mip level.
  }
  else {
    _mipLevels = 1;
  }

  //Not called if we are passing in a VulaknImage alrady created.
  allocateMemory(w, h,
                 img_fmt,
                 VK_IMAGE_TILING_OPTIMAL,
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | transfer_src,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _mipLevels, samples);

  createView(img_fmt, VK_IMAGE_ASPECT_COLOR_BIT);

  //IDK
  transitionImageLayout(img_fmt, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  transitionImageLayout(img_fmt, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  createSampler();

  if (_mipmap != MipmapMode::Disabled) {
    generateMipmaps();
  }
}
VulkanTextureImage::~VulkanTextureImage() {
  vkDestroySampler(vulkan()->device(), _textureSampler, nullptr);
}
void VulkanTextureImage::createSampler() {
  //Sampler
  VkSamplerCreateInfo samplerInfo = {
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,  // VkStructureType
    .pNext = nullptr,                                // const void*
    .flags = 0,                                      // VkSamplerCreateFlags
    //Filtering
    .magFilter = VK_FILTER_LINEAR,                // VkFilter
    .minFilter = VK_FILTER_LINEAR,                // VkFilter
    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,  // VkSamplerMipmapMode
    //Texture repeat
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,   // VkSamplerAddressMode
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,   // VkSamplerAddressMode
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,   // VkSamplerAddressMode
    .mipLodBias = 0,                                  // float
    .anisotropyEnable = VK_TRUE,                      // VkBool32
    .maxAnisotropy = 16.0f,                           // float There is no graphics hardware available today that will use more than 16 samples, because the difference is negligible beyond that point.
    .compareEnable = VK_FALSE,                        // VkBool32
    .compareOp = VK_COMPARE_OP_ALWAYS,                // VkCompareOp - used for PCF
    .minLod = 0,                                      // float
    .maxLod = static_cast<float>(_mipLevels),         // float
    .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,  // VkBorderColor
    .unnormalizedCoordinates = VK_FALSE,              // VkBool32  [0,width] vs [0,1]
  };
  CheckVKR(vkCreateSampler, vulkan()->device(), &samplerInfo, nullptr, &_textureSampler);
}
VkSampler VulkanTextureImage::sampler() { return _textureSampler; }

bool VulkanTextureImage::mipmappingSupported() {
  VkFormatProperties formatProperties;
  vkGetPhysicalDeviceFormatProperties(vulkan()->physicalDevice(), format(), &formatProperties);
  bool supported = formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
  return supported;
}
void VulkanTextureImage::recreateMipmaps(MipmapMode mipmaps) {
}
void VulkanTextureImage::generateMipmaps() {
  //https://vulkan-tutorial.com/Generating_Mipmaps
  if (!mipmappingSupported()) {
    BRLogWarnOnce("Mipmapping is not supported.");
    return;
  }
  std::unique_ptr<VulkanCommands> cmds = std::make_unique<VulkanCommands>(vulkan());
  cmds->begin();

  int32_t last_level_width = static_cast<int32_t>(_width);
  int32_t last_level_height = static_cast<int32_t>(_height);
  for (uint32_t iMipLevel = 1; iMipLevel < this->_mipLevels; ++iMipLevel) {
    int32_t level_width = last_level_width / 2;
    int32_t level_height = last_level_height / 2;

    cmds->blitImage(_image,
                    _image,
                    { 0, 0, static_cast<int32_t>(last_level_width), static_cast<int32_t>(last_level_height) },
                    { 0, 0, static_cast<int32_t>(level_width), static_cast<int32_t>(level_height) },
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    iMipLevel - 1, iMipLevel,
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    (_mipmap == MipmapMode::Nearest) ? (VK_FILTER_NEAREST) : (VK_FILTER_LINEAR));
    cmds->imageTransferBarrier(_image,
                               VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               iMipLevel - 1, VK_IMAGE_ASPECT_COLOR_BIT);
    cmds->imageTransferBarrier(_image,
                               VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                               iMipLevel - 1, VK_IMAGE_ASPECT_COLOR_BIT);
    if (level_width > 1) {
      last_level_width /= 2;
    }
    if (level_height > 1) {
      last_level_height /= 2;
    }
  }
  //Transfer the last mip level to shader optimal
  cmds->imageTransferBarrier(_image,
                             VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             _mipLevels - 1, VK_IMAGE_ASPECT_COLOR_BIT);
  cmds->end();
}

void VulkanTextureImage::copyImageToGPU(std::shared_ptr<Img32> pimg, VkFormat img_fmt) {
  //**Note this assumes a color texture: see  VK_IMAGE_ASPECT_COLOR_BIT
  //For loaded images only.
  _host = std::make_shared<VulkanDeviceBuffer>(vulkan(),
                                               pimg->data_len_bytes,
                                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  _host->copy_host(pimg->_data, pimg->data_len_bytes, 0, 0, pimg->data_len_bytes);

  //Undefined layout will be discard image data.
  transitionImageLayout(img_fmt, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  copyBufferToImage(pimg);
  transitionImageLayout(img_fmt, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  //Cleanup host buffer. We are done with it.
  _host = nullptr;
}
void VulkanTextureImage::copyBufferToImage(std::shared_ptr<Img32> pimg) {
  //todo:
  //GraphicsCommands g
  //g->copyBufferToImage(..)

  auto commandBuffer = vulkan()->beginOneTimeGraphicsCommands();
  VkBufferImageCopy region = {
    .bufferOffset = 0,       //VkDeviceSize
    .bufferRowLength = 0,    //uint32_t
    .bufferImageHeight = 0,  //uint32_t
    .imageSubresource = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,          //VkImageAspectFlags
      .mipLevel = 0,                                    //uint32_t
      .baseArrayLayer = 0,                              //uint32_t
      .layerCount = 1,                                  //uint32_t
    },                                                  //VkImageSubresourceLayers
    .imageOffset = { 0, 0, 0 },                         //VkOffset3D
    .imageExtent = { pimg->_width, pimg->_height, 1 },  //VkExtent3D
  };
  vkCmdCopyBufferToImage(commandBuffer, _host->buffer(), _image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  vulkan()->endOneTimeGraphicsCommands(commandBuffer);
}
void VulkanTextureImage::transitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
  auto commandBuffer = vulkan()->beginOneTimeGraphicsCommands();
  //https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#synchronization-access-types-supported

  VkAccessFlagBits srcAccessMask;
  VkAccessFlagBits dstAccessMask;
  VkPipelineStageFlagBits srcStage;
  VkPipelineStageFlagBits dstStage;

  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    srcAccessMask = (VkAccessFlagBits)0;
    dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;  // Pseudo-stage
  }
  else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  }
  else {
    throw std::invalid_argument("unsupported layout transition!");
  }

  VkImageMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,  //VkStructureType
    .pNext = nullptr,                                 //const void*
    .srcAccessMask = (VkAccessFlags)srcAccessMask,    //VkAccessFlags
    .dstAccessMask = (VkAccessFlags)dstAccessMask,    //VkAccessFlags
    .oldLayout = oldLayout,                           //VkImageLayout
    .newLayout = newLayout,                           //VkImageLayout
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,   //uint32_t
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,   //uint32_t
    .image = _image,                                  //VkImage
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,  //VkImageAspectFlags
      .baseMipLevel = 0,                        //uint32_t
      .levelCount = _mipLevels,                 //uint32_t
      .baseArrayLayer = 0,                      //uint32_t
      .layerCount = 1,                          //uint32_t
    },                                          //VkImageSubresourceRange
  };
  vkCmdPipelineBarrier(commandBuffer,
                       srcStage, dstStage,  //Pipeline stages
                       0,
                       0, nullptr,
                       0, nullptr,
                       1, &barrier);
  //OldLayout can be VK_IMAGE_LAYOUT_UNDEFINED and the image contents are discarded when the image transition occurs
  vulkan()->endOneTimeGraphicsCommands(commandBuffer);
}
void VulkanTextureImage::flipImage20161206(uint8_t* image, int width, int height) {
  int rowSiz = width * 4;

  uint8_t* rowTmp1 = new uint8_t[rowSiz];
  int h2 = height / 2;

  for (int j = 0; j < h2; ++j) {
    uint8_t* ptRowDst = image + rowSiz * j;
    uint8_t* ptRowSrc = image + rowSiz * (height - j - 1);

    memcpy(rowTmp1, ptRowDst, rowSiz);
    memcpy(ptRowDst, ptRowSrc, rowSiz);
    memcpy(ptRowSrc, rowTmp1, rowSiz);
  }

  delete[] rowTmp1;
}

#pragma endregion

#pragma region ShaderModule

class ShaderModule::ShaderModule_Internal : VulkanObject {
public:
  string_t _name = "*unset*";
  VkShaderModule _vkShaderModule = nullptr;
  SpvReflectShaderModule* _spvReflectModule = nullptr;
  string_t _baseName = "*unset*";

  ShaderModule_Internal(std::shared_ptr<Vulkan> pv) : VulkanObject(pv) {
  }
  ~ShaderModule_Internal() {
    if (_spvReflectModule) {
      spvReflectDestroyShaderModule(_spvReflectModule);
    }
    vkDestroyShaderModule(vulkan()->device(), _vkShaderModule, nullptr);
  }
  void load(const string_t& file) {
    auto ch = Gu::readFile(file);
    createShaderModule(ch);
  }
  void createShaderModule(const std::vector<char>& code) {
    if (_vkShaderModule != VK_NULL_HANDLE) {
      BRLogWarn("Shader was initialized when creating new shader.");
      vkDestroyShaderModule(vulkan()->device(), _vkShaderModule, nullptr);
    }
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    BRLogInfo("Creating shader : " + code.size() + " bytes.");
    CheckVKR(vkCreateShaderModule, vulkan()->device(), &createInfo, nullptr, &_vkShaderModule);

    //Get Metadata
    _spvReflectModule = (SpvReflectShaderModule*)malloc(sizeof(SpvReflectShaderModule));
    SpvReflectResult result = spvReflectCreateShaderModule(code.size(), code.data(), _spvReflectModule);
    if (result != SPV_REFLECT_RESULT_SUCCESS) {
      BRThrowException("Spv-Reflect failed to parse shader.");
    }
  }
  VkPipelineShaderStageCreateInfo getPipelineStageCreateInfo() {
    VkShaderStageFlagBits type;

    if (_spvReflectModule->shader_stage & SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT) {
      type = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    else if (_spvReflectModule->shader_stage & SPV_REFLECT_SHADER_STAGE_VERTEX_BIT) {
      type = VK_SHADER_STAGE_VERTEX_BIT;
    }
    else if (_spvReflectModule->shader_stage & SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT) {
      type = VK_SHADER_STAGE_GEOMETRY_BIT;
    }

    VkPipelineShaderStageCreateInfo stage = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,  //VkStructureType
      .pNext = nullptr,                                              //const void*
      .flags = (VkPipelineShaderStageCreateFlags)0,                  //VkPipelineShaderStageCreateFlags
      .stage = type,                                                 //VkShaderStageFlagBits
      .module = _vkShaderModule,                                     //VkShaderModule
      .pName = _spvReflectModule->entry_point_name,                  //const char*
      .pSpecializationInfo = nullptr,                                //const VkSpecializationInfo*
    };
    return stage;
  }
};
SpvReflectShaderModule* ShaderModule::reflectionData() { return _pInt->_spvReflectModule; }
ShaderModule::ShaderModule(std::shared_ptr<Vulkan> v, const string_t& base_name, const string_t& files) : VulkanObject(v) {
  _pInt = std::make_unique<ShaderModule_Internal>(v);
  _pInt->_baseName = base_name;
  _pInt->load(files);
}
ShaderModule::~ShaderModule() {
  _pInt = nullptr;
}
VkPipelineShaderStageCreateInfo ShaderModule::getPipelineStageCreateInfo() {
  return _pInt->getPipelineStageCreateInfo();
}
const string_t& ShaderModule::name() {
  return _pInt->_name;
}

#pragma endregion

#pragma region OutputDescription

FBOType OutputDescription::outputTypeToFBOType(OutputMRT out) {
  if (out == OutputMRT::RT_Undefined) {
    return FBOType::Color;
  }
  if (out == OutputMRT::RT_DefaultColor) {
    return FBOType::Color;
  }
  if (out == OutputMRT::RT_DefaultDepth) {
    return FBOType::Depth;
  }
  if (out == OutputMRT::RT_DF_Position) {
    return FBOType::Color;
  }
  if (out == OutputMRT::RT_DF_Color) {
    return FBOType::Color;
  }
  if (out == OutputMRT::RT_DF_Depth_Plane) {
    return FBOType::Color;
  }
  if (out == OutputMRT::RT_DF_Normal) {
    return FBOType::Color;
  }
  if (out == OutputMRT::RT_DF_Pick) {
    return FBOType::Color;
  }
  return FBOType::Undefined;
}

#pragma endregion

#pragma region PassDescription

PassDescription::PassDescription(std::shared_ptr<PipelineShader> shader) {
  _shader = shader;
}
std::vector<VkClearValue> PassDescription::getClearValues() {
  std::vector<VkClearValue> clearValues;
  for (auto att : this->_outputs) {
    if (att->_clear) {
      VkClearValue cv;
      if (att->_type == FBOType::Depth) {
        cv.depthStencil = { att->_clearDepth,
                            att->_clearStencil };
      }
      else if (att->_type == FBOType::Color) {
        cv.color = {
          att->_clearColor.x,
          att->_clearColor.y,
          att->_clearColor.z,
          att->_clearColor.w
        };
      }
      else {
        _shader->renderError(std::string() + "Invalid FBO type: " + std::to_string((int)att->_type));
      }
      clearValues.push_back(cv);
    }
  }
  return clearValues;
}
void PassDescription::setOutput(std::shared_ptr<OutputDescription> output) {
  addValidOutput(output);
}
void PassDescription::setOutput(OutputMRT output_e, std::shared_ptr<VulkanTextureImage> tex,
                                BlendFunc blend, bool clear, float clear_r, float clear_g, float clear_b) {
  auto output = std::make_shared<OutputDescription>();
  output->_name = "custom_output";
  output->_texture = tex;
  output->_blending = blend;
  //**TODO: if we set a depth texture here, we need to set the correct depth/stencil clear value.
  output->_type = OutputDescription::outputTypeToFBOType(output_e);
  output->_clearColor = { clear_r, clear_g, clear_b, 1 };
  output->_clearDepth = 1;
  output->_clearStencil = 0;
  output->_output = output_e;
  output->_clear = clear;
  addValidOutput(output);
}
void PassDescription::addValidOutput(std::shared_ptr<OutputDescription> out_att) {
  AssertOrThrow2(_shader != nullptr);
  bool valid = true;
  //Find Location.
  for (auto& binding : _shader->outputBindings()) {
    if (binding->_output == OutputMRT::RT_Undefined) {
      //**If you get here then you didn't name the variable correctly in the shader.
      _shader->shaderError("Output MRT was not set for binding '" + binding->_name + "' location '" + binding->_location + "'");
      valid = false;
    }
    else if (binding->_output == out_att->_output) {
      out_att->_outputBinding = binding;
    }
  }
  if (valid) {
    _outputs.push_back(out_att);
  }
}

#pragma endregion

#pragma region FramebufferAttachment

FramebufferAttachment::FramebufferAttachment(std::shared_ptr<Vulkan> v) : VulkanObject(v) {
}
FramebufferAttachment::~FramebufferAttachment() {
  if (_outputImageView != VK_NULL_HANDLE) {
    vkDestroyImageView(vulkan()->device(), _outputImageView, nullptr);
  }
}

bool FramebufferAttachment::init(std::shared_ptr<Framebuffer> fbo, std::shared_ptr<OutputDescription> desc, const BR2::usize2& imageSize, VkImage swap_img, VkFormat swap_img_fmt) {
  _desc = desc;
  _imageSize = imageSize;

  if (imageSize.width == 0 || imageSize.height == 0) {
    return fbo->pipelineError("Invalid input image size for framebuffer attachment '" + desc->_name + "'  '" + VulkanDebug::OutputMRT_toString(desc->_output) + "'");
  }

  VkImageAspectFlagBits aspect;
  if (_desc->_type == FBOType::Color) {
    aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  }
  else if (_desc->_type == FBOType::Depth) {
    aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
  }
  else {
    return fbo->pipelineError("Unsupported FBOType enum: '" + std::to_string((int)_desc->_type) + "'");
  }
  //If texture, what mipmap levels then
  _outputImageView = vulkan()->createImageView(swap_img, swap_img_fmt, aspect, 1);
  return true;
}

#pragma endregion

#pragma region Framebuffer

Framebuffer::Framebuffer(std::shared_ptr<Vulkan> v) : VulkanObject(v) {
}
Framebuffer::~Framebuffer() {
  _attachments.clear();
  vkDestroyFramebuffer(vulkan()->device(), _framebuffer, nullptr);
}
bool Framebuffer::pipelineError(const string_t& msg) {
  BRLogError(msg);
  _bValid = false;
  return false;
}
bool Framebuffer::create(std::shared_ptr<RenderFrame> frame, std::shared_ptr<PassDescription> desc) {
  _frame = frame;
  _passDescription = desc;

  createAttachments();

  if (_attachments.size() == 0) {
    return pipelineError("No framebuffer attachments supplied to Framebuffer::create");
  }

  createRenderPass(getThis<Framebuffer>());

  uint32_t w = _frame->imageSize().width;
  uint32_t h = _frame->imageSize().height;

  std::vector<VkImageView> vk_attachments;
  for (auto att : _attachments) {
    vk_attachments.push_back(att->getVkImageView());
  }

  VkFramebufferCreateInfo framebufferInfo = {
    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,               //VkStructureType
    .pNext = nullptr,                                                 //const void*
    .flags = 0,                                                       //VkFramebufferCreateFlags
    .renderPass = _renderPass,                                        //VkRenderPass
    .attachmentCount = static_cast<uint32_t>(vk_attachments.size()),  //uint32_t
    .pAttachments = vk_attachments.data(),                            //const VkImageView*
    .width = w,                                                       //uint32_t
    .height = h,                                                      //uint32_t
    .layers = 1,                                                      //uint32_t
  };

  CheckVKR(vkCreateFramebuffer, vulkan()->device(), &framebufferInfo, nullptr, &_framebuffer);

  //Validate image sizes
  int32_t def_w = -1;
  int32_t def_h = -1;
  for (size_t i = 0; i < attachments().size(); ++i) {
    if (def_w == -1) {
      def_w = attachments()[i]->imageSize().width;
    }
    if (def_h == -1) {
      def_h = attachments()[i]->imageSize().height;
    }
    if (attachments()[i]->imageSize().width != def_w) {
      return pipelineError("Output FBO " + std::to_string(i) + " width '" +
                           std::to_string(attachments()[i]->imageSize().width) +
                           "' did not match first FBO width '" + std::to_string(def_w) + "'.");
    }
    if (attachments()[i]->imageSize().height != def_h) {
      return pipelineError("Output FBO " + std::to_string(i) + " height '" +
                           std::to_string(attachments()[i]->imageSize().height) +
                           "' did not match first FBO height '" + std::to_string(def_h) + "'.");
    }
  }

  if (def_w == -1 || def_h == -1) {
    return pipelineError("Invalid FBO image size.");
  }

  return true;
}
bool Framebuffer::createAttachments() {
  //Create attachments

  for (size_t i = 0; i < _passDescription->outputs().size(); ++i) {
    auto out_att = _passDescription->outputs()[i];
    VkImage img_img;
    VkFormat img_fmt;

    if (!getOutputImageDataForMRTType(_frame, out_att, img_img, img_fmt)) {
      return false;
    }

    uint32_t location = 0;
    if (out_att->_outputBinding == nullptr) {
      return pipelineError("Failed to find output binding for ShaderOutput '" + out_att->_name + "'");
    }
    else {
      location = out_att->_outputBinding->_location;
    }

    auto swap_siz = _frame->imageSize();  //Change for Dynamic MRT's

    auto fb_a = std::make_shared<FramebufferAttachment>(vulkan());

    if (!fb_a->init(getThis<Framebuffer>(), out_att, swap_siz, img_img, img_fmt)) {
      return pipelineError("Failed to initialize fbo attachment '" + std::to_string(i) + "'.");
    }

    _attachments.push_back(fb_a);
  }

  return true;
}
bool Framebuffer::createRenderPass(std::shared_ptr<Framebuffer> fbo) {
  //using subpassLoad you can read previous subpass values. This is more efficient than the old approach.
  //https://www.saschawillems.de/blog/2018/07/19/vulkan-input-attachments-and-sub-passes/
  //https://github.com/KhronosGroup/GLSL/blob/master/extensions/khr/GL_KHR_vulkan_glsl.txt
  //FOr now we just use this for work.
  std::vector<VkAttachmentDescription> attachments;
  std::vector<VkAttachmentReference> colorAttachmentRefs;

  VkAttachmentReference* depthRefPtr = nullptr;
  VkAttachmentReference depthAttachmentRef;
  for (size_t iatt = 0; iatt < _passDescription->outputs().size(); ++iatt) {
    auto odesc = _passDescription->outputs()[iatt];

    VkAttachmentLoadOp loadOp;
    if (odesc->_clear == false) {
      loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    }
    else {
      loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    }

    //Clear Values
    if (odesc->_type == FBOType::Color) {
      VkImageLayout finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      if (odesc->_isSwapchainColorImage) {
        finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
      }
      attachments.push_back({
        .flags = 0,
        .format = odesc->_outputBinding->_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = loadOp,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = finalLayout,  //** This needs to be changed for Deferred MRTs
      });
      colorAttachmentRefs.push_back({
        .attachment = odesc->_outputBinding->_location,  //layout (location=x)
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      });
    }
    else if (odesc->_type == FBOType::Depth) {
      if (depthRefPtr != nullptr) {
        return fbo->pipelineError("Multiple Renderbuffer depth buffers found in shader Output FBOs");
      }
      //TODO: testing - testing using a renderbuffer
      VkAttachmentDescription depthStencilAttachment = {
        .flags = 0,
        .format = odesc->_outputBinding->_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = loadOp,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,  // We're heavily reusing the depth buffer, right? VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,  //** This needs to be changed for Deferred MRTs
      };
      attachments.push_back(depthStencilAttachment);
      depthAttachmentRef = {
        //Layout location of depth buffer is +1 the last location.
        .attachment = odesc->_outputBinding->_location,  //layout (location=x)
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      };
      depthRefPtr = &depthAttachmentRef;
    }
    else {
      BRThrowNotImplementedException();
    }
  }

  //TODO: implement "pixel local load operations" for deferred FBOs.
  VkSubpassDescription subpass = {
    .flags = 0,
    .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
    .inputAttachmentCount = 0,
    .pInputAttachments = nullptr,
    .colorAttachmentCount = static_cast<uint32_t>(colorAttachmentRefs.size()),
    .pColorAttachments = colorAttachmentRefs.data(),
    .pResolveAttachments = nullptr,
    .pDepthStencilAttachment = depthRefPtr,  // TODO
    .preserveAttachmentCount = 0,
    .pPreserveAttachments = nullptr,
  };
  VkRenderPassCreateInfo renderPassInfo = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .attachmentCount = static_cast<uint32_t>(attachments.size()),
    .pAttachments = attachments.data(),
    .subpassCount = 1,
    .pSubpasses = &subpass,
    .dependencyCount = 0,
    .pDependencies = nullptr,
  };
  CheckVKR(vkCreateRenderPass, vulkan()->device(), &renderPassInfo, nullptr, &_renderPass);

  return true;
}
bool Framebuffer::getOutputImageDataForMRTType(std::shared_ptr<RenderFrame> frame,
                                               std::shared_ptr<OutputDescription> out_att, VkImage& out_image, VkFormat& out_format) {
  //Convert an MRT bind point into an image descriptor.

  OutputMRT type = out_att->_output;

  if (type == OutputMRT::RT_DefaultColor) {
    //Default framebuffer
    out_image = frame->swapImage();
    out_format = frame->imageFormat();
  }
  else if (type == OutputMRT::RT_DefaultDepth) {
    //Default renderbuffer
    out_image = frame->depthImage();
    out_format = frame->depthFormat();
  }
  else {
    out_image = out_att->_texture->image();
    out_format = out_att->_texture->format();
  }

  return true;
}
const BR2::usize2& Framebuffer::imageSize() {
  AssertOrThrow2(_frame != nullptr);
  return _frame->imageSize();
}

#pragma endregion

#pragma region Pipeline

Pipeline::Pipeline(std::shared_ptr<Vulkan> v,
                   VkPrimitiveTopology topo,
                   VkPolygonMode mode) : VulkanObject(v) {
  _primitiveTopology = topo;
  _polygonMode = mode;
}
bool Pipeline::init(std::shared_ptr<PipelineShader> shader,
                    std::shared_ptr<BR2::VertexFormat> vtxFormat,
                    std::shared_ptr<Framebuffer> pfbo) {
  _fbo = pfbo;

  //Pipeline Layout
  auto descriptorLayout = shader->getVkDescriptorSetLayout();
  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    //These are the buffer descriptors
    .setLayoutCount = 1,
    .pSetLayouts = &descriptorLayout,
    //Constants to pass to shaders.
    .pushConstantRangeCount = 0,
    .pPushConstantRanges = nullptr
  };
  CheckVKR(vkCreatePipelineLayout, vulkan()->device(), &pipelineLayoutInfo, nullptr, &_pipelineLayout);

  //Blending
  std::vector<VkPipelineColorBlendAttachmentState> attachmentBlending;
  for (auto& att : pfbo->passDescription()->outputs()) {
    //https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkGraphicsPipelineCreateInfo.html
    //attachmentCount member of pColorBlendState must be equal to the colorAttachmentCount used to create subpass
    //No depth attachments here
    if (att->_type == FBOType::Color) {
      VkPipelineColorBlendAttachmentState cba{};

      if (att->_blending == BlendFunc::Disabled) {
        cba.blendEnable = VK_FALSE;
      }
      else if (att->_blending == BlendFunc::AlphaBlend) {
        cba.blendEnable = VK_TRUE;
        cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        cba.colorBlendOp = VK_BLEND_OP_ADD;
        cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        cba.alphaBlendOp = VK_BLEND_OP_ADD;
        cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
      }
      else {
        shader->shaderError("Unhandled _blending state '" + std::to_string((int)att->_blending) + "' set on ShaderOutput");
      }
      attachmentBlending.push_back(cba);
    }
  }
  VkPipelineColorBlendStateCreateInfo colorBlending = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .logicOpEnable = VK_FALSE,
    //*This is the blending state PER FBO attachment and corresponds to the FBO array.
    .attachmentCount = static_cast<uint32_t>(attachmentBlending.size()),
    .pAttachments = attachmentBlending.data(),
  };

  //Shader Stages
  std::vector<VkPipelineShaderStageCreateInfo> shaderStages = shader->getShaderStageCreateInfos();

  //Vertex Descriptor
  auto vertexInputInfo = shader->getVertexInputInfo(vtxFormat);

  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .topology = _primitiveTopology,
    .primitiveRestartEnable = VK_FALSE,
  };
  //Pipeline
  VkPipelineViewportStateCreateInfo viewportState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .viewportCount = 0,
    .pViewports = nullptr,  //Viewport is dynamic
    .scissorCount = 0,
    .pScissors = nullptr,
  };
  std::shared_ptr<OutputDescription> depth = nullptr;
  for (size_t io = 0; io < pfbo->passDescription()->outputs().size(); ++io) {
    if (pfbo->passDescription()->outputs()[io]->_output == OutputMRT::RT_DefaultDepth) {
      depth = pfbo->passDescription()->outputs()[io];
      break;
    }
  }

  VkPipelineDepthStencilStateCreateInfo depthStencil;
  VkPipelineDepthStencilStateCreateInfo* depthStencilPtr = nullptr;
  if (depth) {
    VkCompareOp depthCompare = VK_COMPARE_OP_LESS;
    if (depth->_compareOp == CompareOp::Never) {
      depthCompare = VK_COMPARE_OP_NEVER;
    }
    else if (depth->_compareOp == CompareOp::Less) {
      depthCompare = VK_COMPARE_OP_LESS;
    }
    else if (depth->_compareOp == CompareOp::Equal) {
      depthCompare = VK_COMPARE_OP_EQUAL;
    }
    else if (depth->_compareOp == CompareOp::Less_Or_Equal) {
      depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL;
    }
    else if (depth->_compareOp == CompareOp::Greater) {
      depthCompare = VK_COMPARE_OP_GREATER;
    }
    else if (depth->_compareOp == CompareOp::Not_Equal) {
      depthCompare = VK_COMPARE_OP_NOT_EQUAL;
    }
    else if (depth->_compareOp == CompareOp::Greater_or_Equal) {
      depthCompare = VK_COMPARE_OP_GREATER_OR_EQUAL;
    }
    else if (depth->_compareOp == CompareOp::CompareAlways) {
      depthCompare = VK_COMPARE_OP_ALWAYS;
    }
    else {
      return shader->shaderError("Invalid depth compare func '" + std::to_string((int)depth->_compareOp) + "'.");
    }

    depthStencil = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
      .depthCompareOp = depthCompare,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE,
      .front = {},
      .back = {},
      .minDepthBounds = 0,
      .maxDepthBounds = 1,
    };
    depthStencilPtr = &depthStencil;
  }
  VkPipelineRasterizationStateCreateInfo rasterizer = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = _polygonMode,
    .cullMode = VK_CULL_MODE_BACK_BIT,
    .frontFace = VK_FRONT_FACE_CLOCKWISE,
    .depthBiasEnable = VK_FALSE,
    .depthBiasConstantFactor = 0,
    .depthBiasClamp = 0,
    .depthBiasSlopeFactor = 0,
    .lineWidth = 1,
  };
  std::vector<VkDynamicState> dynamicStates = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
    VK_DYNAMIC_STATE_LINE_WIDTH,  //Not always supported afaik.
  };
  VkPipelineDynamicStateCreateInfo dynamicState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
    .pDynamicStates = dynamicStates.data(),
  };
  VkPipelineMultisampleStateCreateInfo multisampling = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,

    //**Note:
    //I think this is only available if multisampling is enabled.
    //OpenGL: glEnable(GL_SAMPLE_SHADING); glMinSampleShading(0.2f); glGet..(GL_MIN_SAMPLE_SHADING)
    .sampleShadingEnable = VK_FALSE,  //(VkBool32)(g_samplerate_shading ? VK_TRUE : VK_FALSE),
    .minSampleShading = 1.0f,
    .pSampleMask = nullptr,
    .alphaToCoverageEnable = false,
    .alphaToOneEnable = false,
  };
  VkGraphicsPipelineCreateInfo pipelineInfo = {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .stageCount = static_cast<uint32_t>(shaderStages.size()),
    .pStages = shaderStages.data(),
    .pVertexInputState = &vertexInputInfo,
    .pInputAssemblyState = &inputAssembly,
    .pTessellationState = nullptr,
    .pViewportState = &viewportState,
    .pRasterizationState = &rasterizer,
    .pMultisampleState = &multisampling,
    .pDepthStencilState = depthStencilPtr,
    .pColorBlendState = &colorBlending,
    .pDynamicState = &dynamicState,
    .layout = _pipelineLayout,
    .renderPass = _fbo->getVkRenderPass(),
    .subpass = 0,
    .basePipelineHandle = VK_NULL_HANDLE,
    .basePipelineIndex = -1,
  };

  CheckVKR(vkCreateGraphicsPipelines, vulkan()->device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_pipeline);

  return true;
}
Pipeline::~Pipeline() {
  vkDestroyPipeline(vulkan()->device(), _pipeline, nullptr);
  vkDestroyPipelineLayout(vulkan()->device(), _pipelineLayout, nullptr);
}

#pragma endregion

#pragma region PipelineShader

std::shared_ptr<PipelineShader> PipelineShader::create(std::shared_ptr<Vulkan> v, const string_t& name, const std::vector<string_t>& files) {
  std::shared_ptr<PipelineShader> s = std::make_shared<PipelineShader>(v, name, files);
  s->init();
  s->vulkan()->swapchain()->registerShader(s);
  return s;
}
PipelineShader::PipelineShader(std::shared_ptr<Vulkan> v, const string_t& name, const std::vector<string_t>& files) : VulkanObject(v) {
  _name = name;
  _files = files;
}
PipelineShader::~PipelineShader() {
  cleanupDescriptors();
  _modules.clear();
}
bool PipelineShader::init() {
  for (auto& str : _files) {
    std::shared_ptr<ShaderModule> mod = std::make_shared<ShaderModule>(vulkan(), _name, str);
    _modules.push_back(mod);
  }
  if (!checkGood()) {
    return false;
  }
  if (!createInputs()) {
    return false;
  }
  if (!createOutputs()) {
    return false;
  }
  if (!createDescriptors()) {
    return false;
  }
  return true;
}
VkShaderStageFlagBits spvReflectShaderStageFlagBitsToVk(SpvReflectShaderStageFlagBits b) {
  if (b == SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT) {
    return VK_SHADER_STAGE_FRAGMENT_BIT;
  }
  else if (b == SPV_REFLECT_SHADER_STAGE_VERTEX_BIT) {
    return VK_SHADER_STAGE_VERTEX_BIT;
  }
  else if (b == SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT) {
    return VK_SHADER_STAGE_GEOMETRY_BIT;
  }
  else if (b == SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT) {
    return VK_SHADER_STAGE_COMPUTE_BIT;
  }
  else {
    BRThrowException("Unsupported Spv->Vk shader stage conversion.");
  }
}
std::shared_ptr<ShaderModule> PipelineShader::getModule(VkShaderStageFlagBits stage, bool throwIfNotFound) {
  std::shared_ptr<ShaderModule> mod = nullptr;
  for (auto module : _modules) {
    auto vks = spvReflectShaderStageFlagBitsToVk(module->reflectionData()->shader_stage);
    if (vks == stage) {
      mod = module;
      break;
    }
  }
  if (mod == nullptr && throwIfNotFound) {
    BRThrowException("Could not find vertex shader module for shader '" + name() + "'");
  }

  return mod;
}
bool PipelineShader::checkGood() {
  std::shared_ptr<ShaderModule> vert = getModule(VK_SHADER_STAGE_VERTEX_BIT);
  auto maxinputs = vulkan()->deviceProperties().limits.maxVertexInputAttributes;
  if (vert->reflectionData()->input_variable_count >= maxinputs) {
    return shaderError("Error creating shader '" + name() + "' - too many input variables");
  }

  std::shared_ptr<ShaderModule> frag = getModule(VK_SHADER_STAGE_FRAGMENT_BIT);
  auto maxAttachments = vulkan()->deviceProperties().limits.maxFragmentOutputAttachments;
  if (frag->reflectionData()->output_variable_count >= maxAttachments) {
    return shaderError("Error creating shader '" + name() + "' - too many output attachments in fragment shader.");
  }

  //TODO: geom
  std::shared_ptr<ShaderModule> geom = getModule(VK_SHADER_STAGE_GEOMETRY_BIT);
  return true;
}
bool PipelineShader::createInputs() {
  std::shared_ptr<ShaderModule> mod = getModule(VK_SHADER_STAGE_VERTEX_BIT, true);

  auto maxinputs = vulkan()->deviceProperties().limits.maxVertexInputAttributes;
  auto maxbindings = vulkan()->deviceProperties().limits.maxVertexInputBindings;

  if (mod->reflectionData()->input_variable_count >= maxinputs) {
    return shaderError("Error creating shader '" + name() + "' - too many input variables");
  }

  int iBindingIndex = 0;
  size_t iOffset = 0;
  for (uint32_t ii = 0; ii < mod->reflectionData()->input_variable_count; ++ii) {
    auto& iv = mod->reflectionData()->input_variables[ii];
    std::shared_ptr<VertexAttribute> attrib = std::make_shared<VertexAttribute>();
    attrib->_name = std::string(iv.name);

    //Attrib Size
    attrib->_componentCount = iv.numeric.vector.component_count;
    attrib->_componentSizeBytes = iv.numeric.scalar.width / 8;
    attrib->_matrixSize = iv.numeric.matrix.column_count * iv.numeric.matrix.row_count;
    attrib->_totalSizeBytes = (attrib->_componentCount + attrib->_matrixSize) * attrib->_componentSizeBytes;

    if ((iv.numeric.matrix.column_count != iv.numeric.matrix.row_count)) {
      return shaderError("Failure - non-square matrix dimensions for vertex attribute '" + attrib->_name + "' in shader '" + name() + "'");
    }
    else if ((iv.numeric.matrix.column_count > 0) &&
             (iv.numeric.matrix.column_count != 2) &&
             (iv.numeric.matrix.column_count != 3) &&
             (iv.numeric.matrix.column_count != 4)) {
      return shaderError("Failure - invalid matrix dimensions for vertex attribute '" + attrib->_name + "' in shader '" + name() + "'");
    }
    else if (iv.numeric.matrix.stride > 0) {
      return shaderError("Failure - nonzero stride for matrix vertex attribute '" + attrib->_name + "' in shader '" + name() + "'");
    }

    if (attrib->_matrixSize > 0 && attrib->_componentCount > 0) {
      return shaderError("Failure - matrix and vector dimensions present in attribute '" + attrib->_name + "' in shader '" + name() + "'");
    }

    //Attrib type.
    //Note type_description.typeFlags is the int,scal,mat type.
    attrib->_typeFlags = iv.type_description->type_flags;
    attrib->_userType = parseUserType(attrib->_name);
    attrib->_desc.binding = 0;
    attrib->_desc.location = iBindingIndex;
    attrib->_desc.format = spvReflectFormatToVulkanFormat(iv.format);
    attrib->_desc.offset = static_cast<uint32_t>(iOffset);  // Default offset, for an exact-match vertex.

    if (attrib->_userType == BR2::VertexUserType::gl_InstanceID || attrib->_userType == BR2::VertexUserType::gl_InstanceIndex) {
      _bInstanced = true;
    }
    else {
      iOffset += attrib->_totalSizeBytes;
      iBindingIndex++;
      _attributes.push_back(attrib);
    }
  }

  //I don't believe inputs are std430 aligned, however ..
  // We need to create a new pipeline per new vertex input via the specification.
  uint32_t size = 0;
  _attribDescriptions.clear();
  for (auto& attr : _attributes) {
    _attribDescriptions.push_back(attr->_desc);
    size += static_cast<uint32_t>(attr->_totalSizeBytes);
  }

  VkVertexInputRate inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  if (_bInstanced) {
    //**This doesn't seem to make a difference on my Nvidia 1660 driver
    //inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    //Test frame rate with this option..?
    BRLogWarn("VK_VERTEX_INPUT_RATE_INSTANCE Not supported");
  }
  _bindingDesc = {
    .binding = 0,
    .stride = size,
    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };
  return true;
}
VkFormat PipelineShader::spvReflectFormatToVulkanFormat(SpvReflectFormat in_fmt) {
  VkFormat fmt = VK_FORMAT_UNDEFINED;
#define CpySpvReflectFmtToVulkanFmt(x)    \
  if (in_fmt == SPV_REFLECT_FORMAT_##x) { \
    fmt = VK_FORMAT_##x;                  \
  }
  CpySpvReflectFmtToVulkanFmt(R32_SFLOAT);
  CpySpvReflectFmtToVulkanFmt(R32G32_SFLOAT);
  CpySpvReflectFmtToVulkanFmt(R32G32B32_SFLOAT);
  CpySpvReflectFmtToVulkanFmt(R32G32B32A32_SFLOAT);
  CpySpvReflectFmtToVulkanFmt(R32_UINT);
  CpySpvReflectFmtToVulkanFmt(R32G32_UINT);
  CpySpvReflectFmtToVulkanFmt(R32G32B32_UINT);
  CpySpvReflectFmtToVulkanFmt(R32G32B32A32_UINT);
  CpySpvReflectFmtToVulkanFmt(R32_SINT);
  CpySpvReflectFmtToVulkanFmt(R32G32_SINT);
  CpySpvReflectFmtToVulkanFmt(R32G32B32_SINT);
  CpySpvReflectFmtToVulkanFmt(R32G32B32A32_SINT);

  return fmt;
}
OutputMRT PipelineShader::parseShaderOutputTag(const string_t& tag) {
#define PARSOUTTAG(x)                                              \
  if (StringUtil::equals(tag, ShaderOutputBinding::_outFBO_##x)) { \
    return OutputMRT::RT_##x;                                      \
  }
  PARSOUTTAG(DefaultColor);
  PARSOUTTAG(DefaultDepth);
  PARSOUTTAG(DF_Position);
  PARSOUTTAG(DF_Color);
  PARSOUTTAG(DF_Depth_Plane);
  PARSOUTTAG(DF_Normal);
  PARSOUTTAG(DF_Pick);
  PARSOUTTAG(Custom0);
  PARSOUTTAG(Custom1);
  PARSOUTTAG(Custom2);
  PARSOUTTAG(Custom3);
  PARSOUTTAG(Custom4);
  PARSOUTTAG(Custom5);
  PARSOUTTAG(Custom6);
  PARSOUTTAG(Custom7);
  PARSOUTTAG(Custom8);
  PARSOUTTAG(Custom9);
  return OutputMRT::RT_Undefined;
}
bool PipelineShader::createOutputs() {
  std::shared_ptr<ShaderModule> mod = getModule(VK_SHADER_STAGE_FRAGMENT_BIT, true);
  if (mod == nullptr) {
    return shaderError("Fragment module not found for shader '" + this->name() + "'");
  }
  uint32_t iMaxLocation = 0;
  std::vector<uint32_t> locations;
  for (uint32_t inp = 0; inp < mod->reflectionData()->output_variable_count; ++inp) {
    auto pvar = mod->reflectionData()->output_variables[inp];

    string_t name = string_t(pvar.name);
    if (StringUtil::startsWith(name, "_outFBO")) {
      auto fb = std::make_shared<ShaderOutputBinding>();
      fb->_name = name;
      fb->_location = mod->reflectionData()->output_variables[inp].location;
      fb->_output = parseShaderOutputTag(fb->_name);

      //This should really be how we handle the format.
      int type_w = pvar.type_description->traits.numeric.scalar.width;
      int c_count = pvar.type_description->traits.numeric.vector.component_count;

      fb->_format = spvReflectFormatToVulkanFormat(pvar.format);
      if (fb->_format == VK_FORMAT_R32G32B32A32_SFLOAT) {
        fb->_type = FBOType::Color;
      }
      else if (fb->_format == VK_FORMAT_R32_SFLOAT) {
        return shaderError("Depth format output from shader, this is not implemented.");
      }
      else {
        return shaderError("Unhandled shader output variable format '" + VulkanDebug::VkFormat_toString(fb->_format) + "'");
      }
      locations.push_back(fb->_location);

      _outputBindings.push_back(fb);
    }
    else {
      return shaderError("Shader - output variable was not an fbo prefixed with _outFBO - this is not supported.");
    }
  }
  //Validate sequential attachment Locations.
  uint32_t lastLoc = 0;
  for (size_t iloc = 0; iloc < locations.size(); ++iloc) {
    auto it = std::find(locations.begin(), locations.end(), (uint32_t)iloc);
    if (it == locations.end()) {
      return shaderError("Error one or more FBO locations missing '" + std::to_string(iloc) + "' - all locations to the maximum location must be filled.");
    }
  }

  uint32_t depthLoc = 0;
  auto it = std::max_element(locations.begin(), locations.end());
  if (it == locations.end()) {
    BRLogWarn("Failed to get renderbuffer location from output - setting to zero");
    depthLoc = 0;
  }
  else {
    depthLoc = *it + 1;
  }

  //Add Renderbuffer by default, ignore if not used.
  auto fb = std::make_shared<ShaderOutputBinding>();
  fb->_name = "_auto_depthBuffer";
  fb->_location = depthLoc;
  fb->_type = FBOType::Depth;
  fb->_output = OutputMRT::RT_DefaultDepth;
  fb->_format = vulkan()->findDepthFormat();
  _outputBindings.push_back(fb);

  return true;
}
BR2::VertexUserType PipelineShader::parseUserType(const string_t& zname) {
  BR2::VertexUserType ret;
  //TODO: integrate with the code in VG

  string_t name = StringUtil::trim(zname);

  //**In the old system we had a more generic approach to this, but here we just hard code it.
  if (StringUtil::equals(name, "gl_InstanceIndex")) {
    ret = BR2::VertexUserType::gl_InstanceIndex;
  }
  else if (StringUtil::equals(name, "gl_InstanceID")) {
    ret = BR2::VertexUserType::gl_InstanceID;
  }
  else if (StringUtil::equals(name, "_v201")) {
    ret = BR2::VertexUserType::v2_01;
  }
  else if (StringUtil::equals(name, "_v301")) {
    ret = BR2::VertexUserType::v3_01;
  }
  else if (StringUtil::equals(name, "_v401")) {
    ret = BR2::VertexUserType::v4_01;
  }
  else if (StringUtil::equals(name, "_v402")) {
    ret = BR2::VertexUserType::v4_02;
  }
  else if (StringUtil::equals(name, "_v403")) {
    ret = BR2::VertexUserType::v4_03;
  }
  else if (StringUtil::equals(name, "_n301")) {
    ret = BR2::VertexUserType::n3_01;
  }
  else if (StringUtil::equals(name, "_c301")) {
    ret = BR2::VertexUserType::c3_01;
  }
  else if (StringUtil::equals(name, "_c401")) {
    ret = BR2::VertexUserType::c4_01;
  }
  else if (StringUtil::equals(name, "_x201")) {
    ret = BR2::VertexUserType::x2_01;
  }
  else if (StringUtil::equals(name, "_i201")) {
    ret = BR2::VertexUserType::i2_01;
  }
  else if (StringUtil::equals(name, "_u201")) {
    ret = BR2::VertexUserType::u2_01;
  }
  else {
    //Wer're going to hit this in the beginning because we can have a lot of different attrib types.
    BRLogInfo("  Unrecognized vertex attribute '" + name + "'.");
    Gu::debugBreak();
  }
  return ret;
}
VkPipelineVertexInputStateCreateInfo PipelineShader::getVertexInputInfo(std::shared_ptr<BR2::VertexFormat> fmt) {
  //This is basically a glsl attribute specifying a layout identifier
  //So we need to match the input descriptions with the input vertex info.

  VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .vertexBindingDescriptionCount = 1,
    .pVertexBindingDescriptions = &_bindingDesc,
    .vertexAttributeDescriptionCount = static_cast<uint32_t>(_attribDescriptions.size()),
    .pVertexAttributeDescriptions = _attribDescriptions.data(),
  };

  return vertexInputInfo;
}
std::vector<VkPipelineShaderStageCreateInfo> PipelineShader::getShaderStageCreateInfos() {
  std::vector<VkPipelineShaderStageCreateInfo> ret;
  for (auto shader : _modules) {
    ret.push_back(shader->getPipelineStageCreateInfo());
  }
  return ret;
}
void PipelineShader::cleanupDescriptors() {
  vkDestroyDescriptorPool(vulkan()->device(), _descriptorPool, nullptr);
  vkDestroyDescriptorSetLayout(vulkan()->device(), _descriptorSetLayout, nullptr);
}
DescriptorFunction PipelineShader::classifyDescriptor(const string_t& name) {
  DescriptorFunction ret = DescriptorFunction::Custom;
  if (StringUtil::equals(name, "_uboViewProj")) {
    ret = DescriptorFunction::ViewProjMatrixUBO;
  }
  else if (StringUtil::equals(name, "_uboInstanceData")) {
    ret = DescriptorFunction::InstnaceMatrixUBO;
  }
  return ret;
}
bool PipelineShader::createDescriptors() {
  uint32_t _nPool_Samplers = 0;
  uint32_t _nPool_UBOs = 0;

  //Parse shader metadata
  for (auto& module : _modules) {
    for (uint32_t idb = 0; idb < module->reflectionData()->descriptor_binding_count; idb++) {
      auto& descriptor = module->reflectionData()->descriptor_bindings[idb];

      std::shared_ptr<Descriptor> d = std::make_shared<Descriptor>();

      d->_name = std::string(descriptor.name);
      if (d->_name.length() == 0) {
        BRLogWarn("Name of one or more input shader variables was not specified for shader module '" + module->name() + "'");
      }
      d->_binding = descriptor.binding;
      d->_arraySize = 1;
      d->_function = classifyDescriptor(d->_name);

      if (module->reflectionData()->shader_stage == SPV_REFLECT_SHADER_STAGE_VERTEX_BIT) {
        d->_stage = VK_SHADER_STAGE_VERTEX_BIT;
      }
      else if (module->reflectionData()->shader_stage == SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT) {
        d->_stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      }
      else if (module->reflectionData()->shader_stage == SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT) {
        d->_stage = VK_SHADER_STAGE_GEOMETRY_BIT;
      }
      else {
        return shaderError("Invalid or unsupported shader stage (SpvReflectShaderStage):  " + std::to_string(module->reflectionData()->shader_stage));
      }

      if (descriptor.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
        d->_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        _nPool_UBOs++;

        if (descriptor.array.dims_count > 0) {
          //This is an array descriptor. I don't think this is valid in Vulkan-GLSL
          return shaderError("Illegal Descriptor array was found.");
        }
        if (descriptor.block.member_count > 0) {
          //This is a block , array size = 0. The actual size of the block is viewed as a single data-chunk.s
          d->_arraySize = 1;
          //64 x 2 = 128 _uboViewProj

          //Blocksize = the size of the ENTIRE uniform buffer.
          d->_blockSize = descriptor.block.size;

          //The size of the data-members of the uniform.
          //d->structSize  = descriptor.type_description.
        }
      }
      else if (descriptor.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
        d->_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        _nPool_Samplers++;

        if (descriptor.array.dims_count > 0) {
          if (descriptor.array.dims_count > 1) {
            //This is not illegal, just not supported. Update VkDescriptorSetLayoutBinding count to _arraySize for multi-dim arrays
            return shaderError("Illegal Descriptor multi-array.");
          }
          else {
            d->_arraySize = descriptor.array.dims[0];
          }
        }
      }
      else {
        return shaderError("Shader descriptor not supported - Spirv-Reflect Descriptor: " + descriptor.descriptor_type);
      }
      _descriptors.insert(std::make_pair(d->_name, d));
    }
  }

  //Allocate Descriptor Pools.
  VkDescriptorPoolSize uboPoolSize = {
    .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = static_cast<uint32_t>(vulkan()->swapchainImageCount()) * _nPool_UBOs,
  };
  VkDescriptorPoolSize samplerPoolSize = {
    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .descriptorCount = static_cast<uint32_t>(vulkan()->swapchainImageCount()) * _nPool_Samplers,
  };
  std::array<VkDescriptorPoolSize, 2> poolSizes{ uboPoolSize, samplerPoolSize };
  VkDescriptorPoolCreateInfo poolInfo = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0 /*VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT*/,
    .maxSets = static_cast<uint32_t>(vulkan()->swapchainImageCount()),
    .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
    .pPoolSizes = poolSizes.data(),
  };
  CheckVKR(vkCreateDescriptorPool, vulkan()->device(), &poolInfo, nullptr, &_descriptorPool);

  // Create Descriptor Layouts
  std::vector<VkDescriptorSetLayoutBinding> bindings;
  for (auto& it : _descriptors) {
    auto& desc = it.second;
    bindings.push_back({
      .binding = desc->_binding,
      .descriptorType = desc->_type,
      .descriptorCount = desc->_arraySize,
      .stageFlags = desc->_stage,
      .pImmutableSamplers = nullptr,
    });
  }

  VkDescriptorSetLayoutCreateInfo layoutInfo = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .bindingCount = static_cast<uint32_t>(bindings.size()),
    .pBindings = bindings.data(),
  };
  CheckVKR(vkCreateDescriptorSetLayout, vulkan()->device(), &layoutInfo, nullptr, &_descriptorSetLayout);

  std::vector<VkDescriptorSetLayout> _layouts;
  _layouts.resize(vulkan()->swapchainImageCount(), _descriptorSetLayout);

  //Allocate Descriptor Sets
  VkDescriptorSetAllocateInfo allocInfo = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .pNext = nullptr,
    .descriptorPool = _descriptorPool,
    .descriptorSetCount = static_cast<uint32_t>(vulkan()->swapchainImageCount()),
    .pSetLayouts = _layouts.data(),
  };

  //Descriptor sets are automatically freed when the descriptor pool is destroyed.
  _descriptorSets.resize(vulkan()->swapchainImageCount());
  CheckVKR(vkAllocateDescriptorSets, vulkan()->device(), &allocInfo, _descriptorSets.data());

  return true;
}
std::shared_ptr<Descriptor> PipelineShader::getDescriptor(const string_t& name) {
  //Returns the descriptor of the given name, or nullptr if not found.
  std::shared_ptr<Descriptor> ret = nullptr;

  auto it = _descriptors.find(name);
  if (it != _descriptors.end()) {
    ret = it->second;
  }
  return ret;
}
bool PipelineShader::createUBO(const string_t& name, const string_t& var_name, unsigned long long bufsize) {
  //UBOs are persistent data.
  if (_shaderData.size() == 0) {
    return shaderError("Shader data was uninitialized when creating UBO.");
  }

  for (auto pair : _shaderData) {
    std::shared_ptr<ShaderData> data = pair.second;
    auto desc = getDescriptor(var_name);
    if (desc == nullptr) {
      return shaderError("Failed to locate UBO descriptor for shader variable '" + var_name + "'");
    }
    else if (data->_uniformBuffers.find(name) != data->_uniformBuffers.end()) {
      return shaderError("UBO for shader variable '" + var_name + "' with client variable '" + name + "' was already found in ShaderData.");
    }
    else {
      //Get the descriptor size.
      uint32_t size = (uint32_t)bufsize;
      if (bufsize == VK_WHOLE_SIZE) {
        size = desc->_blockSize;
      }
      std::shared_ptr<ShaderDataUBO> datUBO = std::make_shared<ShaderDataUBO>();
      datUBO->_buffer = std::make_shared<VulkanBuffer>(
        vulkan(),
        VulkanBufferType::UniformBuffer,
        false,  //not on GPU
        size, nullptr, 0);
      datUBO->_descriptor = desc;
      data->_uniformBuffers.insert(std::make_pair(name, datUBO));
    }
  }

  return true;
}
bool PipelineShader::bindUBO(const string_t& name, std::shared_ptr<VulkanBuffer> buffer, VkDeviceSize offset, VkDeviceSize range) {
  //Binds a shader Uniform to this shader for the given swapchain image.
  if (!beginPassGood()) {
    return false;
  }

  std::shared_ptr<Descriptor> desc = getDescriptor(name);
  if (desc == nullptr) {
    return renderError("Descriptor '" + name + "'could not be found for shader '" + this->name() + "'.");
  }

  VkDescriptorBufferInfo bufferInfo = {
    .buffer = buffer->hostBuffer()->buffer(),
    .offset = offset,
    .range = range,
  };
  VkWriteDescriptorSet descWrite = {
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .pNext = nullptr,
    .dstSet = _descriptorSets[_pBoundFrame->frameIndex()],
    .dstBinding = desc->_binding,
    .dstArrayElement = 0,
    .descriptorCount = 1,
    .descriptorType = desc->_type,
    .pImageInfo = nullptr,
    .pBufferInfo = &bufferInfo,
    .pTexelBufferView = nullptr,
  };
  vkUpdateDescriptorSets(vulkan()->device(), 1, &descWrite, 0, nullptr);

  desc->_isBound = true;

  return true;
}
bool PipelineShader::bindSampler(const string_t& name, std::shared_ptr<VulkanTextureImage> texture, uint32_t arrayIndex) {
  if (!beginPassGood()) {
    return false;
  }

  std::shared_ptr<Descriptor> desc = getDescriptor(name);
  if (desc == nullptr) {
    return renderError("Descriptor '" + name + "'could not be found for shader '" + this->name() + "'.");
  }

  VkDescriptorImageInfo imageInfo = {
    .sampler = texture->sampler(),
    .imageView = texture->imageView(),
    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
  VkWriteDescriptorSet descriptorWrite = {
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .pNext = nullptr,
    .dstSet = _descriptorSets[_pBoundFrame->frameIndex()],
    .dstBinding = desc->_binding,
    .dstArrayElement = arrayIndex,  //** Samplers are opaque and thus can be arrayed in GLSL shaders.
    .descriptorCount = 1,
    .descriptorType = desc->_type,
    .pImageInfo = &imageInfo,
    .pBufferInfo = nullptr,
    .pTexelBufferView = nullptr,
  };

  desc->_isBound = true;

  vkUpdateDescriptorSets(vulkan()->device(), 1, &descriptorWrite, 0, nullptr);
  return true;
}
std::shared_ptr<Framebuffer> PipelineShader::getOrCreateFramebuffer(std::shared_ptr<RenderFrame> frame, std::shared_ptr<ShaderData> data, std::shared_ptr<PassDescription> desc) {
  auto fbo = findFramebuffer(data, desc);
  if (fbo == nullptr) {
    //Add the FBO, if there's an error we won't keep trying to recreate it every frame.
    fbo = std::make_shared<Framebuffer>(vulkan());
    data->_framebuffers.push_back(fbo);

    if (desc->outputs().size() == 0) {
      fbo->pipelineError("No FBO outputs were specified.");
    }
    else if (!fbo->create(frame, desc)) {
      fbo->pipelineError("Failed to create FBO.");
    }
  }

  return fbo;
}
bool PipelineShader::beginRenderPass(std::shared_ptr<CommandBuffer> buf, std::shared_ptr<RenderFrame> frame, std::shared_ptr<PassDescription> input_desc, BR2::urect2* extent) {
  if (valid() == false) {
    return false;
  }
  auto sd = getShaderData(frame);
  auto fbo = getOrCreateFramebuffer(frame, sd, input_desc);
  if (!fbo->valid()) {
    return false;
  }
  else if (fbo->attachments().size() == 0) {
    return fbo->pipelineError("No output FBOs have been created.");
  }

  _pBoundFBO = fbo;
  _pBoundData = sd;
  _pBoundFrame = frame;

  std::vector<VkClearValue> clearValues = input_desc->getClearValues();

  uint32_t w = 0, h = 0, x = 0, y = 0;
  if (extent) {
    x = extent->pos.x;
    y = extent->pos.y;
    w = extent->size.width;
    h = extent->size.height;
  }
  else {
    x = 0;
    y = 0;
    w = _pBoundFBO->imageSize().width;
    h = _pBoundFBO->imageSize().height;
  }

  //This function must succeed if beginPass succeeds.
  if (!buf->beginPass() || !_pBoundFBO->valid()) {
    _pBoundFBO = nullptr;
    _pBoundData = nullptr;
    _pBoundFrame = nullptr;
    return false;
  }
  else {
    VkExtent2D outputExtent = {
      .width = static_cast<uint32_t>(w),
      .height = static_cast<uint32_t>(h)
    };
    VkRenderPassBeginInfo passBeginInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .pNext = nullptr,
      .renderPass = _pBoundFBO->getVkRenderPass(),  // **TODO: fbo->renderPass()
      .framebuffer = _pBoundFBO->getVkFramebuffer(),
      .renderArea = { .offset = VkOffset2D{ .x = 0, .y = 0 }, .extent = outputExtent },
      //Note: each attachment uses load_op_clear or not is what sets this value
      .clearValueCount = static_cast<uint32_t>(clearValues.size()),
      .pClearValues = clearValues.data(),
    };
    vkCmdBeginRenderPass(buf->getVkCommandBuffer(), &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
  }

  return true;
}
bool PipelineShader::beginPassGood() {
  //Returns false if the pass has not begun, or the data state is invalid.
  if (_pBoundFBO == nullptr) {
    return renderError("FBO was not bound calling bindDescriptors");
  }
  if (_pBoundFrame == nullptr) {
    return renderError("RenderFrame was not bound calling bindDescriptors");
  }
  if (_pBoundPipeline == nullptr) {
    return renderError("Pipeline was not bound calling bindDescriptors");
  }
  return true;
}
std::shared_ptr<Pipeline> PipelineShader::getPipeline(std::shared_ptr<BR2::VertexFormat> vertexFormat, VkPrimitiveTopology topo, VkPolygonMode mode) {
  if (_pBoundData == nullptr) {
    BRLogError("Pipeline: ShaderData was not set.");
    return nullptr;
  }
  std::shared_ptr<Pipeline> pipe = nullptr;
  for (auto the_pipe : _pBoundData->_pipelines) {
    if (the_pipe->primitiveTopology() == topo &&
        the_pipe->polygonMode() == mode &&
        the_pipe->vertexFormat() == vertexFormat &&
        _pBoundFBO == the_pipe->fbo()) {
      pipe = the_pipe;
      break;
    }
  }
  if (pipe == nullptr) {
    std::shared_ptr<BR2::VertexFormat> format = nullptr;  // ** TODO create multiple pipelines for Vertex Format, Polygonmode & Topo.
    pipe = std::make_shared<Pipeline>(vulkan(), topo, mode);
    pipe->init(getThis<PipelineShader>(), format, _pBoundFBO);
    _pBoundData->_pipelines.push_back(pipe);
  }
  return pipe;
}
bool PipelineShader::bindDescriptors(std::shared_ptr<CommandBuffer> cmd) {
  if (!beginPassGood()) {
    return false;
  }
  for (auto desc : _descriptors) {
    if (desc.second->_isBound == false) {
      BRLogWarnOnce("Descriptor '" + desc.second->_name + "' was not bound before invoking shader '" + this->name() + "'");
    }
  }

  vkCmdBindDescriptorSets(cmd->getVkCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, _pBoundPipeline->getVkPipelineLayout(),
                          0,  //Layout ID
                          1,
                          &_descriptorSets[_pBoundFrame->frameIndex()], 0, nullptr);
  return true;
}
bool PipelineShader::shaderError(const string_t& msg) {
  BRLogError(msg);
  _bValid = false;
  Gu::debugBreak();
  return false;
}
bool PipelineShader::renderError(const string_t& msg) {
  BRLogError(msg);
  //Gu::debugBreak();
  return false;
}
std::shared_ptr<VulkanBuffer> PipelineShader::getUBO(const string_t& name, std::shared_ptr<RenderFrame> frame) {
  auto sd = getShaderData(frame);
  if (sd != nullptr) {
    auto ub = sd->getUBOData(name);
    if (ub != nullptr) {
      return ub->_buffer;
    }
  }

  return nullptr;
}
std::shared_ptr<ShaderData> PipelineShader::getShaderData(std::shared_ptr<RenderFrame> frame) {
  AssertOrThrow2(frame != nullptr);
  if (_shaderData.size() <= frame->frameIndex()) {
    BRThrowException("Tried to access out of bounds shader data.");
  }
  return _shaderData[frame->frameIndex()];
}
std::shared_ptr<Framebuffer> PipelineShader::findFramebuffer(std::shared_ptr<ShaderData> data, std::shared_ptr<PassDescription> outputs) {
  //Returns an exact match on the given input PassDescription and the FBO PassDescription.
  std::shared_ptr<Framebuffer> fbo = nullptr;
  for (auto fb : data->_framebuffers) {
    if (fb->passDescription()->outputs().size() == outputs->outputs().size()) {
      bool match = true;
      for (auto io = 0; io < fb->passDescription()->outputs().size(); ++io) {
        auto fboDescOut = fb->passDescription()->outputs()[io];
        auto inDescOut = outputs->outputs()[io];

        //TODO: Find a less error-prone way to match FBOs with pipeline state.
        if (
          fboDescOut->_output != inDescOut->_output || fboDescOut->_clear != inDescOut->_clear  //This sucks, but this is because the LOAD_OP is hard set in the render pass.
        || fboDescOut->_isSwapchainColorImage != inDescOut->_isSwapchainColorImage
        ) {
          match = false;
        }
      }
      if (match == true) {
        fbo = fb;
        break;
      }
    }
  }
  return fbo;
}
bool PipelineShader::bindPipeline(std::shared_ptr<CommandBuffer> cmd, std::shared_ptr<BR2::VertexFormat> v_fmt, VkPolygonMode mode, VkPrimitiveTopology topo) {
  auto pipe = getPipeline(v_fmt, topo, mode);
  if (pipe == nullptr) {
    return renderError("Output array is not valid for pipeline.");
  }
  return bindPipeline(cmd, pipe);
}
bool PipelineShader::bindPipeline(std::shared_ptr<CommandBuffer> cmd, std::shared_ptr<Pipeline> pipe) {
  if (pipe->fbo() != _pBoundFBO) {
    return renderError("Output FBO is not bound to correct pipeline.");
  }

  vkCmdBindPipeline(cmd->getVkCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->getVkPipeline());
  _pBoundPipeline = pipe;
  return true;
}
void PipelineShader::drawIndexed(std::shared_ptr<CommandBuffer> cmd, std::shared_ptr<Mesh> m, uint32_t numInstances) {
  m->bindBuffers(cmd);
  m->drawIndexed(cmd, numInstances);
}
void PipelineShader::bindViewport(std::shared_ptr<CommandBuffer> cmd, const BR2::urect2& size) {
  cmd->cmdSetViewport(size);
}
void PipelineShader::endRenderPass(std::shared_ptr<CommandBuffer> buf) {
  _pBoundFBO = nullptr;
  _pBoundPipeline = nullptr;
  _pBoundData = nullptr;
  _pBoundFrame = nullptr;
  buf->endPass();
}
void PipelineShader::clearShaderDataCache(std::shared_ptr<RenderFrame> frame) {
  //Per-swapchain-frame shader data - recreated when window resizes.
  std::shared_ptr<ShaderData> data = nullptr;
  auto it = _shaderData.find(frame->frameIndex());
  if (it == _shaderData.end()) {
    std::shared_ptr<ShaderData> dat = std::make_shared<ShaderData>();
    _shaderData.insert(std::make_pair(frame->frameIndex(), dat));
    it = _shaderData.find(frame->frameIndex());
  }
  data = it->second;
  data->_framebuffers.clear();
  data->_pipelines.clear();
}
std::shared_ptr<PassDescription> PipelineShader::getPass(std::shared_ptr<RenderFrame> frame) {
  std::shared_ptr<PassDescription> d = std::make_shared<PassDescription>(getThis<PipelineShader>());

  //TODO: fill default outputs here with default framebuffers.
  // ** TODO: **
  //for example RT_DF_Color is stored in each RenderFrame so use it as default.

  return d;
}

#pragma endregion

#pragma region ShaderData

std::shared_ptr<ShaderDataUBO> ShaderData::getUBOData(const string_t& name) {
  auto it = _uniformBuffers.find(name);
  if (it == _uniformBuffers.end()) {
    return nullptr;
  }
  return it->second;
}

#pragma endregion

#pragma region RenderFrame

RenderFrame::RenderFrame(std::shared_ptr<Vulkan> v, std::shared_ptr<Swapchain> ps, uint32_t frameIndex, VkImage img, VkSurfaceFormatKHR fmt) : VulkanObject(v) {
  _pSwapchain = ps;
  _swapImage = img;
  _frameIndex = frameIndex;
  _imageFormat = fmt;
}
RenderFrame::~RenderFrame() {
  vkDestroySemaphore(vulkan()->device(), _imageAvailableSemaphore, nullptr);
  vkDestroySemaphore(vulkan()->device(), _renderFinishedSemaphore, nullptr);
  vkDestroyFence(vulkan()->device(), _inFlightFence, nullptr);

  _depthImage = nullptr;
}
void RenderFrame::init() {
  createSyncObjects();

  _pCommandBuffer = std::make_shared<CommandBuffer>(vulkan(), getThis<RenderFrame>());

  uint32_t w = _pSwapchain->imageSize().width;
  uint32_t h = _pSwapchain->imageSize().height;

  //**This si still an issue - binding the swapchain format to the FBO format - there could be discrpencies.
  VkFormat depthFmt = vulkan()->findDepthFormat();
  _depthImage = std::make_shared<VulkanImage>(vulkan());
  _depthImage->allocateMemory(w, h, depthFmt, VK_IMAGE_TILING_OPTIMAL,
                              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 1,
                              VK_SAMPLE_COUNT_1_BIT);
  //**DFB images.
}
void RenderFrame::createSyncObjects() {
  VkSemaphoreCreateInfo semaphoreInfo = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
  };
  CheckVKR(vkCreateSemaphore, vulkan()->device(), &semaphoreInfo, nullptr, &_imageAvailableSemaphore);
  CheckVKR(vkCreateSemaphore, vulkan()->device(), &semaphoreInfo, nullptr, &_renderFinishedSemaphore);

  VkFenceCreateInfo fenceInfo = {
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .pNext = nullptr,
    .flags = VK_FENCE_CREATE_SIGNALED_BIT,  //Fences must always be created in a signaled state.
  };

  CheckVKR(vkCreateFence, vulkan()->device(), &fenceInfo, nullptr, &_inFlightFence);
}
bool RenderFrame::beginFrame() {
  //I feel like the async aspect of RenderFrame might need to be a separate DispatchedFrame structure or..
  VkResult res;
  uint64_t wait_fences = UINT64_MAX;
  //wait_fences = 0;  //Don't wait if no image available.

  res = vkWaitForFences(vulkan()->device(), 1, &_inFlightFence, VK_TRUE, wait_fences);
  if (res != VK_SUCCESS) {
    if (res == VK_NOT_READY) {
      return false;
    }
    else if (res == VK_TIMEOUT) {
      //So this doesn't get hit on Windows for some reason.
      return false;
    }
    else if (res == VK_ERROR_DEVICE_LOST) {
      //**TODO: create new logical device from physical (if it's not lost)
      // https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#devsandqueues-lost-device
      Gu::debugBreak();
      vulkan()->errorExit("Lost device! - TODO: implement device recapture");
    }
    else {
      BRLogWarnOnce("Unhandled return code from vkWaitForFences '" + std::to_string((int)res) + "'");
    }
  }

  //The semaphore passed into vkAcquireNextImageKHR makes sure the iamge is not still being read to via the VkQueueSubmit. You must use the same semaphore for both images.
  res = vkAcquireNextImageKHR(vulkan()->device(), _pSwapchain->getVkSwapchain(), wait_fences, _imageAvailableSemaphore, VK_NULL_HANDLE, &_currentRenderingImageIndex);
  if (res != VK_SUCCESS) {
    if (res == VK_NOT_READY) {
      return false;
    }
    else if (res == VK_TIMEOUT) {
      return false;
    }
    else if (res == VK_ERROR_DEVICE_LOST) {
      //**TODO: create new logical device from physical (if it's not lost)
      // https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#devsandqueues-lost-device
      Gu::debugBreak();
      vulkan()->errorExit("Lost device! - TODO: implement device recapture");
    }
    else if (res == VK_ERROR_OUT_OF_DATE_KHR) {
      _pSwapchain->outOfDate();
      return false;
    }
    else if (res == VK_SUBOPTIMAL_KHR) {
      //This is considered success in the tutorial.
      _pSwapchain->outOfDate();
      return false;
    }
    else {
      vulkan()->validateVkResult(res, "vkAcquireNextImageKHR");
    }
  }

  _pSwapchain->waitImage(_currentRenderingImageIndex, _inFlightFence);

  _frameState = FrameState::FrameBegin;
  return true;
}
void RenderFrame::endFrame() {
  if (_frameState != FrameState::FrameBegin) {
    BRLogError("Called RenderFrame::endFrame invalid.");
    return;
  }
  //Submit the recorded command buffer.
  VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

  VkCommandBuffer buf = _pCommandBuffer->getVkCommandBuffer();

  //aquire next image
  VkSubmitInfo submitInfo = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pNext = nullptr,
    //Note: Eadch entry in waitStages corresponds to the semaphore in pWaitSemaphores - we can wait for multiple stages
    //to finish rendering, or just wait for the framebuffer output.
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &_imageAvailableSemaphore,
    .pWaitDstStageMask = waitStages,
    .commandBufferCount = 1,
    .pCommandBuffers = &buf,

    //The semaphore is signaled when the queue has completed the requested wait stages.
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = &_renderFinishedSemaphore,
  };

  vkResetFences(vulkan()->device(), 1, &_inFlightFence);
  vkQueueSubmit(vulkan()->graphicsQueue(), 1, &submitInfo, _inFlightFence);

  std::vector<VkSwapchainKHR> chains{ _pSwapchain->getVkSwapchain() };

  VkPresentInfoKHR presentinfo = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .pNext = nullptr,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &_renderFinishedSemaphore,
    .swapchainCount = static_cast<uint32_t>(chains.size()),
    .pSwapchains = chains.data(),
    .pImageIndices = &_currentRenderingImageIndex,
    .pResults = nullptr
  };
  VkResult res = vkQueuePresentKHR(vulkan()->presentQueue(), &presentinfo);
  if (res != VK_SUCCESS) {
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
      _pSwapchain->outOfDate();
      return;
    }
    else if (res == VK_SUBOPTIMAL_KHR) {
      _pSwapchain->outOfDate();
      return;
    }
    else {
      vulkan()->validateVkResult(res, "vkAcquireNextImageKHR");
    }
  }
  _frameState = FrameState::FrameEnd;
}
VkFormat RenderFrame::imageFormat() {
  return _imageFormat.format;
}
const BR2::usize2& RenderFrame::imageSize() {
  return _pSwapchain->imageSize();
}

#pragma endregion

#pragma region Swapchain

Swapchain::Swapchain(std::shared_ptr<Vulkan> v) : VulkanObject(v) {
}
Swapchain::~Swapchain() {
  cleanupSwapChain();
  _shaders.clear();
}
void Swapchain::registerShader(std::shared_ptr<PipelineShader> shader) {
  //We still need to keep shaders registered to prevent the smart pointer from deallcoating.
  if (_shaders.find(shader) == _shaders.end()) {
    _shaders.insert(shader);
  }
  for (auto frame : frames()) {
    shader->clearShaderDataCache(frame);
  }
}
void Swapchain::initSwapchain(const BR2::usize2& window_size) {
  vkDeviceWaitIdle(vulkan()->device());

  cleanupSwapChain();

  vkDeviceWaitIdle(vulkan()->device());

  createSwapChain(window_size);  // *  - recreate

  //Frames will be new here.
  for (auto shader : _shaders) {
    registerShader(shader);
  }

  _bSwapChainOutOfDate = false;
}
bool Swapchain::findValidPresentMode(VkPresentModeKHR& pm_out) {
  uint32_t presentModeCount;
  CheckVKR(vkGetPhysicalDeviceSurfacePresentModesKHR, vulkan()->physicalDevice(), vulkan()->windowSurface(), &presentModeCount, nullptr);
  std::vector<VkPresentModeKHR> presentModes(presentModeCount);
  CheckVKR(vkGetPhysicalDeviceSurfacePresentModesKHR, vulkan()->physicalDevice(), vulkan()->windowSurface(), &presentModeCount, presentModes.data());

  //**FIFO will use vsync
  pm_out = VK_PRESENT_MODE_FIFO_KHR;  //VK_PRESENT_MODE_FIFO_KHR mode is guaranteed to be available
  if (!vulkan()->vsyncEnabled()) {
    BRLogInfo("Vsync disabled");
    for (const auto& availablePresentMode : presentModes) {
      if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
        pm_out = availablePresentMode;
        return true;
      }
    }
    if (pm_out == VK_PRESENT_MODE_FIFO_KHR) {
      BRLogWarn("Mailbox present mode was not found for presenting swapchain.");
    }
  }
  else {
    BRLogInfo("Vsync enabled.");
  }

  return true;
}
bool Swapchain::findValidSurfaceFormat(std::vector<VkFormat> fmts, VkSurfaceFormatKHR& fmt_out) {
  uint32_t formatCount;
  CheckVKR(vkGetPhysicalDeviceSurfaceFormatsKHR, vulkan()->physicalDevice(), vulkan()->windowSurface(), &formatCount, nullptr);

  std::vector<VkSurfaceFormatKHR> formats(formatCount);
  if (formatCount != 0) {
    CheckVKR(vkGetPhysicalDeviceSurfaceFormatsKHR, vulkan()->physicalDevice(), vulkan()->windowSurface(), &formatCount, formats.data());
  }

  for (auto fmt : fmts) {
    for (const auto& availableFormat : formats) {
      if (availableFormat.format == fmt) {
        if (availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
          fmt_out = availableFormat;
          return true;
        }
      }
    }
  }

  return false;
}
void Swapchain::createSwapChain(const BR2::usize2& window_size) {
  BRLogInfo("Creating Swapchain.");

  // How the surfaces are presented from the swapchain.

  //This is cool. Directly query the color space
  VkSurfaceFormatKHR surfaceFormat;
  if (!findValidSurfaceFormat(std::vector<VkFormat>{ VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM }, surfaceFormat)) {
    vulkan()->errorExit("Could not find valid window surface format.");
  }

  VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
  if (!findValidPresentMode(presentMode)) {
    vulkan()->errorExit("Could not find valid present mode.");
  }

  _imageSize.width = window_size.width;
  _imageSize.height = window_size.height;

  VkExtent2D extent = {
    .width = _imageSize.width,
    .height = _imageSize.height
  };

  //Create swapchain
  VkSwapchainCreateInfoKHR swapChainCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface = vulkan()->windowSurface(),
    .minImageCount = vulkan()->swapchainImageCount(),
    .imageFormat = surfaceFormat.format,
    .imageColorSpace = surfaceFormat.colorSpace,
    .imageExtent = extent,
    .imageArrayLayers = 1,  //more than 1 for stereoscopic application
    .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .preTransform = vulkan()->surfaceCaps().currentTransform,
    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .presentMode = presentMode,
    .clipped = VK_TRUE,
    .oldSwapchain = VK_NULL_HANDLE,  // ** For window resizing.
  };

  CheckVKR(vkCreateSwapchainKHR, vulkan()->device(), &swapChainCreateInfo, nullptr, &_swapChain);

  //ImageCount must be what we asked for as we use this data to initialize (pretty much everything buffer related).
  uint32_t imageCount = 0;
  CheckVKR(vkGetSwapchainImagesKHR, vulkan()->device(), _swapChain, &imageCount, nullptr);
  if (imageCount > vulkan()->swapchainImageCount()) {
    //Not an issue, just use less. This could be a performance improvement, though.
    BRLogDebug("The Graphics Driver returned a swapchain image count '" + std::to_string(imageCount) +
               "' greater than what we specified: '" + std::to_string(vulkan()->swapchainImageCount()) + "'.");
    imageCount = vulkan()->swapchainImageCount();
  }
  else if (imageCount < vulkan()->swapchainImageCount()) {
    //Error: we need at least this many images, not because of any functinoal requirement, but because we use the
    // image count to pre-initialize the swapchain data.
    BRLogError("The Graphics Driver returned a swapchain image count '" + std::to_string(imageCount) +
               "' less than what we specified: '" + std::to_string(vulkan()->swapchainImageCount()) + "'.");
    vulkan()->errorExit("Minimum swapchain was not satisfied. Could not continue.");
  }

  std::vector<VkImage> swapChainImages;
  swapChainImages.resize(imageCount);
  CheckVKR(vkGetSwapchainImagesKHR, vulkan()->device(), _swapChain, &imageCount, swapChainImages.data());

  for (size_t idx = 0; idx < swapChainImages.size(); ++idx) {
    auto& image = swapChainImages[idx];
    //Frame vs. Pipeline - Frames are worker bees that submit pipelines queues. When done, they pick up a new command queue and submit it.
    std::shared_ptr<RenderFrame> f = std::make_shared<RenderFrame>(vulkan(), getThis<Swapchain>(), (uint32_t)idx, image, surfaceFormat);
    f->init();
    _frames.push_back(f);
  }
  _imagesInFlight = std::vector<VkFence>(frames().size());
}
void Swapchain::cleanupSwapChain() {
  _frames.clear();
  _imagesInFlight.clear();

  if (_swapChain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(vulkan()->device(), _swapChain, nullptr);
  }
}
bool Swapchain::beginFrame(const BR2::usize2& windowsize) {
  //Returns true if we acquired an image to draw to
  if (isOutOfDate()) {
    initSwapchain(windowsize);
  }

  bool ret = _frames[_currentFrame]->beginFrame();
  if (ret) {
    _frameState = FrameState::FrameBegin;
  }
  return ret;
}
void Swapchain::endFrame() {
  if (_frameState != FrameState::FrameBegin) {
    BRLogError("Called Swapchain::endFrame invalid.");
    return;
  }
  _frames[_currentFrame]->endFrame();
  _currentFrame = (_currentFrame + 1) % _frames.size();
  _frameState = FrameState::FrameEnd;

  //vkQueueWaitIdle(_presentQueue);  //Waits for operations to complete to prevent overfilling the command buffers .
}
void Swapchain::waitImage(uint32_t imageIndex, VkFence myFence) {
  //There is currently a frame that is using this image. So wait for this image.
  if (_imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
    vkWaitForFences(vulkan()->device(), 1, &_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
  }
  _imagesInFlight[imageIndex] = myFence;
}
VkFormat Swapchain::imageFormat() {
  //This is for the default FBO
  AssertOrThrow2(_frames.size() > 0);
  return _frames[0]->imageFormat();
}
const BR2::usize2& Swapchain::imageSize() {
  return _imageSize;
}
std::shared_ptr<RenderFrame> Swapchain::acquireFrame() {
  std::shared_ptr<RenderFrame> frame = nullptr;

  //INVALID:
  frame = _frames[_currentFrame];
  //TODO:
  //Find the next available frame. Do not block.

  return frame;
}

#pragma endregion

}  // namespace VG
