#include "./VulkanClasses.h"
#include "./Vulkan.h"
#include "./VulkanDebug.h"
#include "./GameClasses.h"

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
class ShaderModule_Internal : VulkanObject {
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

#pragma region FramebufferAttachment
FramebufferAttachment::FramebufferAttachment(std::shared_ptr<Vulkan> v, FBOType type, const string_t& name, VkFormat fbo_fmt, uint32_t glsl_location,
                                             const BR2::usize2& imageSize, VkImage swap_img, VkFormat swap_format, const BR2::vec4& clearColor, BlendFunc blending) : VulkanObject(v) {
  _fboType = type;
  _name = name;
  _location = glsl_location;
  _imageSize = imageSize;
  _clearColor = clearColor;
  _blending = blending;
  _swapchainImageFormat = swap_format;
  _swapchainImage = swap_img;
  _fboFormat = fbo_fmt;
}
FramebufferAttachment::~FramebufferAttachment() {
  if (_swapchainImageView != VK_NULL_HANDLE) {
    vkDestroyImageView(vulkan()->device(), _swapchainImageView, nullptr);
  }
}
bool FramebufferAttachment::init() {
  if (_fboFormat != _swapchainImageFormat) {
    BRLogError("Input FBO format '" + VulkanDebug::VkFormat_toString(_fboFormat) + "' was not equal to reference image format '" + VulkanDebug::VkFormat_toString(_swapchainImageFormat) + "'.");
    return false;
  }
  VkImageAspectFlagBits aspect;
  if (_fboType == FBOType::Color) {
    aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  }
  else if (_fboType == FBOType::Depth) {
    aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
  }
  else {
    BRLogError("Unsupported FBOType enum: '" + std::to_string((int)_fboType) + "'");
    return false;
  }
  _swapchainImageView = vulkan()->createImageView(_swapchainImage, _swapchainImageFormat, aspect, 1);
  return true;
}
#pragma endregion

#pragma region Framebuffer
Framebuffer::Framebuffer(std::shared_ptr<Vulkan> v, std::shared_ptr<PipelineShader> s, const BR2::usize2& size) : VulkanObject(v) {
  _pShader = s;
  _size = size;
}
Framebuffer::~Framebuffer() {
  _attachments.clear();
  vkDestroyFramebuffer(vulkan()->device(), _framebuffer, nullptr);
}
void Framebuffer::addAttachment(std::shared_ptr<FramebufferAttachment> at) {
  _attachments.push_back(at);
}
bool Framebuffer::createFBO() {
  if (_attachments.size() == 0) {
    BRLogError("No framebuffer attachments supplied to Framebuffer::createFBO");
    return false;
  }

  uint32_t w = size().width;
  uint32_t h = size().height;

  std::vector<VkImageView> vk_attachments;
  for (auto att : _attachments) {
    vk_attachments.push_back(att->getVkImageView());
  }

  VkFramebufferCreateInfo framebufferInfo = {
    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,               //VkStructureType
    .pNext = nullptr,                                                 //const void*
    .flags = 0,                                                       //VkFramebufferCreateFlags
    .renderPass = _pShader->renderPass(),                             //VkRenderPass
    .attachmentCount = static_cast<uint32_t>(vk_attachments.size()),  //uint32_t
    .pAttachments = vk_attachments.data(),                            //const VkImageView*
    .width = w,                                                       //uint32_t
    .height = h,                                                      //uint32_t
    .layers = 1,                                                      //uint32_t
  };

  CheckVKR(vkCreateFramebuffer, vulkan()->device(), &framebufferInfo, nullptr, &_framebuffer);

  //Validate framebuffer
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
      BRLogError("Output FBO " + std::to_string(i) + " width '" +
                 std::to_string(attachments()[i]->imageSize().width) +
                 "' did not match first FBO width '" + std::to_string(def_w) + "'.");
      return false;
    }
    if (attachments()[i]->imageSize().height != def_h) {
      BRLogError("Output FBO " + std::to_string(i) + " height '" +
                 std::to_string(attachments()[i]->imageSize().height) +
                 "' did not match first FBO height '" + std::to_string(def_h) + "'.");
      return false;
    }
  }

  if (def_w == -1 || def_h == -1) {
    BRLogError("Invalid FBO image size.");
    return false;
  }

  return true;
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
  if (!createRenderPass()) {
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
  int maxinputs = vulkan()->deviceProperties().limits.maxVertexInputAttributes;
  if (vert->reflectionData()->input_variable_count >= maxinputs) {
    BRLogError("Error creating shader '" + name() + "' - too many input variables");
    return false;
  }

  std::shared_ptr<ShaderModule> frag = getModule(VK_SHADER_STAGE_FRAGMENT_BIT);
  int maxAttachments = vulkan()->deviceProperties().limits.maxFragmentOutputAttachments;
  if (frag->reflectionData()->output_variable_count >= maxAttachments) {
    BRLogError("Error creating shader '" + name() + "' - too many output attachments in fragment shader.");
    return false;
  }

  //TODO: geom
  std::shared_ptr<ShaderModule> geom = getModule(VK_SHADER_STAGE_GEOMETRY_BIT);
  return true;
}
bool PipelineShader::createInputs() {
  std::shared_ptr<ShaderModule> mod = getModule(VK_SHADER_STAGE_VERTEX_BIT, true);

  int maxinputs = vulkan()->deviceProperties().limits.maxVertexInputAttributes;
  int maxbindings = vulkan()->deviceProperties().limits.maxVertexInputBindings;

  if (mod->reflectionData()->input_variable_count >= maxinputs) {
    BRLogError("Error creating shader '" + name() + "' - too many input variables");
    return false;
  }

  int iBindingIndex = 0;
  size_t iOffset = 0;
  for (int ii = 0; ii < mod->reflectionData()->input_variable_count; ++ii) {
    auto& iv = mod->reflectionData()->input_variables[ii];
    std::shared_ptr<VertexAttribute> attrib = std::make_shared<VertexAttribute>();
    attrib->_name = std::string(iv.name);

    //Attrib Size
    attrib->_componentCount = iv.numeric.vector.component_count;
    attrib->_componentSizeBytes = iv.numeric.scalar.width / 8;
    attrib->_matrixSize = iv.numeric.matrix.column_count * iv.numeric.matrix.row_count;
    attrib->_totalSizeBytes = (attrib->_componentCount + attrib->_matrixSize) * attrib->_componentSizeBytes;

    if ((iv.numeric.matrix.column_count != iv.numeric.matrix.row_count)) {
      BRThrowException("Failure - non-square matrix dimensions for vertex attribute '" + attrib->_name + "' in shader '" + name() + "'");
    }
    else if ((iv.numeric.matrix.column_count > 0) &&
             (iv.numeric.matrix.column_count != 2) &&
             (iv.numeric.matrix.column_count != 3) &&
             (iv.numeric.matrix.column_count != 4)) {
      BRThrowException("Failure - invalid matrix dimensions for vertex attribute '" + attrib->_name + "' in shader '" + name() + "'");
    }
    else if (iv.numeric.matrix.stride > 0) {
      BRThrowException("Failure - nonzero stride for matrix vertex attribute '" + attrib->_name + "' in shader '" + name() + "'");
    }

    if (attrib->_matrixSize > 0 && attrib->_componentCount > 0) {
      BRThrowException("Failure - matrix and vector dimensions present in attribute '" + attrib->_name + "' in shader '" + name() + "'");
    }

    //Attrib type.
    //Note type_description.typeFlags is the int,scal,mat type.
    attrib->_typeFlags = iv.type_description->type_flags;
    attrib->_userType = parseUserType(attrib->_name);
    attrib->_desc.binding = 0;
    attrib->_desc.location = iBindingIndex;
    attrib->_desc.format = spvReflectFormatToVulkanFormat(iv.format);
    attrib->_desc.offset = iOffset;  //Default offset, for an exact-match vertex.

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
bool PipelineShader::createOutputs() {
  std::shared_ptr<ShaderModule> mod = getModule(VK_SHADER_STAGE_FRAGMENT_BIT, true);
  if (mod == nullptr) {
    BRLogError("Fragment module not found for shader '" + this->name() + "'");
    Gu::debugBreak();
    return false;
  }

  for (int inp = 0; inp < mod->reflectionData()->output_variable_count; ++inp) {
    string_t name = string_t(mod->reflectionData()->output_variables[inp].name);

    if (StringUtil::startsWith(name, "_outFBO")) {
      //This is instantiated into a Framebuffer when we bind the shader to the Swapchain.
      ShaderOutput fb;
      fb._name = name;
      fb._location = mod->reflectionData()->output_variables[inp].location;
      //We can't use shader output format to create the FBOs because SPV-Reflect doesn't know what format our FBO will be.
      fb._format = spvReflectFormatToVulkanFormat(mod->reflectionData()->output_variables[inp].format);
      if (fb._format == VK_FORMAT_R32G32B32A32_SFLOAT) {
        fb._type = FBOType::Color;
      }
      else if (fb._format == VK_FORMAT_R32_SFLOAT) {
        fb._type = FBOType::Depth;
      }
      else {
        return shaderError("Unrecognized shader output variable type '" + VulkanDebug::VkFormat_toString(fb._format) + "'");
      }

      _shaderOutputs.push_back(fb);
    }
    else {
      return shaderError("Shader - output variable was not an fbo prefixed with _outFBO - this is not supported.");
    }
  }

  return true;
}
bool PipelineShader::createRenderPass() {
  //using subpassLoad you can read previous subpass values. This is more efficient than the old approach.
  //https://www.saschawillems.de/blog/2018/07/19/vulkan-input-attachments-and-sub-passes/
  //https://github.com/KhronosGroup/GLSL/blob/master/extensions/khr/GL_KHR_vulkan_glsl.txt
  //FOr now we just use this for work.
  std::vector<VkAttachmentDescription> colorAttachments;
  std::vector<VkAttachmentReference> attachmentRefs;
  for (size_t iatt = 0; iatt < _shaderOutputs.size(); ++iatt) {
    //Make sure this data matches the data in Framebuffer creation (FramebufferAttachment)
    if (_shaderOutputs[iatt]._type == FBOType::Color) {
      colorAttachments.push_back({
        .flags = 0,
        .format = _shaderOutputs[iatt]._format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,  //** This needs to be changed for Deferred MRTs
      });
      attachmentRefs.push_back({
        .attachment = _shaderOutputs[iatt]._location,  //layout (location=x)
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      });
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
    .colorAttachmentCount = static_cast<uint32_t>(attachmentRefs.size()),
    .pColorAttachments = attachmentRefs.data(),
    .pResolveAttachments = nullptr,
    .pDepthStencilAttachment = nullptr,  // TODO
    .preserveAttachmentCount = 0,
    .pPreserveAttachments = nullptr,
  };
  VkRenderPassCreateInfo renderPassInfo = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .attachmentCount = static_cast<uint32_t>(colorAttachments.size()),
    .pAttachments = colorAttachments.data(),
    .subpassCount = 1,
    .pSubpasses = &subpass,
    .dependencyCount = 0,
    .pDependencies = nullptr,
  };
  CheckVKR(vkCreateRenderPass, vulkan()->device(), &renderPassInfo, nullptr, &_renderPass);

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
        BRLogError("Invalid or unsupported shader stage (SpvReflectShaderStage):  " + std::to_string(module->reflectionData()->shader_stage));
        return false;
      }

      if (descriptor.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
        d->_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        _nPool_UBOs++;

        if (descriptor.array.dims_count > 0) {
          //This is an array descriptor. I don't think this is valid in Vulkan-GLSL
          BRLogError("Illegal Descriptor array was found.");
          return false;
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
            BRLogError("Illegal Descriptor multi-array.");
            return false;
          }
          else {
            d->_arraySize = descriptor.array.dims[0];
          }
        }
      }
      else {
        BRLogError("Shader descriptor not supported - Spirv-Reflect Descriptor: " + descriptor.descriptor_type);
        Gu::debugBreak();
        return false;
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

  for (auto data : _shaderData) {
    auto desc = getDescriptor(var_name);
    if (desc == nullptr) {
      return shaderError("Failed to locate UBO descriptor for shader variable '" + var_name + "'");
    }
    else if (data->_uniformBuffers.find(name) != data->_uniformBuffers.end()) {
      return shaderError("UBO for shader variable '" + var_name + "' with client variable '" + name + "' was already found in ShaderData.");
    }
    else {
      //Get the descriptor size.
      uint32_t size = bufsize;
      if (bufsize == VK_WHOLE_SIZE) {
        size = desc->_blockSize;
      }
      std::shared_ptr<ShaderDataUBO> dat = std::make_shared<ShaderDataUBO>();
      dat->_buffer = std::make_shared<VulkanBuffer>(
        vulkan(),
        VulkanBufferType::UniformBuffer,
        false,  //not on GPU
        size, nullptr, 0);
      dat->_descriptor = desc;
      data->_uniformBuffers.insert(std::make_pair(name, dat));
    }
  }

  return true;
}
bool PipelineShader::bindUBO(const string_t& name, uint32_t swapchainImage, std::shared_ptr<VulkanBuffer> buffer, VkDeviceSize offset, VkDeviceSize range) {
  //Binds a shader Uniform to this shader for the given swapchain image.
  std::shared_ptr<Descriptor> desc = getDescriptor(name);
  if (desc == nullptr) {
    BRLogError("Descriptor '" + name + "'could not be found for shader '" + this->name() + "'.");
    Gu::debugBreak();
    return false;
  }

  VkDescriptorBufferInfo bufferInfo = {
    .buffer = buffer->hostBuffer()->buffer(),
    .offset = offset,
    .range = range,
  };
  VkWriteDescriptorSet descWrite = {
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .pNext = nullptr,
    .dstSet = _descriptorSets[swapchainImage],
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
bool PipelineShader::bindSampler(const string_t& name, uint32_t swapchainImage, std::shared_ptr<VulkanTextureImage> texture, uint32_t arrayIndex) {
  std::shared_ptr<Descriptor> desc = getDescriptor(name);
  if (desc == nullptr) {
    BRLogError("Descriptor '" + name + "'could not be found for shader '" + this->name() + "'.");
    Gu::debugBreak();
    return false;
  }

  VkDescriptorImageInfo imageInfo = {
    .sampler = texture->sampler(),
    .imageView = texture->imageView(),
    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
  VkWriteDescriptorSet descriptorWrite = {
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .pNext = nullptr,
    .dstSet = _descriptorSets[swapchainImage],
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
bool PipelineShader::beginRenderPass(std::shared_ptr<CommandBuffer> buf, std::shared_ptr<RenderFrame> frame, BR2::urect2* extent) {
  if (valid() == false) {
    return false;
  }
  auto sd = getShaderData(frame);
  if (sd == nullptr || sd->_framebuffer == nullptr) {
    BRLogErrorOnce("Shader had no initialized framebuffer or shaderdata.");
    Gu::debugBreak();
    return false;
  }
  auto fbo = sd->_framebuffer;

  //Returns false if the rendering was not set up correctly.
  //Validation
  if (fbo->attachments().size() == 0) {
    BRLogError("No output FBOs have been created.");
    return false;
  }

  std::vector<VkClearValue> clearValues;
  for (auto att : fbo->attachments()) {
    VkClearValue cv;
    if (att->type() == FBOType::Depth) {
      cv.depthStencil.depth = att->clearColor().x;
      cv.depthStencil.stencil = (uint32_t)att->clearColor().y;
    }
    else if (att->type() == FBOType::Color) {
      cv.color = {
        att->clearColor().x,
        att->clearColor().y,
        att->clearColor().z,
        att->clearColor().w
      };
    }
    else {
      BRThrowException(std::string() + "Invalid FBO type: " + std::to_string((int)att->type()));
    }
    clearValues.push_back(cv);
  }

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
    w = fbo->size().width;
    h = fbo->size().height;
  }

  VkExtent2D outputExtent = {
    .width = static_cast<uint32_t>(w),
    .height = static_cast<uint32_t>(h)
  };
  VkRenderPassBeginInfo passBeginInfo = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .pNext = nullptr,
    .renderPass = renderPass(),
    .framebuffer = fbo->getVkFramebuffer(),
    .renderArea = { .offset = VkOffset2D{ .x = 0, .y = 0 }, .extent = outputExtent },
    .clearValueCount = static_cast<uint32_t>(clearValues.size()),
    .pClearValues = clearValues.data(),
  };
  vkCmdBeginRenderPass(buf->getVkCommandBuffer(), &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

  return true;
}
std::shared_ptr<Pipeline> PipelineShader::getPipeline(VkPrimitiveTopology topo, VkPolygonMode mode) {
  //**TODO: HACK - create multiple pipelines for Vertex Format, Polygonmode & Topo.
  //**TODO: HACK - create multiple pipelines for Vertex Format, Polygonmode & Topo.
  BRLogWarn("- Multiple vertex types not implemented yet.");
  std::shared_ptr<Pipeline> pipe = nullptr;
  if (_pipelines.size() == 0) {
    pipe = std::make_shared<Pipeline>(vulkan(), getThis<PipelineShader>(), nullptr, topo, mode);
    _pipelines.push_back(pipe);
  }
  return _pipelines[0];
}
void PipelineShader::bindDescriptors(std::shared_ptr<CommandBuffer> cmd, std::shared_ptr<Pipeline> pipe, uint32_t swapchainImageIndex) {
  //TODO: use ShaderData for descriptor sets (currently the Shader<->Framebuffer on RenderFrame)
  //TODO: pass Renderframe in here and get it's descriptor set
  for (auto desc : _descriptors) {
    if (desc.second->_isBound == false) {
      BRLogWarnOnce("Descriptor '" + desc.second->_name + "' was not bound before invoking shader '" + this->name() + "'");
    }
  }

  vkCmdBindDescriptorSets(cmd->getVkCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->getVkPipelineLayout(),
                          0,  //Layout ID
                          1,
                          &_descriptorSets[swapchainImageIndex], 0, nullptr);
}
bool PipelineShader::shaderError(const string_t& msg) {
  BRLogError(msg);
  _bValid = false;
  Gu::debugBreak();
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

bool PipelineShader::recreateShaderDataFBO(uint32_t frameIndex, VkImage swap_image, VkFormat swap_format, const BR2::usize2& swap_siz) {
  std::shared_ptr<ShaderData> sd = nullptr;
  while (_shaderData.size() <= frameIndex) {
    _shaderData.push_back(std::make_shared<ShaderData>());
  }
  sd = _shaderData[frameIndex];

  //Automatic Descriptors & UBOs - For later.
  //   for (auto d : _descriptors) {
  //     if (d.second->_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
  //       auto sdubo = std::make_shared<ShaderDataUBO>();
  //       sdubo->_descriptor = d.second;
  //
  //       //Automatic UBO allocation.
  //       sdubo->buffer = std::make_shared<VulkanBuffer>(
  //         vulkan(),
  //         VulkanBufferType::UniformBuffer,
  //         false,
  //         d.second->_blockSize, nullptr, 0);
  //
  //       if (d.second->_function == DescriptorFunction::ViewProjMatrixUBO) {
  //       }
  //       else if (d.second->_function == DescriptorFunction::InstnaceMatrixUBO) {
  //         if (d.second->_blockSize % sizeof(InstanceUBOData) != 0) {
  //           return shaderError("Instance blocksize is not a multiple of InstanceUBO data.");
  //         }
  //         sdubo->_data._instanceUBOData._maxInstances = d.second->_blockSize / sizeof(InstanceUBOData);
  //
  //         sdubo->buffer = std::make_shared<VulkanBuffer>(
  //           vulkan(),
  //           VulkanBufferType::UniformBuffer,
  //           false,
  //           sizeof(InstanceUBOData) * _numInstances, nullptr, 0);
  //       }
  //       else {
  //         return shaderError("Failed to identify descriptor type.");
  //       }
  //
  //       sd->_uniformBuffers.push_back(sdubo)
  //     }
  //     else if (d.second->_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
  //     }
  //     else {
  //       return shaderError("Descriptor type not supported '" + VulkanDebug::VkDescriptorType_ToString(d.second->_type) + "'");
  //     }
  //   }

  //FBO
  sd->_framebuffer = std::make_shared<Framebuffer>(vulkan(), getThis<PipelineShader>(), swap_siz);
  for (auto out_att : outputFBOs()) {
    auto att = std::make_shared<FramebufferAttachment>(
      vulkan(),
      out_att._type,
      out_att._name,
      swap_format,  //using the spv-reflect format causes problems
      out_att._location,
      swap_siz,
      swap_image,
      swap_format,
      out_att._clearColor,  //TODO
      out_att._blending);   //TODO

    if (!att->init()) {
      return shaderError("Failed to initialize fbo attachment.");
    }

    sd->_framebuffer->addAttachment(att);
  }
  if (!sd->_framebuffer->createFBO()) {
    return shaderError("Failed to create FBO.");
  }
  return true;
}

#pragma endregion

#pragma region Pipeline

Pipeline::Pipeline(std::shared_ptr<Vulkan> v,
                   std::shared_ptr<PipelineShader> shader,
                   std::shared_ptr<BR2::VertexFormat> vtxFormat,
                   VkPrimitiveTopology topo,
                   VkPolygonMode mode) : VulkanObject(v) {
  _primitiveTopology = topo;
  _polygonMode = mode;

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
  for (auto& att : shader->outputFBOs()) {
    VkPipelineColorBlendAttachmentState cba{};

    if (att._blending == BlendFunc::Disabled) {
      cba.blendEnable = VK_FALSE;
    }
    else {
      cba.blendEnable = VK_TRUE;
      cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
      cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      cba.colorBlendOp = VK_BLEND_OP_ADD;
      cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
      cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
      cba.alphaBlendOp = VK_BLEND_OP_ADD;
      cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    }
    attachmentBlending.push_back(cba);
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
    .topology = topo,
    .primitiveRestartEnable = VK_FALSE,
  };
  //Pipeline
  VkPipelineViewportStateCreateInfo viewportState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .viewportCount = 0,
    .pViewports = nullptr,
    .scissorCount = 0,
    .pScissors = nullptr,
  };

  VkPipelineRasterizationStateCreateInfo rasterizer = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = mode,
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
    VK_DYNAMIC_STATE_LINE_WIDTH,//Not always supported afaik.
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
    .pDepthStencilState = nullptr,
    .pColorBlendState = &colorBlending,
    .pDynamicState = &dynamicState,
    .layout = _pipelineLayout,
    .renderPass = shader->renderPass(),
    .subpass = 0,
    .basePipelineHandle = VK_NULL_HANDLE,
    .basePipelineIndex = -1,
  };

  CheckVKR(vkCreateGraphicsPipelines, vulkan()->device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_pipeline);
}
Pipeline::~Pipeline() {
  vkDestroyPipeline(vulkan()->device(), _pipeline, nullptr);
  vkDestroyPipelineLayout(vulkan()->device(), _pipelineLayout, nullptr);
}
void Pipeline::drawIndexed(std::shared_ptr<CommandBuffer> cmd, std::shared_ptr<Mesh> m, uint32_t numInstances) {
  //TODO: vertex format and primitive topology.
  //std::shared_ptr<Pipeline> pipe = getPipeline(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, mode);
  //if (pipe != nullptr) {
  // vkCmdBindPipeline(cmd->getVkCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->getVkPipeline());

  m->bindBuffers(cmd);
  m->drawIndexed(cmd, numInstances);
  //}
}
void Pipeline::bind(std::shared_ptr<CommandBuffer> cmd) {
  vkCmdBindPipeline(cmd->getVkCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, getVkPipeline());
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
bool CommandBuffer::beginPass(std::shared_ptr<PipelineShader> shader, std::shared_ptr<RenderFrame> frame, BR2::urect2* extent) {
  //Returns true if we can begin the pass.
  validateState(_state == CommandBufferState::Begin);

  if (!shader->beginRenderPass(getThis<CommandBuffer>(), frame, extent)) {
    return false;
  }

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
  _image = img;
  _frameIndex = frameIndex;
  _imageFormat = fmt;
}
RenderFrame::~RenderFrame() {
  //Cleanup
  cleanupSyncObjects();

  vkDestroyImageView(vulkan()->device(), _imageView, nullptr);
}
void RenderFrame::init() {
  _imageView = vulkan()->createImageView(_image, _imageFormat.format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
  createSyncObjects();

  _pCommandBuffer = std::make_shared<CommandBuffer>(vulkan(), getThis<RenderFrame>());
}
void RenderFrame::createSyncObjects() {
  VkSemaphoreCreateInfo semaphoreInfo = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
  };
  CheckVKR(vkCreateSemaphore, vulkan()->device(), &semaphoreInfo, nullptr, &_imageAvailable);
  CheckVKR(vkCreateSemaphore, vulkan()->device(), &semaphoreInfo, nullptr, &_renderFinished);

  VkFenceCreateInfo fenceInfo = {
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .pNext = nullptr,
    .flags = VK_FENCE_CREATE_SIGNALED_BIT,  //Fences must always be created in a signaled state.
  };

  CheckVKR(vkCreateFence, vulkan()->device(), &fenceInfo, nullptr, &_inFlightFence);
}
void RenderFrame::cleanupSyncObjects() {
  vkDestroySemaphore(vulkan()->device(), _imageAvailable, nullptr);
  vkDestroySemaphore(vulkan()->device(), _renderFinished, nullptr);
  vkDestroyFence(vulkan()->device(), _inFlightFence, nullptr);
}
void RenderFrame::beginFrame() {
  //I feel like the async aspect of RenderFrame might need to be a separate DispatchedFrame structure or..

  //Wait forever TODO: we can keep updating the game in case the rendering isn't available.
  vkWaitForFences(vulkan()->device(), 1, &_inFlightFence, VK_TRUE, UINT64_MAX);

  VkResult res;
  res = vkAcquireNextImageKHR(vulkan()->device(), _pSwapchain->getVkSwapchain(), UINT64_MAX, _imageAvailable, VK_NULL_HANDLE, &_currentRenderingImageIndex);
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

  _pSwapchain->waitImage(_currentRenderingImageIndex, _inFlightFence);
}
void RenderFrame::endFrame() {
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
    .pWaitSemaphores = &_imageAvailable,
    .pWaitDstStageMask = waitStages,
    .commandBufferCount = 1,

    .pCommandBuffers = &buf,

    //The semaphore is signaled when the queue has completed the requested wait stages.
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = &_renderFinished,
  };

  vkResetFences(vulkan()->device(), 1, &_inFlightFence);
  vkQueueSubmit(vulkan()->graphicsQueue(), 1, &submitInfo, _inFlightFence);

  std::vector<VkSwapchainKHR> chains{ _pSwapchain->getVkSwapchain() };

  VkPresentInfoKHR presentinfo = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .pNext = nullptr,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &_renderFinished,
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
}
VkFormat RenderFrame::imageFormat() {
  return _imageFormat.format;
}
const BR2::usize2& RenderFrame::imageSize() {
  return _pSwapchain->imageSize();
}
bool RenderFrame::rebindShader(std::shared_ptr<PipelineShader> s) {
  //Recreate Framebuffer
  BR2::usize2 fb_siz = { _pSwapchain->imageSize().width, _pSwapchain->imageSize().height };
  if (!s->recreateShaderDataFBO(frameIndex(), _image, _imageFormat.format, fb_siz)) {
    return s->shaderError("Failed to create shaderdata for shader '" + s->name() + "'");
  }
  return true;
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

  for (auto frame : _frames) {
    frame->rebindShader(shader);
  }
}
void Swapchain::rebindShaders() {
  for (auto shader : _shaders) {
    registerShader(shader);
  }
}
void Swapchain::initSwapchain(const BR2::usize2& window_size) {
  vkDeviceWaitIdle(vulkan()->device());

  cleanupSwapChain();

  createSwapChain(window_size);  // *  - recreate

  //Frames will be new here.
  rebindShaders();

  _bSwapChainOutOfDate = false;
}
bool Swapchain::findValidPresentMode(VkPresentModeKHR& pm_out) {
  uint32_t presentModeCount;
  CheckVKR(vkGetPhysicalDeviceSurfacePresentModesKHR, vulkan()->physicalDevice(), vulkan()->windowSurface(), &presentModeCount, nullptr);
  std::vector<VkPresentModeKHR> presentModes(presentModeCount);
  CheckVKR(vkGetPhysicalDeviceSurfacePresentModesKHR, vulkan()->physicalDevice(), vulkan()->windowSurface(), &presentModeCount, presentModes.data());

  pm_out = VK_PRESENT_MODE_FIFO_KHR;  //VK_PRESENT_MODE_FIFO_KHR mode is guaranteed to be available
  for (const auto& availablePresentMode : presentModes) {
    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
      pm_out = availablePresentMode;
      return true;
    }
  }
  if (pm_out == VK_PRESENT_MODE_FIFO_KHR) {
    BRLogWarn("Mailbox present mode was not found for presenting swapchain.");
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
    std::shared_ptr<RenderFrame> f = std::make_shared<RenderFrame>(vulkan(), getThis<Swapchain>(), idx, image, surfaceFormat);
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
void Swapchain::beginFrame(const BR2::usize2& windowsize) {
  if (isOutOfDate()) {
    initSwapchain(windowsize);
  }

  _frames[_currentFrame]->beginFrame();
}
void Swapchain::endFrame() {
  _frames[_currentFrame]->endFrame();
  _currentFrame = (_currentFrame + 1) % _frames.size();

  //**CONTINUE TUTORIAL AFTER THIS POINT
  //https://vulkan-tutorial.com/en/Drawing_a_triangle/Drawing/Rendering_and_presentation
  // We add additional threads for async the rendering.
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
