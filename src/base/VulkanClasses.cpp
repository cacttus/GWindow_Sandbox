#include "./VulkanClasses.h"
#include "./VulkanDebug.h"
#include "./GWorld.h"
#include "./VulkanUtils.h"

namespace VG {

#pragma region VulkanDeviceBuffer

VulkanDeviceBuffer::VulkanDeviceBuffer(std::shared_ptr<Vulkan> pvulkan, size_t itemSize, size_t itemCount, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) : VulkanObject(pvulkan) {
  _itemSize = itemSize;
  _itemCount = itemCount;
  _byteSize = _itemSize * _itemCount;

  if ((properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) > 0) {
    _isGpuBuffer = true;
  }

  VkBufferCreateInfo buffer_info = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .size = _byteSize,
    .usage = usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = nullptr,
  };
  CheckVKR(vkCreateBuffer, vulkan()->device(), &buffer_info, nullptr, &_buffer);

  VkMemoryRequirements mem_inf;
  vkGetBufferMemoryRequirements(vulkan()->device(), _buffer, &mem_inf);

  VkMemoryAllocateInfo allocInfo = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .pNext = nullptr,
    .allocationSize = mem_inf.size,
    .memoryTypeIndex = findMemoryType(vulkan()->physicalDevice(), mem_inf.memoryTypeBits, properties)
  };
  CheckVKR(vkAllocateMemory, vulkan()->device(), &allocInfo, nullptr, &_bufferMemory);
  CheckVKR(vkBindBufferMemory, vulkan()->device(), _buffer, _bufferMemory, 0);
}
VulkanDeviceBuffer::~VulkanDeviceBuffer() {
  vkDestroyBuffer(vulkan()->device(), _buffer, nullptr);
  vkFreeMemory(vulkan()->device(), _bufferMemory, nullptr);
  _buffer = VK_NULL_HANDLE;
  _bufferMemory = VK_NULL_HANDLE;
}
void VulkanDeviceBuffer::copy_from(void* src_buf, size_t copy_count_items, size_t data_offset_items, size_t buffer_offset_items) {
  //Copy supplied data to the gpu host-side memory getVkBuffer, this is required for both staged and host-only buffers.
  AssertOrThrow2(_isGpuBuffer == false);
  AssertOrThrow2(copy_count_items + buffer_offset_items <= itemCount());

  size_t data_offset_bytes = buffer_offset_items * itemSize();
  size_t buffer_offset_bytes = data_offset_items * itemSize();
  size_t copy_bytes = copy_count_items * itemSize();

  AssertOrThrow2(buffer_offset_bytes + copy_bytes <= _byteSize);

  void* gpu_data = nullptr;
  CheckVKR(vkMapMemory, vulkan()->device(), _bufferMemory, buffer_offset_bytes, copy_bytes, 0, &gpu_data);
  if (gpu_data) {
    memcpy(gpu_data, (void*)((char*)src_buf + data_offset_bytes), copy_bytes);
  }
  vkUnmapMemory(vulkan()->device(), _bufferMemory);
}
void VulkanDeviceBuffer::copy_to(void* dst_buf, size_t copy_count_items, size_t data_offset_items, size_t buffer_offset_items) {
  //Copy supplied data to the gpu host-side memory getVkBuffer, this is required for both staged and host-only buffers.
  AssertOrThrow2(_isGpuBuffer == false);
  AssertOrThrow2(copy_count_items + buffer_offset_items <= itemCount());

  size_t data_offset_bytes = buffer_offset_items * itemSize();
  size_t buffer_offset_bytes = data_offset_items * itemSize();
  size_t copy_bytes = copy_count_items * itemSize();

  AssertOrThrow2(buffer_offset_bytes + copy_bytes <= _byteSize);

  void* gpu_data = nullptr;
  CheckVKR(vkMapMemory, vulkan()->device(), _bufferMemory, buffer_offset_bytes, copy_bytes, 0, &gpu_data);
  if (gpu_data) {
    memcpy((void*)((char*)dst_buf + data_offset_bytes), gpu_data, copy_bytes);
  }
  vkUnmapMemory(vulkan()->device(), _bufferMemory);
}
void VulkanDeviceBuffer::copy_device(std::shared_ptr<VulkanDeviceBuffer> host_buf, size_t item_copyCount, size_t itemOffset_Host, size_t itemOffset_Gpu) {
  AssertOrThrow2(_isGpuBuffer == true);
  AssertOrThrow2(host_buf != nullptr);
  AssertOrThrow2(itemOffset_Host + item_copyCount <= host_buf->itemCount());
  AssertOrThrow2(itemSize() * itemOffset_Gpu + itemSize() * item_copyCount <= _byteSize);

  size_t data_offset_bytes = itemOffset_Host * itemSize();
  size_t device_offset_bytes = itemOffset_Gpu * itemSize();
  size_t copy_bytes = item_copyCount * itemSize();

  CommandBuffer cmd(vulkan(), nullptr);
  cmd.begin();
  cmd.copyBuffer(host_buf->getVkBuffer(), getVkBuffer(), copy_bytes, data_offset_bytes, device_offset_bytes);
  cmd.end();
  cmd.submit({}, {}, {}, VK_NULL_HANDLE, true);
}
uint32_t VulkanDeviceBuffer::findMemoryType(VkPhysicalDevice d, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties props;
  vkGetPhysicalDeviceMemoryProperties(d, &props);

  for (uint32_t i = 0; i < props.memoryTypeCount; i++) {
    if (typeFilter & (1 << i) && (props.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }
  throw std::runtime_error("Failed to find valid memory type for vt buffer.");
  return 0;
}
#pragma endregion

#pragma region VulkanBuffer
VulkanBuffer::VulkanBuffer(std::shared_ptr<Vulkan> dev, VulkanBufferType eType, bool bStaged, size_t itemSize, size_t itemCount, void* items, size_t item_count) : VulkanObject(dev) {
  //@param data - data to be copied, or nullptr to make empty.
  //@param bStaged - If this getVkBuffer is staged for GPU copying. If false this getVkBuffer resized on host (cpu) memory.
  //                 Set this to false if buffers are being updated frequently (ubo data).
  //                 Set to true if getVkBuffer is sent go GPU once (vertex data)
  VkMemoryPropertyFlags bufType = 0;

  _bUseStagingBuffer = bStaged;
  _eType = eType;

  if (_eType == VulkanBufferType::IndexBuffer) {
    bufType = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  }
  else if (_eType == VulkanBufferType::VertexBuffer) {
    bufType = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  }
  else if (_eType == VulkanBufferType::UniformBuffer) {
    bufType = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (_bUseStagingBuffer == true) {
      BRLogWarn("Uniform buffer resides in GPU memory. This will cause a performance penalty if the buffer is updated often (per frame).");
    }
  }
  else {
    BRThrowException(Stz "Invalid buffer type '" + (int)_eType + "'.");
  }

  if (_bUseStagingBuffer) {
    //Create a staging getVkBuffer for efficiency operations
    //Staging buffers only make sense for data that resides on the GPu and doesn't get updated per frame.
    // Uniform data is not a goojd option for staging buffers, but mesh and bone data is a good option.
    _hostBuffer = std::make_shared<VulkanDeviceBuffer>(vulkan(), itemSize, itemCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    _gpuBuffer = std::make_shared<VulkanDeviceBuffer>(vulkan(), itemSize, itemCount, VK_BUFFER_USAGE_TRANSFER_DST_BIT | bufType, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  }
  else {
    //Allocate non-staged
    _hostBuffer = std::make_shared<VulkanDeviceBuffer>(vulkan(), itemSize, itemCount, bufType, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  }

  if (items != nullptr) {
    writeData(items, item_count, 0);
  }
}
VulkanBuffer::~VulkanBuffer() {
  _gpuBuffer = nullptr;
  _hostBuffer = nullptr;
}
void VulkanBuffer::writeData(void* items, size_t item_count, size_t item_offset) {
  if (_bUseStagingBuffer) {
    AssertOrThrow2(_hostBuffer != nullptr);
    AssertOrThrow2(_gpuBuffer != nullptr);
    _hostBuffer->copy_from(items, item_count, 0, 0);
    _gpuBuffer->copy_device(_hostBuffer, item_count, 0, 0);

    //DELETING HOST BUFFER ** If we decide to dynamically copy vertexes we will need to specify a MemoryType This is a segue into memory pools
    //HostOnly, DeviceOnly, Coherent (copy)
    _hostBuffer = nullptr;
  }
  else {
    AssertOrThrow2(_hostBuffer != nullptr);
    AssertOrThrow2(_gpuBuffer == nullptr);
    _hostBuffer->copy_from(items, item_count, 0, 0);
  }
}
std::shared_ptr<VulkanDeviceBuffer> VulkanBuffer::buffer() {
  if (_bUseStagingBuffer) {
    AssertOrThrow2(_gpuBuffer);
    return _gpuBuffer;
  }
  else {
    AssertOrThrow2(_hostBuffer);
    return _hostBuffer;
  }
}

#pragma endregion

#pragma region TextureImage
TextureImage::TextureImage(std::shared_ptr<Vulkan> v, const string_t& name, TextureType type, MSAA samples, const FilterData& filter) : VulkanObject(v) {
  _type = type;
  _filter = filter;
  _samples = samples;
  _name = name;
  if (_filter._anisotropy > v->deviceLimits().maxSamplerAnisotropy) {
    _filter._anisotropy = v->deviceLimits().maxSamplerAnisotropy;
  }
  else if (_filter._anisotropy <= 0) {
    _filter._anisotropy = 0;  // Disabled
  }
}
TextureImage::TextureImage(std::shared_ptr<Vulkan> v, const string_t& name, TextureType type, MSAA samples, const BR2::usize2& size, VkFormat format, const FilterData& filter) : TextureImage(v, name, type, samples, filter) {
  //Depth format constructor
  cleanup();
  _format = format;
  _size = size;

  if (!computeTypeProperties()) {
    return;
  }

  computeMipLevels();
  createGPUImage();
  createView();
  createSampler();
  generateMipmaps();
}
TextureImage::TextureImage(std::shared_ptr<Vulkan> v, const string_t& name, TextureType type, MSAA samples, const BR2::usize2& size, VkFormat format, VkImage image, const FilterData& filter) : TextureImage(v, name, type, samples, filter) {
  //Swapchain Image Constructor
  cleanup();
  _image = image;
  _format = format;
  _size = size;

  computeMipLevels();
  createSampler();
  createView();
  generateMipmaps();
}
TextureImage::TextureImage(std::shared_ptr<Vulkan> v, const string_t& name, TextureType type, MSAA samples, std::shared_ptr<Img32> pimg, const FilterData& filter) : TextureImage(v, name, type, samples, filter) {
  AssertOrThrow2(pimg != nullptr);
  cleanup();
  _bitmap = pimg;
  _size = _bitmap->size();
  _format = _bitmap->format();

  if (!computeTypeProperties()) {
    return;
  }

  computeMipLevels();
  createGPUImage();
  copyImageToGPU();
  createView();
  createSampler();
  generateMipmaps();
}
TextureImage::~TextureImage() {
  cleanup();
}
void TextureImage::computeMipLevels() {
  if (_filter._mipmap != MipmapMode::Disabled) {
    _filter._mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(_size.width, _size.height)))) + 1;
    _transferSrc = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;  //We need to use this image as a src transfer to fill each mip level.
  }
  else {
    _filter._mipLevels = 1;
  }
}
bool TextureImage::computeTypeProperties() {
  if (_type == TextureType::ColorTexture) {
    _tiling = VK_IMAGE_TILING_OPTIMAL;
    _usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | _transferSrc;
    _properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    _aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    _initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  //VK_IMAGE_LAYOUT_GENERAL
    _finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }
  else if (_type == TextureType::ColorAttachment) {
    _tiling = VK_IMAGE_TILING_OPTIMAL;
    //MSAA -
    //VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT/
    _usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | _transferSrc;
    _properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    _aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    _initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    _finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }
  else if (_type == TextureType::DepthAttachment) {
    _tiling = VK_IMAGE_TILING_OPTIMAL;
    _usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    _properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    _aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    _initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    _finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  }
  else {
    _error = true;
    BRLogError("Invalid texture type '" + std::to_string((int)_type) + "'");
    Gu::debugBreak();
    return false;
  }
  return true;
}
void TextureImage::cleanup() {
  vulkan()->waitIdle();
  if (_textureSampler != VK_NULL_HANDLE) {
    vkDestroySampler(vulkan()->device(), _textureSampler, nullptr);
  }
  if (_image != VK_NULL_HANDLE) {
    vkDestroyImage(vulkan()->device(), _image, nullptr);
  }
  if (_imageMemory != VK_NULL_HANDLE) {
    vkFreeMemory(vulkan()->device(), _imageMemory, nullptr);
  }
  if (_imageView != VK_NULL_HANDLE) {
    vkDestroyImageView(vulkan()->device(), _imageView, nullptr);
  }
  _image = VK_NULL_HANDLE;  // If this is a VulkanBufferType::Image
  _imageMemory = VK_NULL_HANDLE;
  _imageView = VK_NULL_HANDLE;
  _textureSampler = VK_NULL_HANDLE;
  _error = false;
}
void TextureImage::createGPUImage() {
  AssertOrThrow2(_format != VK_FORMAT_UNDEFINED && _size.width > 0 && _size.height > 0);
  AssertOrThrow2(_image == VK_NULL_HANDLE);

  if (_filter._mipLevels < 1) {
    BRLogError("Miplevels was < 1 for image. Setting to 1");
    _filter._mipLevels = 1;
  }

  VkSampleCountFlagBits vksamples = multisampleToVkSampleCountFlagBits(_samples);

  if (vksamples != VK_SAMPLE_COUNT_1_BIT) {
    int n = 0;
    n++;  //testing;
  }

  VkImageCreateInfo imageInfo = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = _format,
    .extent = {
      .width = _size.width,
      .height = _size.height,
      .depth = 1 },
    .mipLevels = _filter._mipLevels,
    .arrayLayers = 1,
    .samples = vksamples,
    .tiling = _tiling,
    .usage = _usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = nullptr,
    .initialLayout = _initialLayout,
  };
  CheckVKR(vkCreateImage, vulkan()->device(), &imageInfo, nullptr, &_image);

  VkMemoryRequirements mem_req;
  vkGetImageMemoryRequirements(vulkan()->device(), _image, &mem_req);

  BRLogInfo("Allocating image memory: " + std::to_string((int)mem_req.size) + "B");
  VkMemoryAllocateInfo allocInfo = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .pNext = nullptr,
    .allocationSize = mem_req.size,
    .memoryTypeIndex = VulkanDeviceBuffer::findMemoryType(vulkan()->physicalDevice(), mem_req.memoryTypeBits, _properties)
  };
  CheckVKR(vkAllocateMemory, vulkan()->device(), &allocInfo, nullptr, &_imageMemory);
  CheckVKR(vkBindImageMemory, vulkan()->device(), _image, _imageMemory, 0);
}
uint32_t TextureImage::msaa_to_int(MSAA s) {
  if (s == MSAA::Disabled) {
    return 1;
  }
  else if (s == MSAA::MS_2_Samples) {
    return 2;
  }
  else if (s == MSAA::MS_4_Samples) {
    return 4;
  }
  else if (s == MSAA::MS_8_Samples) {
    return 8;
  }
  else if (s == MSAA::MS_16_Samples) {
    return 16;
  }
  else if (s == MSAA::MS_32_Samples) {
    return 32;
  }
  else if (s == MSAA::MS_64_Samples) {
    return 64;
  }
  else {
    BRThrowException("Unhandled multisample state (beyond maximum) s='" + std::to_string((int)s) + "'");
  }
}
VkSampleCountFlagBits TextureImage::multisampleToVkSampleCountFlagBits(MSAA s) {
  VkSampleCountFlagBits ret = VK_SAMPLE_COUNT_1_BIT;
  if (s == MSAA::Disabled) {
    ret = VK_SAMPLE_COUNT_1_BIT;
  }
  else if (s == MSAA::MS_2_Samples) {
    ret = VK_SAMPLE_COUNT_2_BIT;
  }
  else if (s == MSAA::MS_4_Samples) {
    ret = VK_SAMPLE_COUNT_4_BIT;
  }
  else if (s == MSAA::MS_8_Samples) {
    ret = VK_SAMPLE_COUNT_8_BIT;
  }
  else if (s == MSAA::MS_16_Samples) {
    ret = VK_SAMPLE_COUNT_16_BIT;
  }
  else if (s == MSAA::MS_32_Samples) {
    ret = VK_SAMPLE_COUNT_32_BIT;
  }
  else if (s == MSAA::MS_64_Samples) {
    ret = VK_SAMPLE_COUNT_64_BIT;
  }
  else {
    BRLogError("Unhandled multisample state (beyond maximum) s='" + std::to_string((int)s) + "'");
    Gu::debugBreak();
  }
  return ret;
}
VkFilter TextureImage::convertFilter(TexFilter in_filter, bool cubicSupported) {
  VkFilter ret = VK_FILTER_LINEAR;
  if (in_filter == TexFilter::Linear) {
    ret = VK_FILTER_LINEAR;
  }
  else if (in_filter == TexFilter::Cubic) {
    ret = VK_FILTER_CUBIC_IMG;
  }
  else if (in_filter == TexFilter::Nearest) {
    ret = VK_FILTER_NEAREST;
  }
  else {
    BRLogError("Invalid TexFilter mode.");
  }

  if (ret == VK_FILTER_CUBIC_IMG && !cubicSupported) {
    BRLogError("Cubic interpolation not supported.");
    ret = VK_FILTER_LINEAR;
  }

  return ret;
}
void TextureImage::createSampler() {
  if (_filter._samplerType == SamplerType::None) {
    return;
  }

  //This may not exactly work for every case, but the reason I use nearest filtering is
  //for game sprites or FBO's. In that case we want the mipmap interpolation to be nearest.
  bool cubicFilteringSupported = isFeatureSupported(VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_CUBIC_BIT_IMG);
  VkFilter minFilter = convertFilter(_filter._min_filter, cubicFilteringSupported);
  VkFilter magFilter = convertFilter(_filter._mag_filter, cubicFilteringSupported);
  VkSamplerMipmapMode mipMode = TextureImage::convertMipmapMode(_filter._mipmap, _filter._mag_filter);

  VkBool32 anisotropy_enable = VK_TRUE;
  if (_filter._anisotropy < 1.0f) {
    anisotropy_enable = VK_FALSE;
  }

  VkSamplerCreateInfo samplerInfo = {
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    //Filtering
    .magFilter = magFilter,
    .minFilter = minFilter,
    .mipmapMode = mipMode,
    //Texture repeat
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .mipLodBias = 0,
    .anisotropyEnable = anisotropy_enable,
    .maxAnisotropy = anisotropy_enable ? _filter._anisotropy : 1,  //Note: Gpu will segfault if aniso is less than 1
    .compareEnable = VK_FALSE,
    .compareOp = VK_COMPARE_OP_ALWAYS,
    .minLod = 0,
    .maxLod = static_cast<float>(_filter._mipLevels),
    .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    .unnormalizedCoordinates = VK_FALSE,
  };
  CheckVKR(vkCreateSampler, vulkan()->device(), &samplerInfo, nullptr, &_textureSampler);
}
bool TextureImage::isFeatureSupported(VkFormatFeatureFlagBits flag) {
  VkFormatProperties formatProperties;
  vkGetPhysicalDeviceFormatProperties(vulkan()->physicalDevice(), format(), &formatProperties);
  bool supported = formatProperties.optimalTilingFeatures & flag;
  return supported;
}
void TextureImage::generateMipmaps(std::shared_ptr<CommandBuffer> buf) {
  if (_filter._mipmap == MipmapMode::Disabled || _filter._mipLevels == 1) {
    return;
  }
  if (!isFeatureSupported(VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
    BRLogWarnOnce("Mipmapping is not supported.");
    return;
  }

  bool destroyBuf = false;
  if (buf == nullptr) {
    buf = std::make_shared<CommandBuffer>(vulkan(), nullptr);
    buf->begin();
    destroyBuf = true;
  }

  int32_t last_level_width = static_cast<int32_t>(_size.width);
  int32_t last_level_height = static_cast<int32_t>(_size.height);
  for (uint32_t iMipLevel = 1; iMipLevel < _filter._mipLevels; ++iMipLevel) {
    int32_t level_width = last_level_width / 2;
    int32_t level_height = last_level_height / 2;

    buf->blitImage(_image,
                   _image,
                   { 0, 0, static_cast<int32_t>(last_level_width), static_cast<int32_t>(last_level_height) },
                   { 0, 0, static_cast<int32_t>(level_width), static_cast<int32_t>(level_height) },
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   iMipLevel - 1, iMipLevel,
                   VK_IMAGE_ASPECT_COLOR_BIT,
                   (_filter._mipmap == MipmapMode::Nearest) ? (VK_FILTER_NEAREST) : (VK_FILTER_LINEAR));
    buf->imageTransferBarrier(_image,
                              VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                              iMipLevel - 1, VK_IMAGE_ASPECT_COLOR_BIT);
    buf->imageTransferBarrier(_image,
                              VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _finalLayout,
                              iMipLevel - 1, VK_IMAGE_ASPECT_COLOR_BIT);
    if (level_width > 1) {
      last_level_width /= 2;
    }
    if (level_height > 1) {
      last_level_height /= 2;
    }
  }
  //Transfer the last mip level to shader optimal
  buf->imageTransferBarrier(_image,
                            VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _finalLayout,
                            _filter._mipLevels - 1, VK_IMAGE_ASPECT_COLOR_BIT);
  if (destroyBuf) {
    buf->end();
    buf->submit({}, {}, {}, 0, true);
  }
}
void TextureImage::copyImageToGPU() {
  if (_bitmap == nullptr) {
    return;
  }
  //**Note this assumes a color texture: see  VK_IMAGE_ASPECT_COLOR_BIT
  //For loaded images only.
  auto buf = std::make_shared<VulkanDeviceBuffer>(vulkan(),
                                                  1,  // 1 byte - TODO - this should be the size of a pixel and pixel count
                                                  _bitmap->data_len_bytes,
                                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  buf->copy_from(_bitmap->_data, _bitmap->data_len_bytes, 0, 0);

  //Undefined layout will be discard image data.
  transitionImageLayout(_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  CommandBuffer cmd(vulkan(), nullptr);
  cmd.begin();
  cmd.copyBufferToImage(buf, _image, _size);
  cmd.end();
  cmd.submit({}, {}, {}, 0, true);
  transitionImageLayout(_format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  //Cleanup host getVkBuffer. We are done with it.
  buf = nullptr;
}
std::shared_ptr<Img32> TextureImage::copyImageFromGPU() {
  vulkan()->waitIdle();
  size_t size_bytes = _size.width * _size.height * 4;
  //**Note this assumes a color texture: see  VK_IMAGE_ASPECT_COLOR_BIT
  //For loaded images only.
  auto buf = std::make_shared<VulkanDeviceBuffer>(vulkan(),
                                                  1,
                                                  size_bytes,  //We must convert the image to 4 components unsigned byte
                                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  //Undefined layout will be discard image data.
  //transitionImageLayout(_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  CommandBuffer cmd(vulkan(), nullptr);
  cmd.begin();
  cmd.copyImageToBuffer(getThis<TextureImage>(), buf);
  cmd.end();
  cmd.submit();
  //transitionImageLayout(_format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  std::shared_ptr<Img32> image = std::make_shared<Img32>();
  image->_size.width = _size.width;
  image->_size.height = _size.height;
  image->_vkformat = _format;
  image->_data = new unsigned char[size_bytes];
  image->data_len_bytes = size_bytes;
  buf->copy_to(image->_data, size_bytes, 0, 0);
  buf = nullptr;

  return image;
}
void TextureImage::transitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
  auto commandBuffer = vulkan()->beginOneTimeGraphicsCommands();
  //https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#synchronization-access-types-supported

  //We can make this better .. later.
  //LayoutMap(srfc, dst, srcMask, dstMask, srcStage, dstStage)
  VkAccessFlagBits srcAccessMask;
  VkAccessFlagBits dstAccessMask;
  VkPipelineStageFlagBits srcStage;
  VkPipelineStageFlagBits dstStage;
  //#define LayoutMap(oldL, newL, srcMask, dstMask, srcStage, dstStage) \
//  if (oldLayout == oldL && newLayout == newL) {                     \
//    srcAccessMask = (VkAccessFlagBits)srcMask;                      \
//    dstAccessMask = (VkAccessFlagBits)dstMask;                      \
//    srcStage = srcStage;                                            \
//    dstStage = dstStage;                                            \
//  }
  //
  //  LayoutMap(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)

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
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .pNext = nullptr,
    .srcAccessMask = (VkAccessFlags)srcAccessMask,
    .dstAccessMask = (VkAccessFlags)dstAccessMask,
    .oldLayout = oldLayout,
    .newLayout = newLayout,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = _image,
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = _filter._mipLevels,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
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
void TextureImage::flipImage20161206(uint8_t* image, int width, int height) {
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
VkSamplerMipmapMode TextureImage::convertMipmapMode(MipmapMode mode, TexFilter filter) {
  VkSamplerMipmapMode ret = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  if (mode == MipmapMode::Linear) {
    ret = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  }
  else if (mode == MipmapMode::Nearest) {
    ret = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  }
  else if (mode == MipmapMode::Disabled) {
  }
  else {
    BRLogError("Invalid mipmap mode.");
  }
  return ret;
}
void TextureImage::testCycleFilters(TexFilter& g_min_filter, TexFilter& g_mag_filter, MipmapMode& g_mipmap_mode) {
  //Cycle through all texture filters for testing.
  if (g_mipmap_mode == MipmapMode::Nearest) {
    if (g_min_filter == TexFilter::Nearest) {
      if (g_mag_filter == TexFilter::Nearest) {
        g_mipmap_mode = MipmapMode::Nearest;
        g_min_filter = TexFilter::Nearest;
        g_mag_filter = TexFilter::Linear;
      }
      else if (g_mag_filter == TexFilter::Linear) {
        g_mipmap_mode = MipmapMode::Nearest;
        g_min_filter = TexFilter::Nearest;
        g_mag_filter = TexFilter::Cubic;
      }
      else if (g_mag_filter == TexFilter::Cubic) {
        g_mipmap_mode = MipmapMode::Nearest;
        g_min_filter = TexFilter::Linear;
        g_mag_filter = TexFilter::Nearest;
      }
    }
    else if (g_min_filter == TexFilter::Linear) {
      if (g_mag_filter == TexFilter::Nearest) {
        g_mipmap_mode = MipmapMode::Nearest;
        g_min_filter = TexFilter::Linear;
        g_mag_filter = TexFilter::Linear;
      }
      else if (g_mag_filter == TexFilter::Linear) {
        g_mipmap_mode = MipmapMode::Nearest;
        g_min_filter = TexFilter::Linear;
        g_mag_filter = TexFilter::Cubic;
      }
      else if (g_mag_filter == TexFilter::Cubic) {
        g_mipmap_mode = MipmapMode::Nearest;
        g_min_filter = TexFilter::Cubic;
        g_mag_filter = TexFilter::Nearest;
      }
    }
    else if (g_min_filter == TexFilter::Cubic) {
      if (g_mag_filter == TexFilter::Nearest) {
        g_mipmap_mode = MipmapMode::Nearest;
        g_min_filter = TexFilter::Cubic;
        g_mag_filter = TexFilter::Linear;
      }
      else if (g_mag_filter == TexFilter::Linear) {
        g_mipmap_mode = MipmapMode::Nearest;
        g_min_filter = TexFilter::Cubic;
        g_mag_filter = TexFilter::Cubic;
      }
      else if (g_mag_filter == TexFilter::Cubic) {
        g_mipmap_mode = MipmapMode::Linear;
        g_min_filter = TexFilter::Nearest;
        g_mag_filter = TexFilter::Nearest;
      }
    }
  }
  else if (g_mipmap_mode == MipmapMode::Linear) {
    if (g_min_filter == TexFilter::Nearest) {
      if (g_mag_filter == TexFilter::Nearest) {
        g_mipmap_mode = MipmapMode::Linear;
        g_min_filter = TexFilter::Nearest;
        g_mag_filter = TexFilter::Linear;
      }
      else if (g_mag_filter == TexFilter::Linear) {
        g_mipmap_mode = MipmapMode::Linear;
        g_min_filter = TexFilter::Nearest;
        g_mag_filter = TexFilter::Cubic;
      }
      else if (g_mag_filter == TexFilter::Cubic) {
        g_mipmap_mode = MipmapMode::Linear;
        g_min_filter = TexFilter::Linear;
        g_mag_filter = TexFilter::Nearest;
      }
    }
    else if (g_min_filter == TexFilter::Linear) {
      if (g_mag_filter == TexFilter::Nearest) {
        g_mipmap_mode = MipmapMode::Linear;
        g_min_filter = TexFilter::Linear;
        g_mag_filter = TexFilter::Linear;
      }
      else if (g_mag_filter == TexFilter::Linear) {
        g_mipmap_mode = MipmapMode::Linear;
        g_min_filter = TexFilter::Linear;
        g_mag_filter = TexFilter::Cubic;
      }
      else if (g_mag_filter == TexFilter::Cubic) {
        g_mipmap_mode = MipmapMode::Linear;
        g_min_filter = TexFilter::Cubic;
        g_mag_filter = TexFilter::Nearest;
      }
    }
    else if (g_min_filter == TexFilter::Cubic) {
      if (g_mag_filter == TexFilter::Nearest) {
        g_mipmap_mode = MipmapMode::Linear;
        g_min_filter = TexFilter::Cubic;
        g_mag_filter = TexFilter::Linear;
      }
      else if (g_mag_filter == TexFilter::Linear) {
        g_mipmap_mode = MipmapMode::Linear;
        g_min_filter = TexFilter::Cubic;
        g_mag_filter = TexFilter::Cubic;
      }
      else if (g_mag_filter == TexFilter::Cubic) {
        g_mipmap_mode = MipmapMode::Disabled;
        g_min_filter = TexFilter::Nearest;
        g_mag_filter = TexFilter::Nearest;
      }
    }
  }
  else if (g_mipmap_mode == MipmapMode::Disabled) {
    if (g_min_filter == TexFilter::Nearest) {
      if (g_mag_filter == TexFilter::Nearest) {
        g_mipmap_mode = MipmapMode::Disabled;
        g_min_filter = TexFilter::Nearest;
        g_mag_filter = TexFilter::Linear;
      }
      else if (g_mag_filter == TexFilter::Linear) {
        g_mipmap_mode = MipmapMode::Disabled;
        g_min_filter = TexFilter::Nearest;
        g_mag_filter = TexFilter::Cubic;
      }
      else if (g_mag_filter == TexFilter::Cubic) {
        g_mipmap_mode = MipmapMode::Disabled;
        g_min_filter = TexFilter::Linear;
        g_mag_filter = TexFilter::Nearest;
      }
    }
    else if (g_min_filter == TexFilter::Linear) {
      if (g_mag_filter == TexFilter::Nearest) {
        g_mipmap_mode = MipmapMode::Disabled;
        g_min_filter = TexFilter::Linear;
        g_mag_filter = TexFilter::Linear;
      }
      else if (g_mag_filter == TexFilter::Linear) {
        g_mipmap_mode = MipmapMode::Disabled;
        g_min_filter = TexFilter::Linear;
        g_mag_filter = TexFilter::Cubic;
      }
      else if (g_mag_filter == TexFilter::Cubic) {
        g_mipmap_mode = MipmapMode::Disabled;
        g_min_filter = TexFilter::Cubic;
        g_mag_filter = TexFilter::Nearest;
      }
    }
    else if (g_min_filter == TexFilter::Cubic) {
      if (g_mag_filter == TexFilter::Nearest) {
        g_mipmap_mode = MipmapMode::Disabled;
        g_min_filter = TexFilter::Cubic;
        g_mag_filter = TexFilter::Linear;
      }
      else if (g_mag_filter == TexFilter::Linear) {
        g_mipmap_mode = MipmapMode::Disabled;
        g_min_filter = TexFilter::Cubic;
        g_mag_filter = TexFilter::Cubic;
      }
      else if (g_mag_filter == TexFilter::Cubic) {
        g_mipmap_mode = MipmapMode::Nearest;
        g_min_filter = TexFilter::Nearest;
        g_mag_filter = TexFilter::Nearest;
      }
    }
  }
}
void TextureImage::createView() {
  AssertOrThrow2(_image != VK_NULL_HANDLE);
  AssertOrThrow2(_format != VK_FORMAT_UNDEFINED);
  AssertOrThrow2(_filter._mipLevels >= 1);

  VkImageViewCreateInfo createInfo = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .image = _image,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = _format,
    .components = {
      .r = VK_COMPONENT_SWIZZLE_IDENTITY,
      .g = VK_COMPONENT_SWIZZLE_IDENTITY,
      .b = VK_COMPONENT_SWIZZLE_IDENTITY,
      .a = VK_COMPONENT_SWIZZLE_IDENTITY,
    },
    .subresourceRange = {
      .aspectMask = _aspect,
      .baseMipLevel = 0,
      .levelCount = _filter._mipLevels,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
  };

  CheckVKR(vkCreateImageView, vulkan()->device(), &createInfo, nullptr, &_imageView);
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
  if (_state != CommandBufferState::Submit) {
    BRLogWarn("Command buffer wasn't submitted before being destroyed.");
    Gu::debugBreak();
  }
  vkFreeCommandBuffers(vulkan()->device(), _sharedPool, 1, &_commandBuffer);
}
void CommandBuffer::validateState(bool b) {
  if (!b) {
    Gu::debugBreak();
    BRThrowException("Invalid Command Buffer State.");
  }
}
void CommandBuffer::begin() {
  validateState(_state == CommandBufferState::Submit || _state == CommandBufferState::Unset);

  //VkCommandBufferResetFlags
  CheckVKR(vkResetCommandBuffer, _commandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

  VkCommandBufferBeginInfo beginInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .pNext = nullptr,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,  //,
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
void CommandBuffer::submit(std::vector<VkPipelineStageFlags> waitStages, std::vector<VkSemaphore> waitSemaphores, std::vector<VkSemaphore> signalSemaphores, VkFence submitFence, bool waitIdle) {
  validateState(_state == CommandBufferState::End);

  VkCommandBuffer buf = getVkCommandBuffer();

  if (waitStages.size() == 0 && waitSemaphores.size() == 0 && submitFence == VK_NULL_HANDLE && waitIdle == false) {
    BRLogWarnCycle("No sync objects specified for CommandBuffer::submit and command waitidle is not set either - random behavior will occur.");
  }

  AssertOrThrow2(waitStages.size() == waitSemaphores.size());  //these correspond.

  VkSubmitInfo submitInfo = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pNext = nullptr,
    .waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size()),
    .pWaitSemaphores = waitSemaphores.data(),
    .pWaitDstStageMask = waitStages.data(),
    .commandBufferCount = 1,
    .pCommandBuffers = &buf,
    .signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size()),
    .pSignalSemaphores = signalSemaphores.data(),
  };
  CheckVKR(vkQueueSubmit, vulkan()->graphicsQueue(), 1, &submitInfo, submitFence);  //submitFence is signaled once all buffers complete execution

  if (waitIdle) {
    CheckVKR(vkQueueWaitIdle, vulkan()->graphicsQueue());
  }

  _state = CommandBufferState::Submit;
}
bool CommandBuffer::beginPass() {
  validateState(_state == CommandBufferState::Begin || _state == CommandBufferState::EndPass);

  _state = CommandBufferState::BeginPass;
  return true;
}
void CommandBuffer::cmdSetViewport(const BR2::urect2& extent) {
  validateState(_state == CommandBufferState::BeginPass);
  AssertOrThrow2(_pRenderFrame != nullptr);
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
    .width = _pRenderFrame->getSwapchain()->windowSize().width,
    .height = _pRenderFrame->getSwapchain()->windowSize().height
  };
  vkCmdSetViewport(_commandBuffer, 0, 1, &viewport);
  vkCmdSetScissor(_commandBuffer, 0, 1, &scissor);
}
void CommandBuffer::endPass() {
  validateState(_state == CommandBufferState::BeginPass);
  //This is called once when all pipeline rendering is complete
  vkCmdEndRenderPass(_commandBuffer);

  _pBoundIndexes = nullptr;

  _state = CommandBufferState::EndPass;
}
void CommandBuffer::blitImage(VkImage srcImg, VkImage dstImg, const BR2::irect2& srcRegion, const BR2::irect2& dstRegion,
                              VkImageLayout srcLayout, VkImageLayout dstLayout, uint32_t srcMipLevel, uint32_t dstMipLevel,
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
  vkCmdBlitImage(_commandBuffer,
                 srcImg, srcLayout,
                 dstImg, dstLayout,
                 1, &blit,
                 filter);
}
void CommandBuffer::imageTransferBarrier(VkImage image, VkAccessFlagBits srcAccessFlags, VkAccessFlagBits dstAccessFlags,
                                         VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t baseMipLevel, VkImageAspectFlagBits subresourceMask) {
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
  vkCmdPipelineBarrier(_commandBuffer,
                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                       0,
                       0, nullptr,
                       0, nullptr,
                       1, &barrier);
}
void CommandBuffer::copyBuffer(VkBuffer from, VkBuffer to, size_t count, size_t from_offset, size_t to_offset) {
  validateState(_state == CommandBufferState::Begin || _state == CommandBufferState::BeginPass);
  VkBufferCopy copyRegion{
    .srcOffset = from_offset,
    .dstOffset = to_offset,
    .size = count,
  };
  vkCmdCopyBuffer(_commandBuffer, from, to, 1, &copyRegion);
}
void CommandBuffer::copyBufferToImage(std::shared_ptr<VulkanDeviceBuffer> buf, VkImage img, const BR2::usize2& size) {
  validateState(_state == CommandBufferState::Begin || _state == CommandBufferState::BeginPass);

  VkBufferImageCopy region = {
    .bufferOffset = 0,
    .bufferRowLength = 0,
    .bufferImageHeight = 0,
    .imageSubresource = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .mipLevel = 0,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
    .imageOffset = { 0, 0, 0 },
    .imageExtent = { size.width, size.height, 1 },
  };

  vkCmdCopyBufferToImage(_commandBuffer, buf->getVkBuffer(), img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}
void CommandBuffer::copyImageToBuffer(std::shared_ptr<TextureImage> image, std::shared_ptr<VulkanDeviceBuffer> buf) {
  validateState(_state == CommandBufferState::Begin || _state == CommandBufferState::BeginPass);

  VkBufferImageCopy region = {
    .bufferOffset = 0,
    .bufferRowLength = 0,
    .bufferImageHeight = 0,
    .imageSubresource = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .mipLevel = 0,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
    .imageOffset = { 0, 0, 0 },
    .imageExtent = { image->imageSize().width, image->imageSize().height, 1 },
  };

  vkCmdCopyImageToBuffer(_commandBuffer, image->image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf->getVkBuffer(), 1, &region);
}
void CommandBuffer::bindMesh(std::shared_ptr<Mesh> mesh) {
  validateState(_state == CommandBufferState::BeginPass);
  std::shared_ptr<VulkanBuffer> verts = mesh->vertexBuffer();
  std::shared_ptr<VulkanBuffer> indexes = mesh->indexBuffer();
  IndexType indexType = mesh->indexType();

  VkIndexType vulkan_idxtype = VK_INDEX_TYPE_UINT32;
  if (indexType == IndexType::IndexTypeUint32) {
    vulkan_idxtype = VK_INDEX_TYPE_UINT32;
    AssertOrThrow2(mesh->indexBuffer()->buffer()->itemSize() == 4);
  }
  else if (indexType == IndexType::IndexTypeUint16) {
    vulkan_idxtype = VK_INDEX_TYPE_UINT16;
    AssertOrThrow2(mesh->indexBuffer()->buffer()->itemSize() == 2);
  }
  else {
    BRThrowException("Invalid index type.");
  }

  VkBuffer vertexBuffers[] = { verts->buffer()->getVkBuffer() };
  VkDeviceSize offsets[] = { 0 };
  vkCmdBindVertexBuffers(_commandBuffer, 0, 1, vertexBuffers, offsets);
  vkCmdBindIndexBuffer(_commandBuffer, indexes->buffer()->getVkBuffer(), 0, vulkan_idxtype);

  _pBoundIndexes = indexes;  //Cleared at end of pass.
}
void CommandBuffer::drawIndexed(uint32_t instanceCount) {
  validateState(_state == CommandBufferState::BeginPass);
  AssertOrThrow2(_pBoundIndexes != nullptr && _pBoundIndexes->buffer() != nullptr);
  uint32_t ind_count = static_cast<uint32_t>(_pBoundIndexes->buffer()->itemCount());
  vkCmdDrawIndexed(_commandBuffer, ind_count, instanceCount, 0, 0, 0);
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
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .pNext = nullptr,
      .flags = (VkPipelineShaderStageCreateFlags)0,
      .stage = type,
      .module = _vkShaderModule,
      .pName = _spvReflectModule->entry_point_name,
      .pSpecializationInfo = nullptr,
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
  else if (out == OutputMRT::RT_DefaultColor) {
    return FBOType::Color;
  }
  else if (out == OutputMRT::RT_DefaultDepth) {
    return FBOType::Depth;
  }
  else if (out == OutputMRT::RT_DF_Position) {
    return FBOType::Color;
  }
  else if (out == OutputMRT::RT_DF_Color) {
    return FBOType::Color;
  }
  else if (out == OutputMRT::RT_DF_Depth_Plane) {
    return FBOType::Color;
  }
  else if (out == OutputMRT::RT_DF_Normal) {
    return FBOType::Color;
  }
  else if (out == OutputMRT::RT_DF_Pick) {
    return FBOType::Color;
  }
  return FBOType::Undefined;
}

#pragma endregion

#pragma region RenderTexture

RenderTexture::RenderTexture(const string_t& name, std::shared_ptr<Swapchain> s, VkFormat format, const FilterData& filter) {
  _format = format;
  _filter = filter;
  _swapchain = s;
  _name = name;
}
RenderTexture::~RenderTexture() {
  _textures.clear();
}
void RenderTexture::recreateAllTextures() {
  _swapchain->vulkan()->waitIdle();
  std::vector<MSAA> samples;
  for (auto pair : _textures) {
    samples.push_back(pair.first);
  }
  _textures.clear();
  for (auto sample : samples) {
    createTexture(sample);
  }
}
void RenderTexture::createTexture(MSAA msaa) {
  auto it = _textures.find(msaa);
  if (it != _textures.end()) {
    BRThrowException("Attempted to create duplicate RenderTexture for sample count '" + std::to_string((int)msaa) + "' ");
  }
  std::vector<std::shared_ptr<TextureImage>> frame_texs;

  if (_textures.size() >= 2) {
    BRThrowException("Too many MSAA sampled textures in RenderTexture - limit 2");
  }

  for (auto frame : _swapchain->frames()) {
    auto tex = std::make_shared<TextureImage>(_swapchain->vulkan(),
                                              _name,
                                              TextureType::ColorAttachment,
                                              msaa,
                                              _swapchain->windowSize(),
                                              _format,
                                              _filter);
    frame_texs.push_back(tex);
  }
  _textures.insert(std::make_pair(msaa, frame_texs));
}
std::shared_ptr<TextureImage> RenderTexture::texture(MSAA msaa, uint32_t frame) {
  auto it = _textures.find(msaa);
  if (it == _textures.end()) {
    return nullptr;
  }
  AssertOrThrow2(frame < it->second.size());

  auto tex = it->second[frame];
  AssertOrThrow2(tex->imageSize() == _swapchain->windowSize());  //Sanity check.

  return tex;
}

#pragma endregion

#pragma region PassDescription

PassDescription::PassDescription(std::shared_ptr<RenderFrame> frame, std::shared_ptr<PipelineShader> shader, MSAA c, BlendFunc globalBlend, FramebufferBlendMode rbm) {
  _shader = shader;
  _frame = frame;
  _sampleCount = c;
  _blendMode = rbm;
  _globalBlend = globalBlend;

  bool ib = (frame->vulkan()->deviceFeatures().independentBlend == VK_TRUE);
  if (!ib && _blendMode == FramebufferBlendMode::Independent) {
    passError("In PassDescription: Independent blending is not supported on your GPU. Use 'Global' or allow a Default blending.");
  }

  // Check Multisampling
  int s_a = VulkanUtils::SampleCount_ToInt(shader->vulkan()->maxMSAA());
  int s_b = VulkanUtils::SampleCount_ToInt(_sampleCount);
  if (s_a < s_b) {
    BRLogWarnOnce("Supplied multisample count '" + std::to_string(s_b) + "' was greater than the GPU max supported '" + std::to_string(s_a) + "'.");
    _sampleCount = shader->vulkan()->maxMSAA();
  }
}
void PassDescription::setOutput(std::shared_ptr<OutputDescription> output) {
  addValidOutput(output);
}
void PassDescription::setOutput(const string_t& tag, OutputMRT output_e, std::shared_ptr<RenderTexture> tex,
                                BlendFunc blend, bool clear, float clear_r, float clear_g, float clear_b) {
  auto output = std::make_shared<OutputDescription>();
  output->_name = tag;
  output->_texture = tex;
  output->_blending = blend;
  //**TODO: if we set a depth texture here, we need to set the correct depth/stencil clear value.
  output->_type = OutputDescription::outputTypeToFBOType(output_e);
  output->_clearColor = { clear_r, clear_g, clear_b, 1 };
  output->_clearDepth = 1;
  output->_clearStencil = 0;
  output->_output = output_e;
  output->_clear = clear;
  output->_compareOp = CompareOp::Less;
  addValidOutput(output);
}
bool PassDescription::passError(const string_t& msg) {
  string_t out_msg = "[PassDescription]:" + msg;
  BRLogError(out_msg);
  _bValid = false;
  return false;
}
void PassDescription::addValidOutput(std::shared_ptr<OutputDescription> out_att) {
  //Adds an output description to the pass and assigns it a shader binding.
  AssertOrThrow2(_shader != nullptr);
  bool valid = true;
  bool found_binding = false;
  for (auto& binding : _shader->outputBindings()) {
    if (binding->_output == OutputMRT::RT_Undefined) {
      //**If you get here then you didn't name the variable correctly in the shader.
      passError("Output MRT was not set for binding '" + binding->_name + "' location '" + binding->_location + "'");
      valid = false;
    }
    else if (binding->_output == out_att->_output) {
      if (found_binding == false) {
        //Copy binding info
        out_att->_outputBinding = binding;
        found_binding = true;
      }
      else {
        passError("Multiple shader bindings found for output description: '" + out_att->_name + "'");
        return;
      }
    }
  }
  if (found_binding == false) {
    passError("Shader binding not found for output description: '" + out_att->_name + "'");
    return;
  }

  if (valid) {
    _outputs.push_back(out_att);
  }
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
uint32_t PassDescription::colorOutputCount() {
  uint32_t n = 0;
  for (auto output : outputs()) {
    if (output->_type == FBOType::Color) {
      n++;
    }
  }
  return n;
}
bool PassDescription::hasDepthBuffer() {
  for (auto output : outputs()) {
    if (output->_type == FBOType::Depth) {
      return true;
    }
  }
  return false;
}

#pragma endregion

#pragma region FramebufferAttachment

FramebufferAttachment::FramebufferAttachment(std::shared_ptr<Vulkan> v, std::shared_ptr<OutputDescription> desc) : VulkanObject(v) {
  _desc = desc;
}
FramebufferAttachment::~FramebufferAttachment() {
}
bool FramebufferAttachment::init(std::shared_ptr<Framebuffer> fbo, std::shared_ptr<RenderFrame> frame) {
  createTarget(fbo, frame, _desc);

  if (_computedLocation == FramebufferAttachment::InvalidLocation) {
    return fbo->pipelineError("Invalid attachment location for '" + _desc->_name + "'.");
    return false;
  }
  if (_target == nullptr) {
    return fbo->pipelineError("Failed to create target.");
  }
  if (_target->mipLevels() > 1 && fbo->sampleCount() != MSAA::Disabled) {
    return fbo->pipelineError("Framebuffer::createAttachments Mipmapping enabled with MSAA - this is not valid in Vulkan. Culprit: '" + _desc->_name + "'");
  }
  if (_target->imageSize().width == 0 || _target->imageSize().height == 0) {
    return fbo->pipelineError("Invalid input image size for framebuffer attachment '" + _desc->_name + "'  '" + VulkanUtils::OutputMRT_toString(_desc->_output) + "'");
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

  return true;
}
std::shared_ptr<TextureImage> RenderFrame::getRenderTarget(OutputMRT target, MSAA samples, VkFormat format, string_t& out_errors, VkImage swapImg, bool createNew) {
  //We MUST reuse images. This will eat up memory quickly.
  //*Swapachain holds RenderTextures - visible to the client.
  //*RenderFrame holds RenderTextures - visible only to the swapchain.
  //*Images are shared among pipelines and FBO's.
  //*Re-created when window resizes and share the lifespan of RenderFrame object.
  std::shared_ptr<TextureImage> tex = nullptr;

  //* To prevent memory overflow we will limit a maximum of 2: Disabled (default), and a single MSAA
  const int MAX_SAMPLE_DUPES = 2;

  auto it_target = _renderTargets.find(target);
  if (it_target == _renderTargets.end()) {
    _renderTargets.insert(std::make_pair(target, std::map<MSAA, std::shared_ptr<TextureImage>>()));
    it_target = _renderTargets.find(target);
  }
  auto& sample_map = it_target->second;
  auto it_img = sample_map.find(samples);
  if (it_img == sample_map.end()) {
    if (createNew) {
      if (sample_map.size() >= MAX_SAMPLE_DUPES) {
        //TODO: delete other MSAA images if another MSAA is enabled.
        StringUtil::appendLine(out_errors, "Too many samples specified for framebuffer.");
        Gu::debugBreak();
        return nullptr;
      }
      tex = createNewRenderTarget(target, samples, format, out_errors, swapImg);
      sample_map.insert(std::make_pair(samples, tex));
      it_img = sample_map.find(samples);
    }
    else {
      return nullptr;
    }
  }
  tex = it_img->second;

  return tex;
}
MSAA Framebuffer::sampleCount() {
  AssertOrThrow2(_passDescription != nullptr);
  return _passDescription->sampleCount();
}
std::shared_ptr<TextureImage> RenderFrame::createNewRenderTarget(OutputMRT target, MSAA samples, VkFormat format, string_t& out_errors, VkImage swapImage) {
  std::shared_ptr<TextureImage> ret = nullptr;
  FBOType type = OutputDescription::outputTypeToFBOType(target);

  string_t name = VulkanUtils::OutputMRT_toString(target) + "_" + std::to_string((int)TextureImage::msaa_to_int(samples)) + "_SAMPLE";

  auto siz = _pSwapchain->windowSize();
  if (target == OutputMRT::RT_DefaultColor) {
    if (format == VK_FORMAT_UNDEFINED) {
      StringUtil::appendLine(out_errors, "Swap chain had undefined image format.");
      return nullptr;
    }
    if (samples == MSAA::Disabled) {
      if (swapImage == VK_NULL_HANDLE) {
        StringUtil::appendLine(out_errors, "Swapchain Image wasn't found, or Swapchain image was null when creating swapchain Rendertarget.");
        return nullptr;
      }
      ret = std::make_shared<TextureImage>(vulkan(), name, TextureType::SwapchainImage, MSAA::Disabled,
                                           siz, format, swapImage, FilterData::no_sampler_no_mipmaps());
    }
    else {
      //Create MSAA image for swapchain
      ret = std::make_shared<TextureImage>(vulkan(), name, TextureType::ColorAttachment, samples,
                                           siz, format, FilterData::no_sampler_no_mipmaps());
    }
  }
  else if (target == OutputMRT::RT_DefaultDepth) {
    // there is only 1 default depth getVkBuffer ever attached so it
    // makes sense to create it internally instead of passing in an RT
    ret = std::make_shared<TextureImage>(vulkan(), name, TextureType::DepthAttachment, samples,
                                         siz, format, FilterData::no_sampler_no_mipmaps());
  }
  return ret;
}
void FramebufferAttachment::createTarget(std::shared_ptr<Framebuffer> fb, std::shared_ptr<RenderFrame> frame, std::shared_ptr<OutputDescription> out_att) {
  AssertOrThrow2(fb->sampleCount() != MSAA::Unset);

  MSAA samples = fb->sampleCount();
  if (out_att->_resolve == true) {
    //If we are a resolve attachment disable sampling.
    samples = MSAA::Disabled;
  }
  string_t out_errors = "";
  if (out_att->_texture != nullptr) {
    _target = out_att->_texture->texture(samples, frame->frameIndex());

    if (_target == nullptr) {
      out_att->_texture->createTexture(fb->sampleCount());
      _target = out_att->_texture->texture(samples, frame->frameIndex());
    }
  }
  else {
    _target = frame->getRenderTarget(out_att->_output, samples, out_att->_outputBinding->_format, out_errors, VK_NULL_HANDLE, true);
  }

  if (_target == nullptr) {
    fb->pipelineError("Failed to get new Render Target: " + out_errors);
    return;
  }

  _computedLocation = computeLocation(fb);
  _computedFinalLayout = computeFinalLayout(fb, out_att);
}
uint32_t FramebufferAttachment::computeLocation(std::shared_ptr<Framebuffer> fb) {
  //Locations must come sequentially in the correct order.
  return fb->nextLocation();
}
VkImageLayout FramebufferAttachment::computeFinalLayout(std::shared_ptr<Framebuffer> fb, std::shared_ptr<OutputDescription> out_att) {
  VkImageLayout ret = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
  bool multisampling = fb->sampleCount() != MSAA::Disabled;

  if (out_att->_texture != nullptr) {
    //RenderTexture
    ret = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }
  else if (out_att->_output == OutputMRT::RT_DefaultColor) {
    //Swapchain Color (or resolve)
    if (multisampling) {
      if (out_att->_resolve == false) {
        ret = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;  // We are a color image, and NOT swap image, or MSAA is enabled and we are a swap image
      }
      else {
        ret = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // We are a RESOLVE attachment
      }
    }
    else {
      ret = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // MSAA is disabled and we are the SWAP IMAGE
    }
  }
  else if (out_att->_output == OutputMRT::RT_DefaultDepth) {
    //Swapchain Depth
    if (out_att->_texture != nullptr) {
      fb->pipelineError("Texture was specified for Default Depth FBO '" + out_att->_name + "' - not supported - clear the texture to use default FBO.");
    }
    else {
      ret = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;  // depth images.
    }
  }
  else {
    fb->pipelineError("Invalid output descriptor '" + out_att->_name + "' ");
  }
  if (ret == VK_IMAGE_LAYOUT_UNDEFINED) {
    fb->pipelineError("Undefined image layout for target '" + out_att->_name + "' ");
  }
  return ret;
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
  string_t out_msg = std::string("[") + name() + "]:" + msg;
  BRLogError(out_msg);
  _bValid = false;
  Gu::debugBreak();
  return false;
}
bool Framebuffer::create(const string_t& name, std::shared_ptr<RenderFrame> frame, std::shared_ptr<PassDescription> desc) {
  _frame = frame;
  _passDescription = desc;
  _name = name;
  //Should be equal. There is one extension that allows variable depth/color samples

  createAttachments();

  if (_attachments.size() == 0) {
    return pipelineError("No framebuffer attachments supplied to Framebuffer::create");
  }

  createRenderPass(frame, getThis<Framebuffer>());

  uint32_t w = _frame->imageSize().width;
  uint32_t h = _frame->imageSize().height;

  std::vector<VkImageView> vk_attachments;
  for (auto att : _attachments) {
    if (att->target() == nullptr) {
      return pipelineError("Framebuffer::create Target '" + att->desc()->_name + "' texture was null.");
    }
    if (att->target()->imageView() == VK_NULL_HANDLE) {
      return pipelineError("Framebuffer::create Target '" + att->desc()->_name + "' imageView was null.");
    }
    vk_attachments.push_back(att->target()->imageView());
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

  return true;
}
bool Framebuffer::createAttachments() {
  _attachments.clear();

  //The FBO attachments and RenderTExture attachments merge to create the FBO in this function.
  //Place any information that applies to all FBO attachments in here.
  for (auto out_att : _passDescription->outputs()) {
    auto attachment = std::make_shared<FramebufferAttachment>(vulkan(), out_att);
    if (!attachment->init(getThis<Framebuffer>(), _frame)) {
      return pipelineError("Failed to initialize fbo attachment '" + out_att->_name + "'");
    }
    _attachments.push_back(attachment);
  }

  //Create resolve attachments.
  if (this->sampleCount() != MSAA::Disabled) {
    for (auto output : _passDescription->outputs()) {
      //The vulkan spec says that the number of resolve attachments must equal the number of oclor attachments in VkSubpassDescription
      if (output->_type == FBOType::Color) {
        auto resolve = std::make_shared<OutputDescription>();
        resolve->_name = Stz output->_name + "_resolve";
        resolve->_texture = output->_texture;      //this will be nullptr for swapchain image.
        resolve->_blending = BlendFunc::Disabled;  // ?
        resolve->_type = output->_type;
        resolve->_clearColor = output->_clearColor;
        resolve->_clearDepth = output->_clearDepth;
        resolve->_clearStencil = output->_clearStencil;
        resolve->_output = output->_output;
        resolve->_clear = output->_clear;
        resolve->_compareOp = output->_compareOp;
        resolve->_outputBinding = output->_outputBinding;
        resolve->_resolve = true;
        _resolveDescriptions.push_back(resolve);

        auto attachment = std::make_shared<FramebufferAttachment>(vulkan(), resolve);
        if (!attachment->init(getThis<Framebuffer>(), _frame)) {
          return pipelineError("Failed to initialize FBO resolve attachment '" + resolve->_name + "'");
        }
        _attachments.push_back(attachment);
      }
    }
  }

  return true;
}
bool Framebuffer::createRenderPass(std::shared_ptr<RenderFrame> frame, std::shared_ptr<Framebuffer> fbo) {
  //using subpassLoad you can read previous subpass values. This is more efficient than the old approach.
  //https://www.saschawillems.de/blog/2018/07/19/vulkan-input-attachments-and-sub-passes/
  //https://github.com/KhronosGroup/GLSL/blob/master/extensions/khr/GL_KHR_vulkan_glsl.txt
  std::vector<VkAttachmentDescription> attachments;
  std::vector<VkAttachmentReference> colorAttachmentRefs;
  std::vector<VkAttachmentReference> resolveAttachmentRefs;
  std::vector<VkAttachmentReference> depthAttachmentRefs;

  for (size_t iatt = 0; iatt < _attachments.size(); ++iatt) {
    auto att = _attachments[iatt];

    AssertOrThrow2(att != nullptr);
    AssertOrThrow2(att->desc() != nullptr);
    //Load or Clear the image
    VkAttachmentLoadOp loadOp;
    if (att->desc()->_clear == false) {
      loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    }
    else {
      loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    }

    //Get MSAA sample count
    MSAA output_sampleCount = this->sampleCount();
    VkSampleCountFlagBits sample_flags = TextureImage::multisampleToVkSampleCountFlagBits(output_sampleCount);

    if (att->desc()->_type == FBOType::Color) {
      if (att->desc()->_resolve) {
        sample_flags = VK_SAMPLE_COUNT_1_BIT;
      }
      attachments.push_back({
        .flags = 0,
        .format = att->target()->format(),
        .samples = sample_flags,
        .loadOp = loadOp,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,  // VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = att->finalLayout(),
      });
      if (att->desc()->_resolve) {
        resolveAttachmentRefs.push_back({
          .attachment = att->location(),
          .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        });
      }
      else {
        colorAttachmentRefs.push_back({
          .attachment = att->location(),
          .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        });
      }
    }
    else if (att->desc()->_type == FBOType::Depth) {
      if (depthAttachmentRefs.size() > 0) {
        return fbo->pipelineError("Multiple Renderbuffer depth buffers found in shader Output FBOs");
      }
      attachments.push_back({
        .flags = 0,
        .format = att->target()->format(),
        .samples = sample_flags,
        .loadOp = loadOp,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = att->finalLayout(),  //** This needs to be changed for Deferred MRTs
      });
      depthAttachmentRefs.push_back({
        //Layout location of depth getVkBuffer is +1 the last location.
        .attachment = att->location(),
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      });
    }
    else {
      BRThrowNotImplementedException();
    }
  }

  if (sampleCount() != MSAA::Disabled) {
    if (resolveAttachmentRefs.size() != colorAttachmentRefs.size()) {
      return pipelineError("MSAA is enabled, but Resolve attachment count didn't equal color attachment count.");
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
    .pResolveAttachments = resolveAttachmentRefs.data(),
    .pDepthStencilAttachment = depthAttachmentRefs.data(),
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
const BR2::usize2& Framebuffer::imageSize() {
  AssertOrThrow2(_frame != nullptr);
  return _frame->imageSize();
}
bool Framebuffer::validate() {
  //Validate outputs
  if (_attachments.size() == 0) {
    return pipelineError("No outputs specified before pass for OutputDescription.");
  }

  //Validate has color
  std::vector<std::shared_ptr<OutputDescription>> depthOutputs;
  bool hasColor = false;
  for (size_t i = 0; i < _attachments.size(); ++i) {
    auto output = _attachments[i]->desc();

    if (output->_type == FBOType::Depth) {
      depthOutputs.push_back(output);
    }
    else if (output->_type == FBOType::Color) {
      hasColor = true;
    }
    else {
      BRThrowException("Unsupported FBO output type '" + std::to_string((int)output->_type) + "'");
    }
  }

  //Validate has target
  for (auto output : _attachments) {
    if (output->target() == nullptr) {
      return pipelineError("Output " + output->desc()->_name + " texture was not set.");
    }
  }

  //Validate color sample counts
  MSAA sc_first_color;
  bool sc_set = false;
  for (size_t i = 0; i < _attachments.size(); ++i) {
    auto output = _attachments[i];
    MSAA sc = output->target()->sampleCount();

    if (output->desc()->_type == FBOType::Color) {
      if (sc_set == false) {
        sc_first_color = sc;
      }
      else if (sc_first_color != sc) {
        int sc1 = VulkanUtils::SampleCount_ToInt(sc_first_color);
        int sc2 = VulkanUtils::SampleCount_ToInt(sc);
        return pipelineError(Stz "Sample count '" + std::to_string(sc2) + "' for " + output->desc()->_name + " does not match other color sample counts '" + std::to_string(sc1) + "' .");
      }
    }
  }

  //Validate depth sample count
  //TODO:
  //TODO:
  //TODO:
  bool allowOverSampleDepthBuffer = false;  //vulkan()->extensionEnabled(VK_AMD_MIXED_ATTACHMENT_SAMPLES_EXTENSION_NAME);
  for (size_t i = 0; i < _attachments.size(); ++i) {
    auto output = _attachments[i];

    if (output->desc()->_type == FBOType::Depth) {
      MSAA sc = output->target()->sampleCount();
      int sc_color_ct = VulkanUtils::SampleCount_ToInt(sc_first_color);
      int sc_depth_ct = VulkanUtils::SampleCount_ToInt(sc);
      if (allowOverSampleDepthBuffer) {
        if (sc_depth_ct < sc_color_ct) {
          return pipelineError("Depth buffer sample count less than color buffer sample count with VK_AMD_MIXED_ATTACHMENT_SAMPLES_EXTENSION_NAME enabled.");
        }
      }
      else if (sc != sc_first_color) {
        return pipelineError("Depth buffer sample count '" + std::to_string(sc_depth_ct) + "' does not equal color buffer sample count '" + std::to_string(sc_color_ct) + "'.");
      }
    }
  }

  //Validate depth output count
  if (depthOutputs.size() > 1) {
    std::string depth_st = "";
    for (auto out : depthOutputs) {
      depth_st += Stz "'" + out->_name + "',";
    }
    return pipelineError("More than 1 depth output specified:" + depth_st);
  }

  //Validate equal image sizes
  int32_t def_w = -1;
  int32_t def_h = -1;
  for (size_t i = 0; i < _attachments.size(); ++i) {
    auto size = _attachments[i]->imageSize();
    if (def_w == -1) {
      def_w = size.width;
    }
    if (def_h == -1) {
      def_h = size.height;
    }
    if (size.width != def_w) {
      return pipelineError("Output FBO " + std::to_string(i) + " width '" +
                           std::to_string(size.width) +
                           "' did not match first FBO width '" + std::to_string(def_w) + "'.");
    }
    if (size.height != def_h) {
      return pipelineError("Output FBO " + std::to_string(i) + " height '" +
                           std::to_string(size.height) +
                           "' did not match first FBO height '" + std::to_string(def_h) + "'.");
    }
  }
  if (def_w == -1 || def_h == -1) {
    return pipelineError("Invalid FBO image size.");
  }

  return true;
}
uint32_t Framebuffer::maxLocation() {
  uint32_t maxloc = 0;
  auto locations = passDescription()->shader()->locations();
  auto it = std::max_element(locations.begin(), locations.end());
  if (it == locations.end()) {
    pipelineError("Failed to get location from output - setting to zero");
    return FramebufferAttachment::InvalidLocation;
  }
  else {
    maxloc = *it;
  }
  if (passDescription()->hasDepthBuffer()) {
    maxloc += 1;
  }
  if (sampleCount() != MSAA::Disabled) {
    //Resolve attachments.
    maxloc += passDescription()->colorOutputCount();
  }
  return maxloc;
}
uint32_t Framebuffer::nextLocation() {
  //Sequentially get the attachment locations in the FBO.
  uint32_t ret = _currentLocation;

  if (ret > maxLocation()) {
    return pipelineError(Stz "FBO attachment - location '" + std::to_string(ret) + "' exceeded expected shader maximum '" + std::to_string(maxLocation()) + "'.");
    return FramebufferAttachment::InvalidLocation;
  }

  _currentLocation++;
  return ret;
}

#pragma endregion

#pragma region Pipeline

Pipeline::Pipeline(std::shared_ptr<Vulkan> v,
                   VkPrimitiveTopology topo,
                   VkPolygonMode mode, VkCullModeFlags cullmode) : VulkanObject(v) {
  _primitiveTopology = topo;
  _polygonMode = mode;
  _cullMode = cullmode;
}
VkPipelineColorBlendAttachmentState Pipeline::getVkPipelineColorBlendAttachmentState(BlendFunc bf, std::shared_ptr<Framebuffer> fb) {
  VkPipelineColorBlendAttachmentState cba{};

  if (bf == BlendFunc::Disabled) {
    cba.blendEnable = VK_FALSE;
  }
  else if (bf == BlendFunc::AlphaBlend) {
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
    fb->pipelineError("Unhandled _blending state '" + std::to_string((int)bf) + "' set on ShaderOutput.");
  }
  return cba;
}
bool Pipeline::init(std::shared_ptr<PipelineShader> shader,
                    std::shared_ptr<BR2::VertexFormat> vtxFormat,
                    std::shared_ptr<Framebuffer> pfbo) {
  _fbo = pfbo;

  if (pfbo->passDescription() == nullptr) {
    return pfbo->pipelineError("Pass description was null in Pipeline::init");
  }
  if (pfbo->passDescription()->outputs().size() == 0) {
    return pfbo->pipelineError("No pass description outputs specified in Pipeline::init");
  }

  //Pipeline Layout
  auto descriptorLayout = shader->getVkDescriptorSetLayout();
  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    //These are the getVkBuffer descriptors
    .setLayoutCount = 1,
    .pSetLayouts = &descriptorLayout,
    //Constants to pass to shaders.
    .pushConstantRangeCount = 0,
    .pPushConstantRanges = nullptr
  };
  CheckVKR(vkCreatePipelineLayout, vulkan()->device(), &pipelineLayoutInfo, nullptr, &_pipelineLayout);

  //Blending
  bool independentBlend = (vulkan()->deviceFeatures().independentBlend == VK_TRUE);
  if (!independentBlend && pfbo->passDescription()->blendMode() == FramebufferBlendMode::Independent) {
    return pfbo->pipelineError("In Pipeline: Independent blend mode not supported. Use 'Global'");
  }
  std::vector<VkPipelineColorBlendAttachmentState> attachmentBlending;
  if (pfbo->passDescription()->blendMode() == FramebufferBlendMode::Independent) {
    for (auto& att : pfbo->passDescription()->outputs()) {
      //https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkGraphicsPipelineCreateInfo.html
      //attachmentCount member of pColorBlendState must be equal to the colorAttachmentCount used to create subpass
      //If the independent blending feature is not enabled on the device, all VkPipelineColorBlendAttachmentState elements in the pAttachments array must be identical.
      if (att->_type == FBOType::Color) {
        auto cba = getVkPipelineColorBlendAttachmentState(att->_blending, pfbo);
        attachmentBlending.push_back(cba);
      }
    }
  }
  else if (pfbo->passDescription()->blendMode() == FramebufferBlendMode::Global) {
    auto cba = getVkPipelineColorBlendAttachmentState(pfbo->passDescription()->globalBlend(), pfbo);
    for (auto& att : pfbo->passDescription()->outputs()) {
      if (att->_type == FBOType::Color) {
        attachmentBlending.push_back(cba);
      }
    }
  }
  else {
    return pfbo->pipelineError("Unhandled FramebufferBlendMode '" + std::to_string((int)pfbo->passDescription()->blendMode()) + "'");
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

  //Viewport (disabled, dynamic)
  VkPipelineViewportStateCreateInfo viewportState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .viewportCount = 0,
    .pViewports = nullptr,  //Viewport is dynamic
    .scissorCount = 0,
    .pScissors = nullptr,
  };

  //Depth getVkBuffer
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
    .cullMode = _cullMode,
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

  VkSampleCountFlagBits samples = TextureImage::multisampleToVkSampleCountFlagBits(pfbo->sampleCount());
  VkBool32 sampleShadingEnabled = VK_FALSE;
  float minSampleShading = 1;
  if (vulkan()->deviceFeatures().sampleRateShading) {
    if (shader->sampleShadingVariables()) {
      //https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#primsrast-sampleshading
      //Per spec - this is FORCED ENABLED if there is a SampleId or SamplePosition in the frag shader
      sampleShadingEnabled = true;
      minSampleShading = 1;  // This is not used if there are sample shading variables
    }
    else {
      //It would be silly to create a new pipeline every time we change sampleshading float.
      //Also - it would be equally silly to do so if one already exists. Probably better to recreate pipeline when the data changes.
      //**TODO: check MSAA is >= std::max(minSampleShading x rasterizationSamples, 1) samples for all images.
      sampleShadingEnabled = false;
      minSampleShading = 1;
    }
  }

  VkPipelineMultisampleStateCreateInfo multisampling = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .rasterizationSamples = samples,
    .sampleShadingEnable = sampleShadingEnabled,
    .minSampleShading = minSampleShading,
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

std::shared_ptr<ShaderModule> PipelineShader::getModule(ShaderStage stage, bool throwIfNotFound) {
  std::shared_ptr<ShaderModule> mod = nullptr;
  for (auto module : _modules) {
    auto ss = VulkanUtils::spvReflectShaderStageFlagBits_To_ShaderStage(module->reflectionData()->shader_stage);
    if (ss == stage) {
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
  std::shared_ptr<ShaderModule> vert = getModule(ShaderStage::VertexStage);
  auto maxinputs = vulkan()->deviceProperties().limits.maxVertexInputAttributes;
  if (vert->reflectionData()->input_variable_count >= maxinputs) {
    return shaderError("Error creating shader '" + name() + "' - too many input variables");
  }

  std::shared_ptr<ShaderModule> frag = getModule(ShaderStage::FragmentStage);
  auto maxAttachments = vulkan()->deviceProperties().limits.maxFragmentOutputAttachments;
  if (frag->reflectionData()->output_variable_count >= maxAttachments) {
    return shaderError("Error creating shader '" + name() + "' - too many output attachments in fragment shader.");
  }

  //TODO: geom
  std::shared_ptr<ShaderModule> geom = getModule(ShaderStage::GeometryStage);
  return true;
}
bool PipelineShader::createInputs() {
  std::shared_ptr<ShaderModule> mod = getModule(ShaderStage::VertexStage, true);

  auto maxinputs = vulkan()->deviceProperties().limits.maxVertexInputAttributes;
  auto maxbindings = vulkan()->deviceProperties().limits.maxVertexInputBindings;

  if (mod->reflectionData()->input_variable_count >= maxinputs) {
    return shaderError("Error creating shader '" + name() + "' - too many input variables");
  }

  //int iBindingIndex = 0;
  uint32_t highestLocation = 0;
  //Note: spvReflect bindings are NOT required to be in order.
  for (uint32_t ii = 0; ii < mod->reflectionData()->input_variable_count; ++ii) {
    auto& iv = mod->reflectionData()->input_variables[ii];
    std::shared_ptr<VertexAttribute> attrib = std::make_shared<VertexAttribute>();
    if (iv.location == 0xFFFFFFFF) {
      //0xFFFFFFFF is reserved for gl_InstanceIndex
      _bInstanced = true;
    }
    else {
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
      attrib->_desc.location = iv.location;
      attrib->_desc.format = spvReflectFormatToVulkanFormat(iv.format);
      attrib->_desc.offset = 0;  //This is computed later

      if (attrib->_desc.location > highestLocation) {
        highestLocation = attrib->_desc.location;
      }

      //iBindingIndex++;
      _attributes.push_back(attrib);
    }
  }

  //Sort attribs
  std::sort(_attributes.begin(), _attributes.end(), [](std::shared_ptr<VertexAttribute>& a, std::shared_ptr<VertexAttribute>& b) {
    return a->_desc.location < b->_desc.location;
  });

  //Compute Byte offsets.
  size_t iOffset = 0;
  for (auto iat : _attributes) {
    iat->_desc.offset = iOffset;
    iOffset += iat->_totalSizeBytes;
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
  std::shared_ptr<ShaderModule> mod = getModule(ShaderStage::FragmentStage, true);
  if (mod == nullptr) {
    return shaderError("Fragment module not found for shader '" + this->name() + "'");
  }

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
        return shaderError("Unhandled shader output variable format '" + VulkanUtils::VkFormat_toString(fb->_format) + "'");
      }
      _locations.push_back(fb->_location);

      _outputBindings.push_back(fb);
    }
    else {
      return shaderError("Shader - output variable was not an fbo prefixed with _outFBO - this is not supported.");
    }
  }
  //Validate attachment Locations found
  for (size_t iloc = 0; iloc < _locations.size(); ++iloc) {
    auto it = std::find(_locations.begin(), _locations.end(), (uint32_t)iloc);
    if (it == _locations.end()) {
      return shaderError("Error one or more FBO locations missing '" + std::to_string(iloc) + "' - all locations to the maximum location must be filled.");
    }
  }
  //Validate sequential locations
  if (_locations.size() > 0) {
    uint32_t lastLoc = _locations[0];
    for (size_t iloc = 1; iloc < _locations.size(); ++iloc) {
      if (iloc != lastLoc + 1) {
        return shaderError("Error shader output location is out of order '" + std::to_string(iloc) + "' -  must be sequential.");
      }
    }
  }
  uint32_t maxLoc;
  auto it = std::max_element(_locations.begin(), _locations.end());
  if (it == _locations.end()) {
    shaderError("Failed to get renderbuffer location from output.");
    maxLoc = 0;
  }
  else {
    maxLoc = *it + 1;
  }

  //Add Renderbuffer by default, ignore if not used.
  //Note: we need this for the _format variable.
  auto fb = std::make_shared<ShaderOutputBinding>();
  fb->_name = "_auto_depthBuffer";
  fb->_location = maxLoc++;
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
  else if (StringUtil::equals(name, "_uboLights")) {
    ret = DescriptorFunction::LightsUBO;
  }
  return ret;
}
bool PipelineShader::createDescriptors() {
  //Create uniform blocks
  //Create samplers
  uint32_t _nPool_Samplers = 0;
  uint32_t _nPool_UBOs = 0;

  //Parse shader metadata
  std::vector<std::shared_ptr<Descriptor>> bindingLocations;
  for (auto& module : _modules) {
    for (uint32_t idb = 0; idb < module->reflectionData()->descriptor_binding_count; idb++) {
      auto& descriptor = module->reflectionData()->descriptor_bindings[idb];

      std::shared_ptr<Descriptor> d = std::make_shared<Descriptor>();

      d->_name = std::string(descriptor.name);
      if (d->_name.length() == 0) {
        BRLogWarn("Name of one or more input shader variables was not specified for shader module '" + module->name() + "'");
      }

      d->_arraySize = 1;
      d->_function = classifyDescriptor(d->_name);

      if (module->reflectionData()->shader_stage == SPV_REFLECT_SHADER_STAGE_VERTEX_BIT) {
        d->_stage = ShaderStage::VertexStage;
      }
      else if (module->reflectionData()->shader_stage == SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT) {
        d->_stage = ShaderStage::FragmentStage;
      }
      else if (module->reflectionData()->shader_stage == SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT) {
        d->_stage = ShaderStage::GeometryStage;
      }
      else if (module->reflectionData()->shader_stage == SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT) {
        d->_stage = ShaderStage::ComputeStage;
      }
      else if (module->reflectionData()->shader_stage == SPV_REFLECT_SHADER_STAGE_TESSELLATION_CONTROL_BIT) {
        d->_stage = ShaderStage::TessControlStage;
      }
      else if (module->reflectionData()->shader_stage == SPV_REFLECT_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) {
        d->_stage = ShaderStage::TessEvalStage;
      }
      else {
        return shaderError("Invalid or unsupported shader stage (SpvReflectShaderStage):  " + std::to_string(module->reflectionData()->shader_stage));
      }

      if (descriptor.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
        d->_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        _nPool_UBOs++;

        d->_blockSizeBytes = descriptor.block.size;

        if (descriptor.array.dims_count > 0) {
          //In order to support this we need to create arrays of descriptors.
          return shaderError("UBO '" + d->_name + "' was a Block array - Arrays of UBO blocks not supported.");
          //  uint32_t arraySize = descriptor.array.dims[0];
          //  for (auto iDim = 1; iDim < descriptor.array.dims_count; ++iDim) {
          //    d->_arraySize *= descriptor.array.dims[iDim];
          //  }
          //  d->_arraySize = arraySize;
        }
        else {
          d->_arraySize = 1;
        }
        d->_bufferSizeBytes = d->_blockSizeBytes * d->_arraySize;
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

      d->_binding = descriptor.binding;

      //Reusing names is alright, but may cause problems with built-in variables. This is disabled by default.

      for (auto other : bindingLocations) {
        //**Reusing descriptors will cause severe vulkan errors. It will hault the GPU.
        if (d->_binding == other->_binding) {
          return shaderError("Duplicate binding specified for descriptor '" + d->_name + "' in stage '" + VulkanUtils::ShaderStage_toString(d->_stage) + "', and '" +
                             other->_name + "' in stage '" + VulkanUtils::ShaderStage_toString(other->_stage));
        }
        if (StringUtil::equals(d->_name, other->_name)) {
          string_t st_dupe = "Duplicate named shader variables encountered: '" + d->_name + "' in stage '" + VulkanUtils::ShaderStage_toString(d->_stage) + "', and '" +
                             other->_name + "' in stage '" + VulkanUtils::ShaderStage_toString(other->_stage) +
                             ". It is recommended that variables have different names, or, multi-stage uniform data is passed as varying variables. " +
                             "";
          // if (vulkan()->allowDuplicateUniforms()) {
          // shaderWarning(st_dupe);
          // }
          // else {
          shaderError(st_dupe);
          //}
        }
      }
      bindingLocations.push_back(d);

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
    .flags = 0,  //VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT - this may cause fragmentation
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
      .stageFlags = static_cast<VkShaderStageFlags>(VulkanUtils::ShaderStage_to_VkShaderStageFlagBits(desc->_stage)),
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
bool PipelineShader::createUBO(const string_t& name, const string_t& var_name, size_t itemSize, size_t itemCount) {
  //Create a UBO. Name must match uniform name in shader.
  // We allow the creation of dynamically sized UBO's (separate from the GPU spec) to allow to use smaller memory sizes.
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
      return shaderError("UBO for shader variable '" + var_name + "' with client variable '" + name + "' was already created.");
    }
    else {
      if (itemSize * itemCount > desc->_bufferSizeBytes) {
        return shaderError("Ubo size was greater than the supplied size.");
      }

      std::shared_ptr<ShaderDataUBO> datUBO = std::make_shared<ShaderDataUBO>();
      datUBO->_buffer = std::make_shared<VulkanBuffer>(
        vulkan(),
        VulkanBufferType::UniformBuffer,
        false,  //not on GPU
        itemSize, itemCount, nullptr, 0);
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
    .buffer = buffer->buffer()->getVkBuffer(),
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
bool PipelineShader::bindSampler(const string_t& name, std::shared_ptr<TextureImage> texture, uint32_t arrayIndex) {
  if (!beginPassGood()) {
    return false;
  }
  if (texture == nullptr) {
    return renderError("Descriptor '" + name + "'could not be found for shader .");
  }
  if (texture->sampleCount() != MSAA::Disabled) {
    return renderError("Tried to bind a texture with an MSAA format.");
  }
  if (texture->filter()._samplerType == SamplerType::None) {
    return renderError("Tried to bind texture '" + texture->name() + "' that did not have a sampler to sampler location '" + name + "'.");
  }

  std::shared_ptr<Descriptor> desc = getDescriptor(name);
  if (desc == nullptr) {
      return renderError("Descriptor '" + name + "'could not be found for shader.");
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
string_t PipelineShader::createUniqueFBOName(std::shared_ptr<RenderFrame> frame, std::shared_ptr<ShaderData> data, std::shared_ptr<PassDescription> passdesc) {
  string_t ret = std::string("(") + name() + ").(fbo" + std::to_string((int)data->_framebuffers.size()) + ")";

  for (auto io = 0; io < passdesc->outputs().size(); ++io) {
    auto desc = passdesc->outputs()[io];
    ret += ".(";
    ret += VulkanUtils::OutputMRT_toString(desc->_output);
    ret += desc->_clear ? ".clear" : ".retain";
    // ret += desc->isSwapchainColorImage() ? ".swap_color" : (desc->isSwapchainDepthImage() ? ".swap_depth" : ".rendertexture");
    ret += ")";
  }
  return ret;
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
    else {
      string_t fboName = createUniqueFBOName(frame, data, desc);
      if (!fbo->create(fboName, frame, desc)) {
        fbo->pipelineError("Failed to create FBO.");
      }
    }
  }

  return fbo;
}
std::shared_ptr<Framebuffer> PipelineShader::findFramebuffer(std::shared_ptr<ShaderData> data, std::shared_ptr<PassDescription> outputs) {
  //Returns an exact match on the given input PassDescription and the FBO PassDescription.
  std::shared_ptr<Framebuffer> fbo = nullptr;
  for (auto fb : data->_framebuffers) {
    if (fb->passDescription()->sampleCount() == outputs->sampleCount()) {
      if (fb->passDescription()->outputs().size() == outputs->outputs().size()) {
        bool match = true;
        for (auto io = 0; io < fb->passDescription()->outputs().size(); ++io) {
          auto fboDescOut = fb->passDescription()->outputs()[io];
          auto inDescOut = outputs->outputs()[io];

          //TODO: Find a less error-prone way to match FBOs with pipeline state.
          if (
            fboDescOut->_output != inDescOut->_output || fboDescOut->_clear != inDescOut->_clear  //This sucks, but this is because the LOAD_OP is hard set in the render pass.
            || fboDescOut->_type != inDescOut->_type) {
            match = false;
          }
        }
        if (match == true) {
          fbo = fb;
          break;
        }
      }
    }
  }
  return fbo;
}
bool PipelineShader::beginRenderPass(std::shared_ptr<CommandBuffer> buf, std::shared_ptr<PassDescription> input_desc, BR2::urect2* extent) {
  if (!valid()) {
    return false;
  }
  if (!input_desc->valid()) {
    return false;
  }

  auto frame = input_desc->frame();
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
std::shared_ptr<Pipeline> PipelineShader::getPipeline(std::shared_ptr<BR2::VertexFormat> vertexFormat, VkPrimitiveTopology topo, VkPolygonMode polymode, VkCullModeFlags cullMode) {
  if (_pBoundData == nullptr) {
    BRLogError("Pipeline: ShaderData was not set.");
    return nullptr;
  }
  std::shared_ptr<Pipeline> pipe = nullptr;
  for (auto the_pipe : _pBoundData->_pipelines) {
    if (the_pipe->primitiveTopology() == topo &&
        the_pipe->polygonMode() == polymode &&
        the_pipe->vertexFormat() == vertexFormat &&
        the_pipe->cullMode() == cullMode &&
        _pBoundFBO == the_pipe->fbo()) {
      pipe = the_pipe;
      break;
    }
  }
  if (pipe == nullptr) {
    std::shared_ptr<BR2::VertexFormat> format = nullptr;  // ** TODO create multiple pipelines for Vertex Format, Polygonmode & Topo.
    pipe = std::make_shared<Pipeline>(vulkan(), topo, polymode, cullMode);
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
  string_t out_msg = "[" + name() + "]:" + msg;
  BRLogError(out_msg);
  _bValid = false;
  Gu::debugBreak();
  return false;
}
bool PipelineShader::renderError(const string_t& msg) {
  string_t out_msg = "[" + name() + "]:" + msg;
  BRLogError(out_msg);
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
bool PipelineShader::bindPipeline(std::shared_ptr<CommandBuffer> cmd, std::shared_ptr<BR2::VertexFormat> v_fmt, VkPolygonMode mode, VkPrimitiveTopology topo, VkCullModeFlags cull) {
  auto pipe = getPipeline(v_fmt, topo, mode, cull);
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
  cmd->bindMesh(m);
  cmd->drawIndexed(numInstances);
}
void PipelineShader::bindViewport(std::shared_ptr<CommandBuffer> cmd, const BR2::urect2& size) {
  cmd->cmdSetViewport(size);
}
void PipelineShader::endRenderPass(std::shared_ptr<CommandBuffer> buf) {
  AssertOrThrow2(_pBoundFBO != nullptr);
  AssertOrThrow2(_pBoundFrame != nullptr);

  //Update RenderTexture Mipmaps (if enabled)
  //**Note: I didn't find anything that allowed FBOs to automatically generate mipmaps.
  //That said, this may not be the best way to recreate mipmaps.
  for (auto att : _pBoundFBO->attachments()) {
    if (att->desc()->_texture != nullptr) {
      auto tex = att->desc()->_texture->texture(_pBoundFBO->sampleCount(), _pBoundFrame->frameIndex());
      if (tex) {
        tex->generateMipmaps(buf);
      }
      else {
        BRLogErrorCycle("Output Texture '" + att->desc()->_name + "'for mip (enum) level '" + std::to_string((int)_pBoundFBO->sampleCount()) + "'was not found.");
      }
    }
  }

  //Clear everything, done.
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
std::shared_ptr<PassDescription> PipelineShader::getPass(std::shared_ptr<RenderFrame> frame, MSAA sampleCount, BlendFunc globalBlend, FramebufferBlendMode rbm) {
  //@param MSAA - You can't have mixed sample counts except for using the AMD extension to allow varied depth getVkBuffer sample counts.
  //@param globalBlend - If FramebufferBlendMode is default and independent blending is disabled,
  //We set the sample count across all objects ane recreate FBO's as needed.
  std::shared_ptr<PassDescription> d = std::make_shared<PassDescription>(frame, getThis<PipelineShader>(), sampleCount, globalBlend, rbm);
  return d;
}
bool PipelineShader::sampleShadingVariables() {
  //TODO: return true if sample rate shading variables are supplied
  //There are not any reasons why an input variable would be decorated this way that I can see right now so we will skip this
  return false;
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

RenderFrame::RenderFrame(std::shared_ptr<Vulkan> v) : VulkanObject(v) {
}
RenderFrame::~RenderFrame() {
  vkDestroySemaphore(vulkan()->device(), _imageAvailableSemaphore, nullptr);
  vkDestroySemaphore(vulkan()->device(), _renderFinishedSemaphore, nullptr);
  vkDestroyFence(vulkan()->device(), _inFlightFence, nullptr);
}
void RenderFrame::addRenderTarget(OutputMRT output, MSAA samples, std::shared_ptr<TextureImage> tex) {
}

void RenderFrame::init(std::shared_ptr<Swapchain> ps, uint32_t frameIndex, VkImage swapImg, VkSurfaceFormatKHR fmt) {
  _pSwapchain = ps;
  _frameIndex = frameIndex;

  createSyncObjects();
  string_t errors;
  if (getRenderTarget(OutputMRT::RT_DefaultColor, MSAA::Disabled, fmt.format, errors, swapImg, true) == nullptr) {
    BRThrowException("Failed to create swapchain render target: " + errors)
  }

  _pCommandBuffer = std::make_shared<CommandBuffer>(vulkan(), getThis<RenderFrame>());
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
  VkResult res = VK_SUCCESS;
  uint64_t wait_fences = UINT64_MAX;
  if (!vulkan()->waitFences()) {
    wait_fences = 0;  //Don't wait if no image available.
  }
  //Waits for all command buffers to complete execution
  res = vkWaitForFences(vulkan()->device(), 1, &_inFlightFence, VK_TRUE, wait_fences);
  if (res != VK_SUCCESS) {
    if (res == VK_NOT_READY) {
      return false;
    }
    else if (res == VK_TIMEOUT) {
      return false;
    }
    else if (res == VK_ERROR_DEVICE_LOST) {
      throw std::runtime_error(Vulkan::c_strErrDeviceLost);
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
      throw std::runtime_error(Vulkan::c_strErrDeviceLost);
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

  CheckVKR(vkResetFences, vulkan()->device(), 1, &_inFlightFence);

  AssertOrThrow2(_pCommandBuffer->state() != CommandBufferState::Submit);
  _pCommandBuffer->submit({
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT  //Wait at the color attachment output
                          },
                          {
                            _imageAvailableSemaphore  //Wait for image available.
                          },
                          { _renderFinishedSemaphore }, _inFlightFence);

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

  //Uncomment this to test sync issues.
  //vulkan()->waitIdle();

  _frameState = FrameState::FrameEnd;
}
const BR2::usize2& RenderFrame::imageSize() {
  return _pSwapchain->windowSize();
}

#pragma endregion

#pragma region Swapchain
Swapchain::Swapchain(std::shared_ptr<Vulkan> v) : VulkanObject(v) {
}
Swapchain::~Swapchain() {
  _renderTextures.clear();
  cleanupSwapChain();
  _shaders.clear();
}
void Swapchain::initSwapchain(const BR2::usize2& window_size) {
  //This is the main method to create AND re-create swapchain.
  vulkan()->waitIdle();

  cleanupSwapChain();

  vulkan()->waitIdle();

  createSwapChain(window_size);

  //Redo RenderTextures (do this before any FBO stuff)
  for (auto r : _renderTextures) {
    r->recreateAllTextures();
  }

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

  _imageSize.width = window_size.width;
  _imageSize.height = window_size.height;

  //This is cool. Directly query the color space
  VkSurfaceFormatKHR surfaceFormat;
  if (!findValidSurfaceFormat(std::vector<VkFormat>{ VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM }, surfaceFormat)) {
    vulkan()->errorExit("Could not find valid window surface format.");
  }

  _surfaceFormat = surfaceFormat;

  VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
  if (!findValidPresentMode(presentMode)) {
    vulkan()->errorExit("Could not find valid present mode.");
  }

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

  //ImageCount must be what we asked for as we use this data to initialize (pretty much everything getVkBuffer related).
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
    std::shared_ptr<RenderFrame> f = std::make_shared<RenderFrame>(vulkan());
    f->init(getThis<Swapchain>(), (uint32_t)idx, image, surfaceFormat);
    _frames.push_back(f);
  }
  _imagesInFlight = std::vector<VkFence>(frames().size());
}
void Swapchain::cleanupSwapChain() {
  _frames.clear();
  _imagesInFlight.clear();

  for (auto r : _renderTextures) {
    r->_texture = nullptr;
  }

  if (_swapChain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(vulkan()->device(), _swapChain, nullptr);
  }
}
void Swapchain::registerShader(std::shared_ptr<PipelineShader> shader) {
  //Register to prevent the smart pointer from deallcoating.
  if (_shaders.find(shader) == _shaders.end()) {
    _shaders.insert(shader);
  }
  for (auto frame : frames()) {
    shader->clearShaderDataCache(frame);
  }
}
bool Swapchain::beginFrame(const BR2::usize2& windowsize) {
  //Returns true if we acquired an image to draw to, false if none are ready.
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

  if (_copyImage_Flag) {
    string_t err;
    auto target = this->frames()[0]->getRenderTarget(OutputMRT::RT_DefaultColor, MSAA::Disabled, VK_FORMAT_B8G8R8A8_SRGB, err, VK_NULL_HANDLE, false);
    auto img = target->copyImageFromGPU();
    img->save("out_Final_Result.png");

    for (int i = 0; i < _renderTextures.size(); ++i) {
      auto tex = _renderTextures[i]->texture(MSAA::Disabled, _currentFrame);
      if (tex) {
        auto img = tex->copyImageFromGPU();
        img->save(std::string("out_RenderTexture" + std::to_string(i) + ".png").c_str());
      }
      else {
        Gu::debugBreak();
      }
    }

    _copyImage_Flag = false;
  }

  _currentFrame = (_currentFrame + 1) % _frames.size();
  _frameState = FrameState::FrameEnd;
}
void Swapchain::waitImage(uint32_t imageIndex, VkFence myFence) {
  if (_imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
    CheckVKR(vkWaitForFences, vulkan()->device(), 1, &_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
  }
  _imagesInFlight[imageIndex] = myFence;
}
const BR2::usize2& Swapchain::windowSize() {
  return _imageSize;
}
std::shared_ptr<RenderFrame> Swapchain::currentFrame() {
  std::shared_ptr<RenderFrame> frame = nullptr;
  frame = _frames[_currentFrame];
  return frame;
}
std::shared_ptr<RenderTexture> Swapchain::createRenderTexture(const string_t& name, VkFormat format, MSAA msaa, const FilterData& filter) {
  AssertOrThrow2(filter._samplerType != SamplerType::None);
  std::shared_ptr<RenderTexture> rt = std::make_shared<RenderTexture>(name, getThis<Swapchain>(), format, filter);
  rt->createTexture(msaa);
  _renderTextures.push_back(rt);
  return rt;
}
std::shared_ptr<Img32> Swapchain::grabImage(int debugImg) {
  string_t err;
  std::shared_ptr<TextureImage> target = nullptr;
  if (debugImg == 0) {
    target = frames()[0]->getRenderTarget(OutputMRT::RT_DefaultColor, MSAA::Disabled, VK_FORMAT_B8G8R8A8_SRGB, err, VK_NULL_HANDLE, false);
  }
  else if (debugImg == 1) {
    target = _renderTextures[0]->texture(MSAA::Disabled, frames()[0]->frameIndex());
  }
  else {
    return nullptr;
  }

  //Note: we set this to null in GVulkan.cpp. and make a new buffer each frame (thanks shared_ptr)
  // This would be loads faster if we didn't discard the buffer each frame, however
  //i'm not sure i'm going to keep this little feature.
  std::shared_ptr<Img32> img = nullptr;
  if (target) {
    img = target->copyImageFromGPU();
  }
  return img;
}
#pragma endregion

#pragma region Vulkan

std::shared_ptr<Vulkan> Vulkan::create(const string_t& title, SDL_Window* win, bool vsync_enabled, bool wait_fences, bool enableDebug) {
  std::shared_ptr<Vulkan> v = std::make_shared<Vulkan>();
  v->init(title, win, vsync_enabled, wait_fences, enableDebug);
  return v;
}
Vulkan::Vulkan() {
}
Vulkan::~Vulkan() {
  CheckVKRV(vkDeviceWaitIdle, _device);

  _pSwapchain = nullptr;
  _pQueueFamilies = nullptr;

  vkDestroyCommandPool(_device, _commandPool, nullptr);
  vkDestroyDevice(_device, nullptr);
  _pDebug = nullptr;
  vkDestroyInstance(_instance, nullptr);
}
void Vulkan::init(const string_t& title, SDL_Window* win, bool vsync_enabled, bool wait_fences, bool enableDebug) {
  AssertOrThrow2(win != nullptr);

  //Setup vulkan devices
  _vsync_enabled = vsync_enabled;
  _wait_fences = wait_fences;

  initVulkan(title, win, enableDebug);

  int win_w = 0, win_h = 0;
  SDL_GetWindowSize(win, &win_w, &win_h);

  //Create swapchain
  _pSwapchain = std::make_shared<Swapchain>(getThis<Vulkan>());
  _pSwapchain->initSwapchain(BR2::usize2(win_w, win_h));
}

void Vulkan::initVulkan(const string_t& title, SDL_Window* win, bool enableDebug) {
  _pDebug = std::make_unique<VulkanDebug>(getThis<Vulkan>(), enableDebug);
  createInstance(title, win);
  pickPhysicalDevice();
  createLogicalDevice();
  getDeviceProperties();
  createCommandPool();
}
void Vulkan::createInstance(const string_t& title, SDL_Window* win) {
  VkApplicationInfo appInfo{};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = title.c_str();
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "No Engine";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_0;

  VkInstanceCreateInfo createinfo{};
  createinfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createinfo.pApplicationInfo = &appInfo;

  //Validation layers
  std::vector<const char*> layerNames = getValidationLayers();
  if (_bEnableValidationLayers) {
    createinfo.enabledLayerCount = static_cast<uint32_t>(layerNames.size());
    createinfo.ppEnabledLayerNames = layerNames.data();
  }
  else {
    createinfo.enabledLayerCount = 0;
    createinfo.ppEnabledLayerNames = nullptr;
  }

  //Extensions
  std::vector<const char*> extensionNames = getRequiredExtensionNames(win);
  createinfo.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size());
  createinfo.ppEnabledExtensionNames = extensionNames.data();

  CheckVKRV(vkCreateInstance, &createinfo, nullptr, &_instance);

  if (!SDL_Vulkan_CreateSurface(win, _instance, &_windowSurface)) {
    checkErrors();
    errorExit("SDL failed to create vulkan window.");
  }
}

void Vulkan::errorExit(const string_t& str) {
  BRLogError(str);

  SDLUtils::checkSDLErr();
  Gu::debugBreak();

  BRThrowException(str);
}

std::vector<const char*> Vulkan::getRequiredExtensionNames(SDL_Window* win) {
  std::vector<const char*> extensionNames{};
  //TODO: SDL_Vulkan_GetInstanceExtensions -the window parameter may not need to be valid in future releases.
  //**test this.
  //Returns # of REQUIRED instance extensions
  unsigned int extensionCount;
  if (!SDL_Vulkan_GetInstanceExtensions(win, &extensionCount, nullptr)) {
    errorExit("Couldn't get instance extensions");
  }
  extensionNames = std::vector<const char*>(extensionCount);
  if (!SDL_Vulkan_GetInstanceExtensions(win, &extensionCount,
                                        extensionNames.data())) {
    errorExit("Couldn't get instance extensions (2)");
  }

  // Debug print the extension names.
  std::string exts = "";
  std::string del = "";
  for (const char* st : extensionNames) {
    exts += del + std::string(st) + "\r\n";
    del = "  ";
  }
  BRLogInfo("Available Vulkan Extensions: \r\n" + exts);

  if (_bEnableValidationLayers) {
    extensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    extensionNames.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
  }
  return extensionNames;
}
std::vector<const char*> Vulkan::getValidationLayers() {
  std::vector<const char*> layerNames{};
  if (_bEnableValidationLayers) {
    layerNames.push_back("VK_LAYER_LUNARG_standard_validation");
    //layerNames.push_back("VK_LAYER_KHRONOS_validation");
  }

  //Check if validation layers are supported.
  string_t str = "";
  for (auto layer : layerNames) {
    if (!isValidationLayerSupported(layer)) {
      str += Stz layer + "\r\n";
    }
  }
  if (str.length()) {
    errorExit("One or more validation layers are not supported:\r\n" + str);
  }
  str = "Enabling Validation Layers: \r\n";
  for (auto layer : layerNames) {
    str += Stz "  " + layer;
  }
  BRLogInfo(str);

  return layerNames;
}
bool Vulkan::isValidationLayerSupported(const string_t& name) {
  if (supported_validation_layers.size() == 0) {
    std::vector<string_t> names;
    uint32_t layerCount;

    CheckVKRV(vkEnumerateInstanceLayerProperties, &layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    CheckVKRV(vkEnumerateInstanceLayerProperties, &layerCount, availableLayers.data());

    //    CheckVKRV(vkEnumerateInstanceExtensionProperties, nullptr, &layerCount, nullptr);
    //std::vector<VkExtensionProperties> asdfg(layerCount);
    //    CheckVKRV(vkEnumerateInstanceExtensionProperties, nullptr, &layerCount, asdfg.data());

    for (auto layer : availableLayers) {
      supported_validation_layers.insert(std::make_pair(layer.layerName, layer));
    }
  }

  if (supported_validation_layers.find(name) != supported_validation_layers.end()) {
    return true;
  }

  return false;
}

void Vulkan::pickPhysicalDevice() {
  BRLogInfo("  Finding Physical Device.");

  //** TODO: some kind of operatino that lets us choose the best device.
  // Or let the user choose the device, right?

  uint32_t deviceCount = 0;
  CheckVKRV(vkEnumeratePhysicalDevices, _instance, &deviceCount, nullptr);
  if (deviceCount == 0) {
    errorExit("No Vulkan enabled GPUs available.");
  }
  BRLogInfo("Found " + deviceCount + " rendering device(s).");

  std::vector<VkPhysicalDevice> devices(deviceCount);
  CheckVKRV(vkEnumeratePhysicalDevices, _instance, &deviceCount, devices.data());

  // List all devices for debug, and also pick one.
  std::string devInfo = "";
  int i = 0;
  for (const auto& device : devices) {
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
    devInfo += Stz " Device " + i + ": " + deviceProperties.deviceName + "\r\n";
    devInfo += Stz "  Driver Version: " + deviceProperties.driverVersion + "\r\n";
    devInfo += Stz "  API Version: " + deviceProperties.apiVersion + "\r\n";

    //**NOTE** deviceFeatures must be modified in the deviceFeatures in
    if (_physicalDevice == VK_NULL_HANDLE) {
      //We need to fix this to allow for optional samplerate shading.
      if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
          deviceFeatures.geometryShader &&

          // ** This is mostly debugging stuff - If the driver doens't have it we should create one without it.
          deviceFeatures.fillModeNonSolid &&  // VK_POLYGON_MODE
          deviceFeatures.wideLines &&
          deviceFeatures.largePoints &&

          //deviceFeatures. pipelineStatisticsQuery && // -- Query pools
          //imageCubeArray
          //logicOp should be supported

          //MSAA
          deviceFeatures.shaderStorageImageMultisample &&  //Again not necessary but pretty
          //AF
          deviceFeatures.samplerAnisotropy &&
          deviceFeatures.sampleRateShading) {
        _physicalDevice = device;
        _deviceProperties = deviceProperties;  //Make sure this comes before getMaxUsableSampleCount!
        _deviceFeatures = deviceFeatures;
        _bPhysicalDeviceAcquired = true;
      }
    }

    i++;
  }
  BRLogInfo(devInfo);

  if (_physicalDevice == VK_NULL_HANDLE) {
    errorExit("Failed to find a suitable GPU.");
  }
}
std::unordered_map<string_t, VkExtensionProperties>& Vulkan::getDeviceExtensions() {
  if (_deviceExtensions.size() == 0) {
    uint32_t extensionCount;
    CheckVKRV(vkEnumerateDeviceExtensionProperties, _physicalDevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    CheckVKRV(vkEnumerateDeviceExtensionProperties, _physicalDevice, nullptr, &extensionCount, availableExtensions.data());
    for (auto ext : availableExtensions) {
      _deviceExtensions.insert(std::make_pair(ext.extensionName, ext));
    }
  }
  return _deviceExtensions;
}
bool Vulkan::isExtensionSupported(const string_t& extName) {
  if (getDeviceExtensions().find(extName) != getDeviceExtensions().end()) {
    return true;
  }
  return false;
}
bool Vulkan::extensionEnabled(const string_t& in_ext) {
  auto it = _enabledExtensions.find(in_ext);
  return it != _enabledExtensions.end();
}
std::vector<const char*> Vulkan::getEnabledDeviceExtensions() {
  //Required Extensions
  const std::vector<const char*> requiredExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
  };
  //Optional
  const std::vector<const char*> optinalExtensions = {
    VK_AMD_MIXED_ATTACHMENT_SAMPLES_EXTENSION_NAME
  };

  string_t extMsg = "";
  bool fatal = false;
  std::vector<const char*> ret;
  //Check Required
  for (auto strext : requiredExtensions) {
    if (!isExtensionSupported(strext)) {
      extMsg += Stz "  Required extension " + strext + " wasn't supported\n";
      fatal = true;
    }
    else {
      ret.push_back(strext);
      _enabledExtensions.insert(string_t(strext));
    }
  }
  //Check Optional
  for (auto strext : optinalExtensions) {
    if (!isExtensionSupported(strext)) {
      extMsg += Stz "  Optional extension " + strext + " wasn't supported\n";
    }
    else {
      ret.push_back(strext);
      _enabledExtensions.insert(string_t(strext));
    }
  }
  if (extMsg.length()) {
    if (fatal) {
      errorExit(extMsg);
    }
    else {
      BRLogWarn(extMsg);
    }
  }

  return ret;
}
void Vulkan::createLogicalDevice() {
  //Here we should really do a "best fit" like we do for OpenGL contexts.
  BRLogInfo("Creating Logical Device.");

  //**NOTE** deviceFeatures must be modified in the deviceFeatures in
  // isDeviceSuitable
  VkPhysicalDeviceFeatures deviceFeatures{};
  deviceFeatures.geometryShader = VK_TRUE;
  deviceFeatures.fillModeNonSolid = VK_TRUE;
  //widelines, largepoints, individualBlendState

  // Queues
  findQueueFamilies();

  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueFamilies = { _pQueueFamilies->_graphicsFamily.value(),
                                             _pQueueFamilies->_presentFamily.value() };

  float queuePriority = 1.0f;
  for (uint32_t queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
  }

  //Check Device Extensions
  std::vector<const char*> extensions = getEnabledDeviceExtensions();

  // Logical Device
  VkDeviceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
  createInfo.pQueueCreateInfos = queueCreateInfos.data();
  createInfo.pEnabledFeatures = &deviceFeatures;
  createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  createInfo.ppEnabledExtensionNames = extensions.data();

  // Validation layers are Deprecated
  createInfo.enabledLayerCount = 0;

  CheckVKRV(vkCreateDevice, _physicalDevice, &createInfo, nullptr, &_device);

  // Create queues
  //**0 is the queue index - this should be checke to make sure that it's less than the queue family size.
  vkGetDeviceQueue(_device, _pQueueFamilies->_graphicsFamily.value(), 0, &_graphicsQueue);
  vkGetDeviceQueue(_device, _pQueueFamilies->_presentFamily.value(), 0, &_presentQueue);
}
Vulkan::QueueFamilies* Vulkan::findQueueFamilies() {
  if (_pQueueFamilies != nullptr) {
    return _pQueueFamilies.get();
  }
  _pQueueFamilies = std::make_unique<QueueFamilies>();

  //These specify the KIND of queue (command pool)
  //Ex: Grahpics, or Compute, or Present (really that's mostly what there is)
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueFamilyCount, nullptr);

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueFamilyCount, queueFamilies.data());

  string_t qf_info = " Device Queue Families" + Os::newline();

  for (int i = 0; i < queueFamilies.size(); ++i) {
    auto& queueFamily = queueFamilies[i];
    // Check for presentation support
    VkBool32 presentSupport = false;
    CheckVKRV(vkGetPhysicalDeviceSurfaceSupportKHR, _physicalDevice, i, _windowSurface, &presentSupport);

    if (queueFamily.queueCount > 0 && presentSupport) {
      if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        _pQueueFamilies->_graphicsFamily = i;
      }
      if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
        _pQueueFamilies->_computeFamily = i;
      }
      _pQueueFamilies->_presentFamily = i;
    }
  }

  BRLogInfo(qf_info);

  if (_pQueueFamilies->_graphicsFamily.has_value() == false || _pQueueFamilies->_presentFamily.has_value() == false) {
    errorExit("GPU doesn't contain any suitable queue families.");
  }

  return _pQueueFamilies.get();
}
void Vulkan::createCommandPool() {
  BRLogInfo("Creating Command Pool.");
  findQueueFamilies();

  VkCommandPoolCreateInfo poolInfo{
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .queueFamilyIndex = _pQueueFamilies->_graphicsFamily.value()
  };
  CheckVKRV(vkCreateCommandPool, _device, &poolInfo, nullptr, &_commandPool);
}
void Vulkan::getDeviceProperties() {
  CheckVKRV(vkGetPhysicalDeviceSurfaceCapabilitiesKHR, _physicalDevice, _windowSurface, &_surfaceCaps);
  _swapchainImageCount = _surfaceCaps.minImageCount + 1;
  //_swapchainImageCount = _surfaceCaps.maxImageCount;
  if (_swapchainImageCount > _surfaceCaps.maxImageCount) {
    _swapchainImageCount = _surfaceCaps.maxImageCount;
  }
  if (_surfaceCaps.maxImageCount > 0 && _swapchainImageCount > _surfaceCaps.maxImageCount) {
    _swapchainImageCount = _surfaceCaps.maxImageCount;
  }
}
void Vulkan::checkErrors() {
  SDLUtils::checkSDLErr();
}
void Vulkan::validateVkResult(VkResult res, const string_t& fname) {
  checkErrors();
  if (res != VK_SUCCESS) {
    errorExit(Stz "Error: '" + fname + "' returned '" + VulkanUtils::VkResult_toString(res) + "' (" + res + ")" + Os::newline());
  }
}

const VkPhysicalDeviceProperties& Vulkan::deviceProperties() {
  AssertOrThrow2(_bPhysicalDeviceAcquired);
  return _deviceProperties;
}
const VkPhysicalDeviceFeatures& Vulkan::deviceFeatures() {
  AssertOrThrow2(_bPhysicalDeviceAcquired);
  return _deviceFeatures;
}
const VkPhysicalDeviceLimits& Vulkan::deviceLimits() {
  AssertOrThrow2(_bPhysicalDeviceAcquired);
  return _deviceProperties.limits;
}
const VkSurfaceCapabilitiesKHR& Vulkan::surfaceCaps() {
  AssertOrThrow2(_bPhysicalDeviceAcquired);
  return _surfaceCaps;
}
MSAA Vulkan::maxMSAA() {
  AssertOrThrow2(_bPhysicalDeviceAcquired);
  VkSampleCountFlags counts = _deviceProperties.limits.framebufferColorSampleCounts &
                              _deviceProperties.limits.framebufferDepthSampleCounts;
  if (counts & VK_SAMPLE_COUNT_64_BIT) {
    return MSAA::MS_64_Samples;
  }
  else if (counts & VK_SAMPLE_COUNT_32_BIT) {
    return MSAA::MS_32_Samples;
  }
  else if (counts & VK_SAMPLE_COUNT_16_BIT) {
    return MSAA::MS_16_Samples;
  }
  else if (counts & VK_SAMPLE_COUNT_8_BIT) {
    return MSAA::MS_8_Samples;
  }
  else if (counts & VK_SAMPLE_COUNT_4_BIT) {
    return MSAA::MS_4_Samples;
  }
  else if (counts & VK_SAMPLE_COUNT_2_BIT) {
    return MSAA::MS_2_Samples;
  }
  return MSAA::Disabled;
}

VkFormat Vulkan::findDepthFormat() {
  return findSupportedFormat(
    { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT },
    VK_IMAGE_TILING_OPTIMAL,
    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}
VkFormat Vulkan::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
  for (VkFormat format : candidates) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(this->physicalDevice(), format, &props);

    if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
      return format;
    }
    else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
      return format;
    }
  }

  throw std::runtime_error("failed to find supported format!");
}

VkCommandBuffer Vulkan::beginOneTimeGraphicsCommands() {
  VkCommandBufferAllocateInfo allocInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,  //VkStructureType
    .pNext = nullptr,                                         //const void*
    .commandPool = commandPool(),                             //VkCommandPool
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,                 //VkCommandBufferLevel
    .commandBufferCount = 1,                                  //uint32_t
  };

  VkCommandBuffer commandBuffer;
  CheckVKRV(vkAllocateCommandBuffers, device(), &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,  //VkStructureType
    .pNext = nullptr,                                      //const void*
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,  //VkCommandBufferUsageFlags
    .pInheritanceInfo = nullptr,                           //const VkCommandBufferInheritanceInfo*
  };

  CheckVKRV(vkBeginCommandBuffer, commandBuffer, &beginInfo);

  return commandBuffer;
}
void Vulkan::endOneTimeGraphicsCommands(VkCommandBuffer commandBuffer) {
  CheckVKRV(vkEndCommandBuffer, commandBuffer);

  VkSubmitInfo submitInfo = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,  //VkStructureType
    .pNext = nullptr,                        //const void*
    .pWaitSemaphores = nullptr,              //const VkSemaphore*
    .pWaitDstStageMask = nullptr,            //const VkPipelineStageFlags*
    .commandBufferCount = 1,                 //uint32_t
    .pCommandBuffers = &commandBuffer,       //const VkCommandBuffer*
    .signalSemaphoreCount = 0,               //uint32_t
    .pSignalSemaphores = nullptr,            //const VkSemaphore*
  };

  CheckVKRV(vkQueueSubmit, graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
  CheckVKRV(vkQueueWaitIdle, graphicsQueue());

  vkFreeCommandBuffers(device(), commandPool(), 1, &commandBuffer);
}
uint32_t Vulkan::swapchainImageCount() {
  return _swapchainImageCount;
}
std::shared_ptr<Swapchain> Vulkan::swapchain() {
  return _pSwapchain;
}
void Vulkan::waitIdle() {
  //A fence that waits for the GPU to finish operations.
  CheckVKRV(vkDeviceWaitIdle, device());
}
float Vulkan::maxAF() {
  return deviceLimits().maxSamplerAnisotropy;
}

#pragma endregion

}  // namespace VG
