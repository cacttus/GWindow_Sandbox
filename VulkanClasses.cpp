#include "./VulkanClasses.h"
#include "./Vulkan.h"
namespace VG {

#pragma region VulkanDeviceBuffer

class VulkanDeviceBuffer_Internal {
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

  if (properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT > 0) {
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

class VulkanBuffer_Internal {
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
VulkanImage::~VulkanImage() {
  vkDestroyImage(vulkan()->device(), _image, nullptr);
  vkFreeMemory(vulkan()->device(), _textureImageMemory, nullptr);
  vkDestroyImageView(vulkan()->device(), _textureImageView, nullptr);
}
void VulkanImage::allocateMemory(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                                 VkImageUsageFlags usage, VkMemoryPropertyFlags properties, uint32_t mipLevels,
                                 VkSampleCountFlagBits samples) {
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
    .samples = VK_SAMPLE_COUNT_1_BIT,            // VkSampleCountFlagBits
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
  CheckVKR(vkAllocateMemory, vulkan()->device(), &allocInfo, nullptr, &_textureImageMemory);
  CheckVKR(vkBindImageMemory, vulkan()->device(), _image, _textureImageMemory, 0);
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
                               const BR2::iext2& srcRegion,
                               const BR2::iext2& dstRegion,
                               VkImageLayout srcLayout,
                               VkImageLayout dstLayout,
                               uint32_t srcMipLevel,
                               uint32_t dstMipLevel,
                               VkImageAspectFlagBits aspectFlags, VkFilter filter) {
  VkImageBlit blit = {
    .srcSubresource = {
      .aspectMask = aspectFlags,                                                                  // VkImageAspectFlags
      .mipLevel = srcMipLevel,                                                                    // uint32_t
      .baseArrayLayer = 0,                                                                        // uint32_t
      .layerCount = 1,                                                                            // uint32_t
    },                                                                                            // VkImageSubresourceLayers
    .srcOffsets = { { srcRegion.x, srcRegion.y, 0 }, { srcRegion.width, srcRegion.height, 1 } },  // VkOffset3D
    .dstSubresource = {
      .aspectMask = aspectFlags,  // VkImageAspectFlags
      .mipLevel = dstMipLevel,    // uint32_t
      .baseArrayLayer = 0,        // uint32_t
      .layerCount = 1,            // uint32_t
    },
    .dstOffsets = { { dstRegion.x, dstRegion.y, 0 }, { dstRegion.width, dstRegion.height, 1 } },  // VkOffset3D
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
    .srcAccessMask = srcAccessFlags,
    .dstAccessMask = dstAccessFlags,
    .oldLayout = oldLayout,
    .newLayout = newLayout,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = image,
    .subresourceRange = {
      .aspectMask = subresourceMask,
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

#pragma region VulkanTextureImage

VulkanTextureImage::VulkanTextureImage(std::shared_ptr<Vulkan> pvulkan, std::shared_ptr<Img32> pimg, MipmapMode mipmaps) : VulkanImage(pvulkan) {
  BRLogInfo("Creating Vulkan image");
  VkFormat img_fmt = VK_FORMAT_R8G8B8A8_SRGB;  //VK_FORMAT_R8G8B8A8_UINT;
  VkImageUsageFlagBits transfer_src = (VkImageUsageFlagBits)0;

  _mipmap = mipmaps;

  if (_mipmap != MipmapMode::Disabled) {
    _mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(pimg->_width, pimg->_height)))) + 1;
    transfer_src = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;  //We need to use this image as a src transfer to fill each mip level.
  }
  else {
    _mipLevels = 1;
  }

  allocateMemory(pimg->_width, pimg->_height,
                 img_fmt,
                 VK_IMAGE_TILING_OPTIMAL,
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | transfer_src,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _mipLevels);
  // this was for OpenGL may not be needed
  //flipImage20161206(pimg->_data, pimg->_width, pimg->_height);

  copyImageToGPU(pimg, img_fmt);

  if (_mipmap != MipmapMode::Disabled) {
    generateMipmaps();
  }
}
VulkanTextureImage::~VulkanTextureImage() {
  vkDestroySampler(vulkan()->device(), _textureSampler, nullptr);
}
VkSampler VulkanTextureImage::sampler() { return _textureSampler; }

bool VulkanTextureImage::mipmappingSupported() {
  VkFormatProperties formatProperties;
  vkGetPhysicalDeviceFormatProperties(vulkan()->physicalDevice(), format(), &formatProperties);
  bool supported = formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
  return supported;
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

  //   VkImageLayout src_layout, dst_layout;
  //
  //   if(_mipLevels>1){
  // src_layout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ;
  // dst_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ;
  //   }
  //   else{
  // src_layout=VK_IMAGE_LAYOUT_UNDEFINED ;
  // dst_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ;
  //   }

  //Undefined layout will be discard image data.
  transitionImageLayout(img_fmt, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  copyBufferToImage(pimg);
  transitionImageLayout(img_fmt /*? - transition to the same format?*/, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  //Cleanup host buffer. We are done with it.
  _host = nullptr;

  //Image view
  _textureImageView = vulkan()->createImageView(_image, img_fmt, VK_IMAGE_ASPECT_COLOR_BIT, _mipLevels);

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
void VulkanTextureImage::copyBufferToImage(std::shared_ptr<Img32> pimg) {
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
    .srcAccessMask = srcAccessMask,                   //VkAccessFlags
    .dstAccessMask = dstAccessMask,                   //VkAccessFlags
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

}  // namespace VG