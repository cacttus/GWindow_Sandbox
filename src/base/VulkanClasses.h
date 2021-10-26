/**
*  @file VulkanClasses.h
*  @date 09/15/2020
*  @author Derek Page
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
  Vulkan* _vulkan = nullptr;

public:
  VulkanObject(Vulkan* dev) { _vulkan = dev; }
  virtual ~VulkanObject() {}
  Vulkan* vulkan() { return _vulkan; }
};
class VulkanObjectShared : public VulkanObject, public SharedObject<VulkanObject> {
public:
  VulkanObjectShared(Vulkan* v) : VulkanObject(v) {}
  virtual ~VulkanObjectShared() {}
};
/**
 * @class VulkanDeviceBuffer
 * @brief Represents a getVkBuffer that can reside on the host or on the gpu.
 * */
class VulkanDeviceBuffer : public VulkanObject {
public:
  VulkanDeviceBuffer(Vulkan* pvulkan, size_t itemSize, size_t itemCount, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
  virtual ~VulkanDeviceBuffer() override;

  VkBuffer& getVkBuffer() { return _buffer; }
  VkDeviceMemory bufferMemory() { return _bufferMemory; }
  VkDeviceSize totalSizeBytes() { return _byteSize; }
  size_t itemSize() { return _itemSize; }
  size_t itemCount() { return _itemCount; }

  void copy_from(void* src_buf, size_t copy_count_items, size_t data_offset_items, size_t device_offset_item);
  void copy_to(void* dst_buf, size_t copy_count_items, size_t data_offset_items, size_t buffer_offset_items);
  void copy_device(VulkanDeviceBuffer* host_buf, size_t item_copyCount, size_t itemOffset_Host, size_t itemOffset_Gpu);
  static uint32_t findMemoryType(VkPhysicalDevice d, uint32_t typeFilter, VkMemoryPropertyFlags properties);

private:
  void cleanup();

  size_t _itemSize = 0;
  size_t _itemCount = 0;
  VkBuffer _buffer = VK_NULL_HANDLE;  // If a UniformBuffer, VertexBuffer, or IndexBuffer
  VkDeviceMemory _bufferMemory = VK_NULL_HANDLE;
  VkDeviceSize _byteSize = 0;
  bool _isGpuBuffer = false;  //for staged buffers, this class represents the GPU side of the getVkBuffer. This getVkBuffer resides on the GPU and vkMapMemory can't be called on it.
};
/**
 * @class VulkanBuffer
 * @brief Interface for Vulkan buffers used for vertex and index data.
 *        You're supposed to use getVkBuffer pools as there is a maximum getVkBuffer allocation limit on the GPU, however this is just a demo.
 *        4096 = Nvidia GPU allocation limit.
 */
class VulkanBuffer : public VulkanObject {
public:
  VulkanBuffer(Vulkan* dev, VulkanBufferType eType, bool bStaged, size_t itemSize, size_t itemCount, void* items, size_t item_count);
  virtual ~VulkanBuffer() override;

  VulkanDeviceBuffer* buffer();

  void writeData(void* items, size_t item_count, size_t item_offset = 0);

private:
  std::unique_ptr<VulkanDeviceBuffer> _hostBuffer = nullptr;
  std::unique_ptr<VulkanDeviceBuffer> _gpuBuffer = nullptr;

  VulkanBufferType _eType = VulkanBufferType::VertexBuffer;
  bool _bUseStagingBuffer = false;
};
/**
* @class FilterData
*/
class FilterData {
public:
  SamplerType _samplerType = SamplerType::None;
  MipmapMode _mipmap = MipmapMode::Linear;
  float _anisotropy = 1.0f;  // use vulkan()->maxAF()
  TexFilter _min_filter = TexFilter::Linear;
  TexFilter _mag_filter = TexFilter::Linear;
  uint32_t _mipLevels = MipLevels::Unset;
  static FilterData no_sampler_no_mipmaps() {
    FilterData r{
      ._samplerType = SamplerType::None,
      ._mipmap = MipmapMode::Disabled,
      ._anisotropy = 1.0f,
      ._min_filter = TexFilter::Nearest,
      ._mag_filter = TexFilter::Nearest,
      ._mipLevels = MipLevels::Unset
    };
    return r;
  }
};
/**
* @class Texture2D
* @brief 2D images residing on GPU.
*/
class TextureImage : public VulkanObjectShared {
public:
  TextureImage(Vulkan* v, const string_t& name, TextureType type, MSAA samples, const BR2::usize2& size, VkFormat imgFormat, VkImage img, const FilterData& filter);
  TextureImage(Vulkan* v, const string_t& name, TextureType type, MSAA samples, const BR2::usize2& size, VkFormat imgFormat, const FilterData& filter);
  TextureImage(Vulkan* v, const string_t& name, TextureType type, MSAA samples, std::shared_ptr<Img32>, const FilterData& filter);
  TextureImage(Vulkan* v, const string_t& name, TextureType type, MSAA samples, const FilterData& filter);

  virtual ~TextureImage() override;

  string_t name() { return _name; }
  VkImageView imageView() { return _imageView; }
  VkFormat format() { return _format; }
  VkImage image() { return _image; }
  const BR2::usize2& imageSize() { return _size; }
  MSAA sampleCount() { return _samples; }
  VkSampler sampler() { return _textureSampler; }
  uint32_t mipLevels() { return _filter._mipLevels; }  //See ifthese are actually used.
  bool error() { return _error; }
  const FilterData& filter() { return _filter; }

  static VkSampleCountFlagBits multisampleToVkSampleCountFlagBits(MSAA s);
  static VkSamplerMipmapMode convertMipmapMode(MipmapMode mode, TexFilter filter);
  static void testCycleFilters(TexFilter& g_min_filter, TexFilter& g_mag_filter, MipmapMode& g_mipmap_mode);
  void generateMipmaps(CommandBuffer* buf = nullptr);
  static uint32_t msaa_to_int(MSAA s);
  std::shared_ptr<Img32> copyImageFromGPU();

protected:
  string_t _name = "*unset";
  TextureType _type = TextureType::Unset;
  std::shared_ptr<Img32> _bitmap = nullptr;
  VkImage _image = VK_NULL_HANDLE;  // If this is a VulkanBufferType::Image
  VkDeviceMemory _imageMemory = VK_NULL_HANDLE;
  VkImageView _imageView = VK_NULL_HANDLE;
  BR2::usize2 _size{ 0, 0 };
  VkFormat _format = VK_FORMAT_UNDEFINED;  //Invalid format
  VkSampler _textureSampler = VK_NULL_HANDLE;
  FilterData _filter;
  MSAA _samples = MSAA::Disabled;
  VkImageTiling _tiling = VK_IMAGE_TILING_OPTIMAL;
  VkImageUsageFlags _usage = VK_IMAGE_USAGE_SAMPLED_BIT;
  VkMemoryPropertyFlags _properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  VkImageAspectFlags _aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  VkImageLayout _initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkImageLayout _finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkImageLayout _currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkImageUsageFlags _transferSrc = (VkImageUsageFlags)0;
  bool _error = false;
  bool _ownsImage = true;

  void cleanup();
  void createGPUImage();  // = VK_IMAGE_LAYOUT_UNDEFINED
  void createView();      // = 1
  void createSampler();
  bool isFeatureSupported(VkFormatFeatureFlagBits flag);
  void formatGPUImageMemory();
  void transitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
  void flipImage20161206(uint8_t* image, int width, int height);
  VkFilter convertFilter(TexFilter filter, bool cubicSupported);
  void computeMipLevels();
  bool computeTypeProperties();
  void transitionImage();
};
/**
 * @class CommandBuffer
 * */
class CommandBuffer : public VulkanObject {
public:
  CommandBuffer(Vulkan* ob, RenderFrame* frame);
  virtual ~CommandBuffer() override;

  CommandBufferState state() { return _state; }
  VkCommandBuffer getVkCommandBuffer() { return _commandBuffer; }

  void cmdSetViewport(const BR2::urect2& size);
  void begin();
  bool beginPass();
  void endPass();
  void end();
  void submit(std::vector<VkPipelineStageFlags> waitStages = {}, std::vector<VkSemaphore> waitSemaphores = {}, std::vector<VkSemaphore> signalSemaphores = {}, VkFence submitFence = VK_NULL_HANDLE, bool waitGPUIdle = true);
  void copyBufferToImage(VulkanDeviceBuffer* buf, VkImage img, const BR2::usize2& size);
  void copyImageToBuffer(std::shared_ptr<TextureImage> image, VulkanDeviceBuffer* buf);
  void blitImage(VkImage srcImg, VkImage dstImg, const BR2::irect2& srcRegion, const BR2::irect2& dstRegion,
                 VkImageLayout srcLayout, VkImageLayout dstLayout, uint32_t srcMipLevel, uint32_t dstMipLevel,
                 VkImageAspectFlagBits aspectFlags, VkFilter filter);
  void imageTransferBarrier(VkImage image, VkAccessFlagBits srcAccessFlags, VkAccessFlagBits dstAccessFlags,
                            VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t baseMipLevel, VkImageAspectFlagBits subresourceMask);
  void validateState(bool b);
  void copyBuffer(VkBuffer from, VkBuffer to, size_t count, size_t from_offset, size_t to_offset);
  void bindMesh(std::shared_ptr<Mesh> mesh);
  void drawIndexed(uint32_t instanceCount);

private:
  CommandBufferState _state = CommandBufferState::Unset;
  RenderFrame* _pRenderFrame = nullptr;
  VkCommandPool _sharedPool = VK_NULL_HANDLE;       //Do not free
  VkCommandBuffer _commandBuffer = VK_NULL_HANDLE;  //_commandBuffers;
  VulkanBuffer* _pBoundIndexes = nullptr;
};
/**
 * @class UBOClassData
 * @brief Instance data for an instance UBO
 * */
class UBOClassData {
  // ViewProjUBOData _viewProjUBOData;
  InstanceUBOClassData _instanceUBOSpec;
};
/**
 * @class ShaderDataUBO
 * @brief This is a UBO stored on the rendering frames
 * */
class ShaderDataUBO {
public:
  Descriptor* _descriptor = nullptr;  //shared ptr to the descriptor in the shader.
  std::shared_ptr<VulkanBuffer> _buffer = nullptr;
  UBOClassData _data;
};
/**
 * @class VulkanShaderModule
 * @brief Shader module with reflection thanks to Spirv-Reflect.
 * */
class ShaderModule : VulkanObject {
public:
  ShaderModule(Vulkan* v, const string_t& base_name, const string_t& file);
  virtual ~ShaderModule() override;

  VkPipelineShaderStageCreateInfo getPipelineStageCreateInfo();
  SpvReflectShaderModule* reflectionData();
  const string_t& name();

private:
  class ShaderModule_Internal;
  std::unique_ptr<ShaderModule_Internal> _pInt;
};
/**
 * @class Descriptor
 * @brief Shader input descriptor.
 * */
class Descriptor {
public:
  string_t _name = "";
  VkDescriptorType _type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
  uint32_t _binding = 0;  //The acutal binding index. GLSL: layout(location = ..)
  uint32_t _arraySize = 0;
  uint32_t _blockSizeBytes = 0;
  uint32_t _bufferSizeBytes = 0;
  ShaderStage _stage;
  bool _isBound = false;
  DescriptorFunction _function = DescriptorFunction::Unset; //Used by the engine to auto update common descriptors (lights/MVP matrix), Custom if not auto.
};
/**
 * Vertex attribute
 * TODO: use BR2 attribs.
 * */
class VertexAttribute {
public:
  string_t _name = "";
  uint32_t _componentSizeBytes = 0;  //Size of EACH component
  uint32_t _componentCount = 0;
  uint32_t _matrixSize = 0;  //Number of entries 4, 9, 16 ..
  //VkFormat _format; //TODO ,  FYI Attribute formats use colors.
  VkVertexInputAttributeDescription _desc;
  SpvReflectTypeFlags _typeFlags;
  size_t _totalSizeBytes = 0;
  BR2::VertexUserType _userType;
};
/**
 * @class ShaderOutputBinding
 * @brief Generated FBO metadata from the shader that describes the output image.
 * @details .. Example In GLSL: layout(location = 0) out vec4 _outFBO_DefaultColor;
 * */
class ShaderOutputBinding {
public:
  static constexpr const char* _outFBO_DefaultColor = "_outFBO_DefaultColor";
  static constexpr const char* _outFBO_DefaultDepth = "_outFBO_DefaultDepth";
  static constexpr const char* _outFBO_DF_Position = "_outFBO_DF_Position";
  static constexpr const char* _outFBO_DF_Color = "_outFBO_DF_Color";
  static constexpr const char* _outFBO_DF_Depth_Plane = "_outFBO_DF_Depth_Plane";
  static constexpr const char* _outFBO_DF_Normal = "_outFBO_DF_Normal";
  static constexpr const char* _outFBO_DF_Pick = "_outFBO_DF_Pick";
  static constexpr const char* _outFBO_Custom0 = "_outFBO_Custom0";
  static constexpr const char* _outFBO_Custom1 = "_outFBO_Custom1";
  static constexpr const char* _outFBO_Custom2 = "_outFBO_Custom2";
  static constexpr const char* _outFBO_Custom3 = "_outFBO_Custom3";
  static constexpr const char* _outFBO_Custom4 = "_outFBO_Custom4";
  static constexpr const char* _outFBO_Custom5 = "_outFBO_Custom5";
  static constexpr const char* _outFBO_Custom6 = "_outFBO_Custom6";
  static constexpr const char* _outFBO_Custom7 = "_outFBO_Custom7";
  static constexpr const char* _outFBO_Custom8 = "_outFBO_Custom8";
  static constexpr const char* _outFBO_Custom9 = "_outFBO_Custom9";

  uint32_t _location = 0;
  string_t _name = "";
  VkFormat _format;  // This is incorrect from spv-reflect. Don't use.
  //BlendFunc _blending = BlendFunc::AlphaBlend;
  FBOType _type = FBOType::Undefined;
  OutputMRT _output = OutputMRT::RT_Undefined;
};
/**
* @class ShaderOutputDescription
* @brief User passed output image data description.
*/
class OutputDescription {
public:
  static FBOType outputTypeToFBOType(OutputMRT out);
  string_t _name = "";
  RenderTexture* _texture;
  BlendFunc _blending = BlendFunc::AlphaBlend;
  FBOType _type = FBOType::Undefined;
  BR2::vec4 _clearColor{ 0, 0, 0, 1 };
  bool _clear = true;
  float _clearDepth = 1;
  uint32_t _clearStencil = 0;
  OutputMRT _output = OutputMRT::RT_DefaultColor;
  CompareOp _compareOp = CompareOp::Less;
  ShaderOutputBinding* _outputBinding = nullptr;  // This is set internally when we make the FBO
  bool _resolve = false;                          // used internally do not set this
  static std::unique_ptr<OutputDescription> depthDefault(bool clear = true) {
    //Returns a default output Depth FBO description
    auto outd = std::make_unique<OutputDescription>();
    outd->_name = ShaderOutputBinding::_outFBO_DefaultDepth;
    outd->_texture = nullptr;
    outd->_blending = BlendFunc::Disabled;
    outd->_type = FBOType::Depth;
    outd->_clearColor = { 0, 0, 0, 0 };
    outd->_clearDepth = 1;
    outd->_clearStencil = 0;
    outd->_output = OutputMRT::RT_DefaultDepth;
    outd->_clear = clear;
    return outd;
  }
  static std::unique_ptr<OutputDescription> colorDefault(RenderTexture* tex = nullptr,
                                                            bool clear = true, float clear_r = 0, float clear_g = 0, float clear_b = 0) {
    //Returns a default output Color FBO description
    auto outd = std::make_unique<OutputDescription>();
    outd->_name = ShaderOutputBinding::_outFBO_DefaultColor;
    outd->_texture = tex;  // If null, then we use the default framebuffer
    outd->_blending = BlendFunc::AlphaBlend;
    outd->_type = FBOType::Color;
    outd->_clearColor = { clear_r, clear_g, clear_b, 1 };
    outd->_clearDepth = 1;
    outd->_clearStencil = 0;
    outd->_output = OutputMRT::RT_DefaultColor;
    outd->_clear = clear;
    return outd;
  }
};
/**
* @class RenderTexture
* Stores Encapsulates texture FBO attachments that update when window resizes.
*/
class RenderTexture {
  friend class Swapchain;

public:
  RenderTexture(const string_t& name, Swapchain* swap, VkFormat format, const FilterData& filter);
  virtual ~RenderTexture();
  //**TODO: use a std::map and switch texture based on multisample preference.
  //We do this if this image must match the swapchain.

  std::shared_ptr<TextureImage> texture(MSAA msaa, uint32_t frame);
  void createTexture(MSAA msaa);

private:
  void recreateAllTextures();

  std::map<MSAA, std::vector<std::shared_ptr<TextureImage>>> _textures;
  FilterData _filter;
  string_t _name = "unset";
  VkFormat _format = VK_FORMAT_UNDEFINED;
  std::shared_ptr<TextureImage> _texture = nullptr;
  Swapchain* _swapchain = nullptr;
};
/**
 * @class PassDescription
 * @brief Describes a rendering pass FBO
 * */
class PassDescription {
public:
  PassDescription(RenderFrame* frame, PipelineShader* shader, MSAA c, BlendFunc globalBlend, FramebufferBlendMode rbm);

  void setOutput(std::unique_ptr<OutputDescription> output);
  void setOutput(const string_t& tag, OutputMRT output, RenderTexture* tex, BlendFunc blend, bool clear = true, float clear_r = 0, float clear_g = 0, float clear_b = 0);

  const std::vector<std::unique_ptr<OutputDescription>>& outputs() { return _outputs; }
  std::vector<VkClearValue> getClearValues();
  RenderFrame* frame() { return _frame; }
  MSAA sampleCount() { return _sampleCount; }
  bool valid() { return _bValid; }
  PipelineShader* shader() { return _shader; }
  bool hasDepthBuffer();
  uint32_t colorOutputCount();
  BlendFunc globalBlend() { return _globalBlend; }
  FramebufferBlendMode blendMode() { return _blendMode; }

private:
  bool passError(const string_t& msg);
  void addValidOutput(std::unique_ptr<OutputDescription> desc);

  std::vector<std::unique_ptr<OutputDescription>> _outputs;
  PipelineShader* _shader = nullptr;
  RenderFrame* _frame = nullptr;
  bool _bValid = true;  //If an error happened
  MSAA _sampleCount = MSAA::Unset;
  FramebufferBlendMode _blendMode = FramebufferBlendMode::Global;
  BlendFunc _globalBlend = BlendFunc::Disabled;
};
/**
 * @class FramebufferAttachment
 * */
class FramebufferAttachment : public VulkanObject {
public:
  static const uint32_t InvalidLocation = 999999;
  FramebufferAttachment(Vulkan* v, OutputDescription* desc);
  virtual ~FramebufferAttachment() override;

  bool init(Framebuffer* fbo, RenderFrame* frame);
  //VkImageView getVkImageView() { return _outputImageView; }
  const BR2::usize2& imageSize() { return _imageSize; }
  OutputDescription* desc() { return _desc; }
  VkImageLayout finalLayout() { return _computedFinalLayout; }
  //VkFormat format() { return _computedFormat; }
  uint32_t location() { return _computedLocation; }
  std::shared_ptr<TextureImage> target() { return _target; }

private:
  void createTarget(Framebuffer* fb, RenderFrame* frame, OutputDescription* out_att);
  uint32_t computeLocation(Framebuffer* fb);
  VkImageLayout computeFinalLayout(Framebuffer* fb, OutputDescription* out_att);

  // VkImageView _outputImageView = VK_NULL_HANDLE;
  BR2::usize2 _imageSize{ 0, 0 };
  OutputDescription* _desc = nullptr;
  std::shared_ptr<TextureImage> _target = nullptr;  //this must be set.
  VkImageLayout _computedFinalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  // VkFormat _computedFormat = VK_FORMAT_UNDEFINED;
  uint32_t _computedLocation = FramebufferAttachment::InvalidLocation;
};
/**
 * @class Framebuffer
 * @brief Shader output. Tied to a shader.
 * */
class Framebuffer : public VulkanObject {
public:
  Framebuffer(Vulkan* v);
  virtual ~Framebuffer() override;

  string_t name() { return _name; }
  VkFramebuffer getVkFramebuffer() { return _framebuffer; }
  VkRenderPass getVkRenderPass() { return _renderPass; }
  MSAA sampleCount();
  bool valid() { return _bValid; }
  const BR2::usize2& imageSize();
  std::vector<std::unique_ptr<FramebufferAttachment>>& attachments() { return _attachments; }
  PassDescription* passDescription() { return _passDescription.get(); }

  bool create(const string_t& name, RenderFrame* frame, std::unique_ptr<PassDescription> desc);
  bool pipelineError(const string_t& msg);
  uint32_t nextLocation();
  uint32_t maxLocation();

private:
  bool createAttachments();
  bool createRenderPass(RenderFrame* frame, Framebuffer* fbo);
  bool validate();

  std::vector<std::unique_ptr<OutputDescription>> _resolveDescriptions;
  string_t _name = "*unset";
  VkFramebuffer _framebuffer = VK_NULL_HANDLE;
  std::vector<std::unique_ptr<FramebufferAttachment>> _attachments;
  RenderFrame* _frame = nullptr;
  std::unique_ptr<PassDescription> _passDescription = nullptr;
  VkRenderPass _renderPass = VK_NULL_HANDLE;
  bool _bValid = true;
  uint32_t _currentLocation = 0;
};
/**
 * @class Pipeline
 * @brief Essentially, a GL ShaderProgram with VAO state.
 * */
class Pipeline : public VulkanObject {
public:
  Pipeline(Vulkan* v,
           VkPrimitiveTopology topo,
           VkPolygonMode mode, VkCullModeFlags cm);
  virtual ~Pipeline() override;
  bool init(PipelineShader* shader,
            std::shared_ptr<BR2::VertexFormat> vtxFormat,
            Framebuffer* fbo);

  VkPipeline getVkPipeline() { return _pipeline; }
  VkPipelineLayout getVkPipelineLayout() { return _pipelineLayout; }
  VkPrimitiveTopology primitiveTopology() { return _primitiveTopology; }
  VkPolygonMode polygonMode() { return _polygonMode; }
  VkCullModeFlags cullMode() { return _cullMode; }
  std::shared_ptr<BR2::VertexFormat> vertexFormat() { return _vertexFormat; }
  Framebuffer* fbo() { return _fbo; }

private:
  VkPipelineColorBlendAttachmentState getVkPipelineColorBlendAttachmentState(BlendFunc bf, Framebuffer* fb);

  std::shared_ptr<BR2::VertexFormat> _vertexFormat = nullptr;
  VkPrimitiveTopology _primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkPolygonMode _polygonMode = VK_POLYGON_MODE_FILL;
  VkCullModeFlags _cullMode = VK_CULL_MODE_NONE;
  VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;
  VkPipeline _pipeline = VK_NULL_HANDLE;
  Framebuffer* _fbo = nullptr;
};
/**
 * @class PipelineShader
 * @brief This is pipeline + shader modules rolled into a single object.
 * */
class PipelineShader : public VulkanObject {
public:
  static std::unique_ptr<PipelineShader> create(Vulkan* v, const string_t& name, const std::vector<string_t>& files);
  PipelineShader(Vulkan* v, const string_t& name, const std::vector<string_t>& files);
  virtual ~PipelineShader() override;

  const string_t& name() { return _name; }
  bool valid() { return _bValid; }
  bool shaderError(const string_t& msg);
  bool renderError(const string_t& msg);
  const std::vector<std::unique_ptr<ShaderOutputBinding>>& outputBindings() const { return _outputBindings; }
  const std::vector<uint32_t> locations() { return _locations; }
  VkDescriptorSetLayout getVkDescriptorSetLayout() { return _descriptorSetLayout; }
  std::vector<VkPipelineShaderStageCreateInfo> getShaderStageCreateInfos();
  VkPipelineVertexInputStateCreateInfo getVertexInputInfo(std::shared_ptr<BR2::VertexFormat> fmt);
  bool sampleShadingVariables();
  Pipeline* getPipeline(std::shared_ptr<BR2::VertexFormat> vertexFormat, VkPrimitiveTopology topo, VkPolygonMode mode, VkCullModeFlags cullMode);
  std::shared_ptr<VulkanBuffer> getUBO(const string_t& name, RenderFrame* frame);
  bool createUBO(const string_t& name, const string_t& var_name, size_t itemSize, size_t itemCount);
  Pipeline* boundPipeline() { return _pBoundPipeline; }
  void clearShaderDataCache(RenderFrame* frame);

  std::unique_ptr<PassDescription> getPass(RenderFrame* frame, MSAA sampleCount, BlendFunc globalBlend, FramebufferBlendMode blendMode = FramebufferBlendMode::Global);
  bool beginRenderPass(CommandBuffer* buf, std::unique_ptr<PassDescription> desc, BR2::urect2* extent = nullptr);
  void endRenderPass(CommandBuffer* buf);
  bool bindUBO(const string_t& name, std::shared_ptr<VulkanBuffer> buf, VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE);  //buf =  Optionally, update.
  bool bindSampler(const string_t& name, std::shared_ptr<TextureImage> texture, uint32_t arrayIndex = 0);
  bool bindPipeline(CommandBuffer* cmd, std::shared_ptr<BR2::VertexFormat> v_fmt, VkPolygonMode mode = VK_POLYGON_MODE_FILL,
                    VkPrimitiveTopology topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VkCullModeFlags cull = VK_CULL_MODE_BACK_BIT);
  bool bindPipeline(CommandBuffer* cmd, Pipeline* pipe);
  void bindViewport(CommandBuffer* cmd, const BR2::urect2& size);
  bool bindDescriptors(CommandBuffer* cmd);
  void drawIndexed(CommandBuffer* cmd, std::shared_ptr<Mesh> m, uint32_t numInstances);

private:
  bool init();
  bool checkGood();
  bool createInputs();
  bool createOutputs();
  bool createDescriptors();
  void cleanupDescriptors();
  Framebuffer* getOrCreateFramebuffer(RenderFrame* frame, ShaderData* data, std::unique_ptr<PassDescription> desc);
  Framebuffer* findFramebuffer(ShaderData* data, PassDescription* desc);
  ShaderData* getShaderData(RenderFrame* frame);
  OutputMRT parseShaderOutputTag(const string_t& tag);
  DescriptorFunction classifyDescriptor(const string_t& name);
  BR2::VertexUserType parseUserType(const string_t& err);
  ShaderModule* getModule(ShaderStage stage, bool throwIfNotFound = false);
  Descriptor* getDescriptor(const string_t& name);
  VkFormat spvReflectFormatToVulkanFormat(SpvReflectFormat fmt);
  bool beginPassGood();
  string_t createUniqueFBOName(RenderFrame* frame, ShaderData* data, PassDescription* desc);

  string_t _name = "*undefined*";
  std::vector<string_t> _files;
  VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;
  VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> _descriptorSets;  //One per frame, TODO: put these on ShaderData (one per frame)
  std::vector<VkVertexInputAttributeDescription> _attribDescriptions;
  VkVertexInputBindingDescription _bindingDesc;
  std::vector<std::unique_ptr<ShaderModule>> _modules;
  std::unordered_map<string_t, std::unique_ptr<Descriptor>> _descriptors; //Maps descriptor name e.g. "_viewMatrix" to the descriptor (specific shader input)
  std::vector<std::unique_ptr<VertexAttribute>> _attributes;
  std::vector<std::unique_ptr<ShaderOutputBinding>> _outputBindings;
  Framebuffer* _pBoundFBO = nullptr;
  Pipeline* _pBoundPipeline = nullptr;
  ShaderData* _pBoundData = nullptr;
  RenderFrame* _pBoundFrame = nullptr;
  bool _bInstanced = false;  //True if we find gl_InstanceIndex (gl_instanceID) in the shader - and we will bind vertexes per instance.
  bool _bValid = true;       // TODO: flags
  std::map<uint32_t, std::unique_ptr<ShaderData>> _shaderData;
  std::vector<uint32_t> _locations;
};
/**
 * @class ShaderData
 * @brief Swapchain Frame (async) shader data.
 * @details Contains one set of data per async frame. Each vectors/array should have
 *          the same number of first rank elements which corresponds to one frame in the Swapchain.
 * */
class ShaderData {
public:
  std::unordered_map<std::string, std::unique_ptr<ShaderDataUBO>> _uniformBuffers;
  ShaderDataUBO* getUBOData(const string_t& name);
  std::vector<std::unique_ptr<Framebuffer>> _framebuffers;  //In the future we can optimize this search.
  std::vector<std::unique_ptr<Pipeline>> _pipelines;        // All pipelines bound to this data.
};
/**
 * @class RenderFrame
 *  RenderPass is Shadred among FBOS and shaders
 */
class RenderFrame : public VulkanObject {
public:
  RenderFrame(Vulkan* v);
  virtual ~RenderFrame() override;

  const BR2::usize2& imageSize();
  Swapchain* getSwapchain() const { return _pSwapchain; }
  //VkImageView getVkImageView() { return _swapImageView; }
  CommandBuffer* commandBuffer() { return _pCommandBuffer.get(); }               //Possible to have multiple buffers as vkQUeueSubmit allows for multiple. Need?
  uint32_t currentRenderingImageIndex() { return _currentRenderingImageIndex; }  //TODO: remove later
  uint32_t frameIndex() { return _frameIndex; }                                  //Image index in the swapchain array

  void init(Swapchain* ps, uint32_t frameIndex, VkImage swapImg, VkSurfaceFormatKHR fmt);
  bool beginFrame();
  void endFrame();

  std::shared_ptr<TextureImage> getRenderTarget(OutputMRT target, MSAA samples, VkFormat format, string_t& out_errors, VkImage swapImg, bool createNew);

private:
  void createSyncObjects();
  void addRenderTarget(OutputMRT output, MSAA samples, std::shared_ptr<TextureImage> tex);
  std::shared_ptr<TextureImage> createNewRenderTarget(OutputMRT target, MSAA samples, VkFormat format, string_t& out_error, VkImage swapImage);

  FrameState _frameState = FrameState::Unset;

  Swapchain* _pSwapchain = nullptr;
  std::unique_ptr<CommandBuffer> _pCommandBuffer = nullptr;

  uint32_t _frameIndex = 0;

  std::map<OutputMRT, std::map<MSAA, std::shared_ptr<TextureImage>>> _renderTargets;  //Stores output images by their ShaderOutput, and by their MSAA level. MAX 2 MSAA images.

  VkFence _inFlightFence = VK_NULL_HANDLE;
  VkFence _imageInFlightFence = VK_NULL_HANDLE;
  VkSemaphore _imageAvailableSemaphore = VK_NULL_HANDLE;
  VkSemaphore _renderFinishedSemaphore = VK_NULL_HANDLE;
  uint32_t _currentRenderingImageIndex = 0;
};
/**
 * @class Swapchain
 * @brief The n-buffered asynchronous rendering swapchain.
 * */
class Swapchain : public VulkanObject {
public:
  Swapchain(Vulkan* v);  //TODO: bool vsync -> use FIFO swapchain mode.
  virtual ~Swapchain() override;

  const uint32_t maxRenderFrameMSAAImages() { return 2; }  //* To prevent memory overflow we will limit MSAA image levels to a maximum of 2: Disabled (default), and a single MSAA
  void outOfDate() { _bSwapChainOutOfDate = true; }
  bool isOutOfDate() { return _bSwapChainOutOfDate; }
  void waitImage(uint32_t imageIndex, VkFence myFence);
  VkSwapchainKHR getVkSwapchain() { return _swapChain; }
  const std::vector<std::unique_ptr<RenderFrame>>& frames() { return _frames; }
  RenderFrame* currentFrame();
  const BR2::usize2& windowSize();
  VkFormat imageFormat() { return _surfaceFormat.format; }

  void initSwapchain(const BR2::usize2& window_size);
  bool beginFrame(const BR2::usize2& windowsize);
  void endFrame();
  void registerShader(PipelineShader* shader);
  void unregisterShader(PipelineShader* shader);
  RenderTexture* getRenderTexture(const string_t& name, VkFormat format, MSAA msaa, const FilterData& filter);
  void copyImageFlag() {
    _copyImage_Flag = true;
  }
  std::shared_ptr<Img32> grabImage(int debugImg);
  const VkSurfaceCapabilitiesKHR& surfaceCaps();
  uint32_t swapchainImageCount();

private:
  void createSwapChain(const BR2::usize2& window_size);
  void cleanupSwapChain();
  bool findValidSurfaceFormat(std::vector<VkFormat> fmts, VkSurfaceFormatKHR& fmt_out);
  bool findValidPresentMode(VkPresentModeKHR& pm_out);
  void refreshSurfaceCaps();

  bool _copyImage_Flag = false;

  FrameState _frameState = FrameState::Unset;
  std::unordered_set<PipelineShader*> _shaders;
  std::vector<std::unique_ptr<RenderFrame>> _frames;
  size_t _currentFrame = 0;
  std::vector<VkFence> _imagesInFlight;  //Shared handle do not delete
  VkSwapchainKHR _swapChain = VK_NULL_HANDLE;
  BR2::usize2 _imageSize{ 0, 0 };
  bool _bSwapChainOutOfDate = false;
  std::unordered_map<string_t, std::unique_ptr<RenderTexture>> _renderTextures;
  VkSurfaceFormatKHR _surfaceFormat;
  VkSurfaceCapabilitiesKHR _surfaceCaps;
};
class ValidationLayerExtension {
public:
  const char* _layer;
  std::vector<const char*> _extensions;
};
/**
*  @class VulkanDebug
*  @brief A lot of vulkan errors are simple access violations relevant to a struct.
*         Outputs memory offsets of vulkan structs for debugging.
*         Provides enum stringification.
*/
class VulkanDebug : public VulkanObject {
public:
  VulkanDebug(Vulkan* v, bool bEnableDebug);
  virtual ~VulkanDebug() override;
  bool debugEnabled() { return _enableDebug; }
  void createDebugObjects();

private:
  void createDebugMessenger();
  void createDebugReport();

  bool _enableDebug = true;
  VkExtFn(vkCreateDebugUtilsMessengerEXT);
  VkExtFn(vkDestroyDebugUtilsMessengerEXT);
  VkExtFn(vkCreateDebugReportCallbackEXT);
  VkExtFn(vkDestroyDebugReportCallbackEXT);

  VkDebugUtilsMessengerEXT _debugMessenger = VK_NULL_HANDLE;
  VkDebugReportCallbackEXT _debugReporter = VK_NULL_HANDLE;
};
/**
 * @class Vulkan 
 * @brief Root class for the GWindow vulkan api.
 */
class Vulkan /*: public SharedObject<Vulkan> */ {
public:
  struct QueueFamilies {
    std::optional<uint32_t> _graphicsFamily;
    std::optional<uint32_t> _computeFamily;
    std::optional<uint32_t> _presentFamily;
  };

public:
  static constexpr const char* c_strErrDeviceLost = "VK_ERROR_DEVICE_LOST";  //TODO; move this somewhere else.
  Vulkan();
  virtual ~Vulkan();

  static std::unique_ptr<Vulkan> create(const string_t& title, SDL_Window* win, bool vsync_enabled, bool wait_fences, bool enableDebug);

  const VkSurfaceKHR& windowSurface() { return _windowSurface; }
  const VkPhysicalDevice& physicalDevice() { return _physicalDevice; }
  const VkDevice& device() { return _device; }
  const VkInstance& instance() { return _instance; }
  const VkCommandPool& commandPool() { return _commandPool; }
  const VkQueue& graphicsQueue() { return _graphicsQueue; }
  const VkQueue& presentQueue() { return _presentQueue; }
  bool vsyncEnabled() { return _vsync_enabled; }
  bool waitFences() { return _wait_fences; }
  const VkPhysicalDeviceProperties& deviceProperties();
  const VkPhysicalDeviceLimits& deviceLimits();
  const VkPhysicalDeviceFeatures& deviceFeatures();
  uint32_t swapchainImageCount();
  MSAA maxMSAA();
  float maxAF();
  void set_vsync(bool enable);
  void set_wait_fences(bool enable);

  void waitIdle();
  void checkErrors();
  void validateVkResult(VkResult res, const string_t& fname);
  void errorExit(const string_t&);
  bool extensionEnabled(const string_t& in_ext);
  VkFormat findDepthFormat();
  VkCommandBuffer beginOneTimeGraphicsCommands();
  void endOneTimeGraphicsCommands(VkCommandBuffer commandBuffer);
  Swapchain* swapchain();

private:
  void init(const string_t& title, SDL_Window* win, bool vsync_enabled, bool wait_fences, bool enableDebug);
  void initVulkan(const string_t& title, SDL_Window* win, bool enableDebug);
  void createInstance(const string_t& title, SDL_Window* win, bool enableDebug);
  std::vector<const char*> getRequiredExtensionNames(SDL_Window* win);
  std::vector<const char*> getValidationLayers();
  bool isValidationLayerSupported(const string_t& name);
  void pickPhysicalDevice();
  std::unordered_map<string_t, VkExtensionProperties>& getDeviceExtensions();
  bool isExtensionSupported(const string_t& extName);
  std::vector<const char*> getEnabledDeviceExtensions();
  void createLogicalDevice();
  Vulkan::QueueFamilies* findQueueFamilies();
  void createCommandPool();
  VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

  std::unique_ptr<VulkanDebug> _pDebug;
  std::unique_ptr<QueueFamilies> _pQueueFamilies;
  std::unique_ptr<Swapchain> _pSwapchain = nullptr;
  VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
  VkDevice _device = VK_NULL_HANDLE;
  VkInstance _instance = VK_NULL_HANDLE;
  VkCommandPool _commandPool = VK_NULL_HANDLE;
  VkQueue _graphicsQueue = VK_NULL_HANDLE;  // Device queues are implicitly cleaned up when the device is destroyed, so we don't need to do anything in cleanup.
  VkQueue _presentQueue = VK_NULL_HANDLE;
  VkSurfaceKHR _windowSurface;
  std::unordered_map<string_t, VkExtensionProperties> _deviceExtensions;
  std::unordered_map<std::string, VkLayerProperties> supported_validation_layers;
  bool _bPhysicalDeviceAcquired = false;
  bool _vsync_enabled = false;
  bool _wait_fences = false;
  std::unordered_set<std::string> _enabledExtensions;
  VkPhysicalDeviceProperties _deviceProperties;
  VkPhysicalDeviceLimits _physicalDeviceLimits;
  VkPhysicalDeviceFeatures _deviceFeatures;
};

}  // namespace VG

#endif
