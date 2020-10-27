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
  class VulkanDeviceBuffer_Internal;
  void cleanup();
  std::unique_ptr<VulkanDeviceBuffer_Internal> _pInt;
};
/**
 * @class VulkanBuffer
 * @brief Interface for Vulkan buffers used for vertex and index data.
 *        You're supposed to use buffer pools as there is a maximum buffer allocation limit on the GPU, however this is just a demo.
 *        4096 = Nvidia GPU allocation limit.
 */
class VulkanBuffer : public VulkanObject {
public:
  VulkanBuffer(std::shared_ptr<Vulkan> dev, VulkanBufferType eType, bool bStaged);
  VulkanBuffer(std::shared_ptr<Vulkan> dev, VulkanBufferType eType, bool bStaged, VkDeviceSize bufsize, void* data = nullptr, size_t datasize = 0);
  virtual ~VulkanBuffer() override;
  void writeData(void* data, size_t off, size_t datasize);
  std::shared_ptr<VulkanDeviceBuffer> hostBuffer();
  std::shared_ptr<VulkanDeviceBuffer> gpuBuffer();

private:
  class VulkanBuffer_Internal;
  std::unique_ptr<VulkanBuffer_Internal> _pInt;
};
/**
* @class VulkanImage
* @brief 2D images residing on GPU.
*/
class VulkanImage : public VulkanObject {
public:
  VulkanImage(std::shared_ptr<Vulkan> pvulkan);
  VulkanImage(std::shared_ptr<Vulkan> pvulkan, VkImage img, uint32_t w, uint32_t h, VkFormat imgFormat);

  virtual ~VulkanImage() override;

  VkImageView imageView() { return _imageView; }
  VkFormat format() { return _format; }
  VkImage image() { return _image; }

  void allocateMemory(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                      VkImageUsageFlags usage, VkMemoryPropertyFlags properties, uint32_t mipLevels,
                      VkSampleCountFlagBits samples);
  void createView(VkFormat fmt, VkImageAspectFlagBits aspect = VK_IMAGE_ASPECT_COLOR_BIT, uint32_t mipLevel = 1);

protected:
  VkImage _image = VK_NULL_HANDLE;  // If this is a VulkanBufferType::Image
  VkDeviceMemory _imageMemory = VK_NULL_HANDLE;
  VkImageView _imageView = VK_NULL_HANDLE;
  uint32_t _width = 0;
  uint32_t _height = 0;
  VkFormat _format = VK_FORMAT_UNDEFINED;  //Invalid format
};
/**
* @class VulkanCommands
* @brief Common graphics commands.
*/
class VulkanCommands : public VulkanObject {
public:
  VulkanCommands(std::shared_ptr<Vulkan> v);
  void begin();
  void end();
  void blitImage(VkImage srcImg,
                 VkImage dstImg,
                 const BR2::irect2& srcRegion,
                 const BR2::irect2& dstRegion,
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
 * @class CommandBuffer
 * */
class CommandBuffer : public VulkanObject {
public:
  CommandBuffer(std::shared_ptr<Vulkan> ob, std::shared_ptr<RenderFrame> frame);
  virtual ~CommandBuffer() override;

  VkCommandBuffer getVkCommandBuffer() { return _commandBuffer; }
  void cmdSetViewport(const BR2::urect2& size);
  void begin();
  void end();
  bool beginPass();
  void endPass();

private:
  void validateState(bool b);

  CommandBufferState _state = CommandBufferState::Unset;
  std::shared_ptr<RenderFrame> _pRenderFrame = nullptr;
  VkCommandPool _sharedPool = VK_NULL_HANDLE;       //Do not free
  VkCommandBuffer _commandBuffer = VK_NULL_HANDLE;  //_commandBuffers;
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
  std::shared_ptr<Descriptor> _descriptor = nullptr;  //shared ptr to the descriptor in the shader.
  std::shared_ptr<VulkanBuffer> _buffer = nullptr;
  UBOClassData _data;
};
/**
*  @class VulkanTextureImage
*  @brief Image residing on the GPU.
*/
class VulkanTextureImage : public VulkanImage {
public:
  VulkanTextureImage(std::shared_ptr<Vulkan> pvulkan, std::shared_ptr<Img32> pimg, MipmapMode mipmaps, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
  VulkanTextureImage(std::shared_ptr<Vulkan> pvulkan, uint32_t w, uint32_t h, MipmapMode mipmaps, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
  virtual ~VulkanTextureImage() override;
  VkSampler sampler();

  void recreateMipmaps(MipmapMode mipmaps);

private:
  std::shared_ptr<VulkanDeviceBuffer> _host = nullptr;
  VkSampler _textureSampler = VK_NULL_HANDLE;
  uint32_t _mipLevels = 1;
  MipmapMode _mipmap = MipmapMode::Linear;

  void createSampler();
  bool mipmappingSupported();
  void generateMipmaps();
  void copyImageToGPU(std::shared_ptr<Img32> pimg, VkFormat img_fmt);
  void copyBufferToImage(std::shared_ptr<Img32> pimg);
  void transitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
  void flipImage20161206(uint8_t* image, int width, int height);
};
/**
 * @class VulkanShaderModule
 * @brief Shader module with reflection tanks to Spirv-Reflect.
 * */
class ShaderModule : VulkanObject {
public:
  ShaderModule(std::shared_ptr<Vulkan> v, const string_t& base_name, const string_t& file);
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
  uint32_t _binding = 0;  //The acutal binding index.
  uint32_t _arraySize = 0;
  uint32_t _blockSize = 0;
  VkShaderStageFlags _stage;
  bool _isBound = false;
  DescriptorFunction _function = DescriptorFunction::Unset;
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
  //VkFormat _format; // FYI Attribute formats use colors.
  VkVertexInputAttributeDescription _desc;
  SpvReflectTypeFlags _typeFlags;
  size_t _totalSizeBytes = 0;
  BR2::VertexUserType _userType;
};
/**
 * @class ShaderOutputBinding
 * @brief Generated FBO metadata from the shader that describes the output image.
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
  std::shared_ptr<VulkanTextureImage> _texture = nullptr;
  BlendFunc _blending = BlendFunc::AlphaBlend;
  FBOType _type = FBOType::Undefined;
  BR2::vec4 _clearColor{ 0, 0, 0, 1 };
  bool _clear = true;
  float _clearDepth = 1;
  uint32_t _clearStencil = 0;
  OutputMRT _output = OutputMRT::RT_DefaultColor;
  CompareOp _compareOp = CompareOp::Less;
  std::shared_ptr<ShaderOutputBinding> _outputBinding = nullptr;  // This is set internally when we make the FBO
  static std::shared_ptr<OutputDescription> getDepthDF(bool clear = true) {
    auto outd = std::make_shared<OutputDescription>();
    outd->_name = ShaderOutputBinding::_outFBO_DefaultDepth;
    outd->_texture = nullptr;
    outd->_blending = BlendFunc::Disabled;
    outd->_type = FBOType::Depth;
    outd->_clearColor = { 0, 0, 0, 1 };
    outd->_clearDepth = 1;
    outd->_clearStencil = 0;
    outd->_output = OutputMRT::RT_DefaultDepth;
    outd->_clear = clear;
    return outd;
  }
  static std::shared_ptr<OutputDescription> getColorDF(std::shared_ptr<VulkanTextureImage> tex = nullptr, bool clear = true) {
    auto outd = std::make_shared<OutputDescription>();
    outd->_name = ShaderOutputBinding::_outFBO_DefaultColor;
    outd->_texture = tex;  // If null, then we use the default framebuffer
    outd->_blending = BlendFunc::AlphaBlend;
    outd->_type = FBOType::Color;
    outd->_clearColor = { 1, 0, 0, 1 };
    outd->_clearDepth = 1;
    outd->_clearStencil = 0;
    outd->_output = OutputMRT::RT_DefaultColor;
    outd->_clear = clear;
    return outd;
  }
};
/**
 * @class PassDescription
 * @brief Describes a rendering pass FBO
 * */
class PassDescription {
  std::vector<std::shared_ptr<OutputDescription>> _outputs;
  std::shared_ptr<PipelineShader> _shader = nullptr;
  void addValidOutput(std::shared_ptr<OutputDescription> desc);

public:
  PassDescription(std::shared_ptr<PipelineShader> shader);
  void setOutput(std::shared_ptr<OutputDescription> output);
  void setOutput(OutputMRT output, std::shared_ptr<VulkanTextureImage> tex, BlendFunc blend, bool clear);
  const std::vector<std::shared_ptr<OutputDescription>> outputs() { return _outputs; }
};
/**
 * @class FramebufferAttachment
 * */
class FramebufferAttachment : public VulkanObject {
public:
  FramebufferAttachment(std::shared_ptr<Vulkan> v, FBOType type, const string_t& name, VkFormat fbo_fmt, uint32_t glsl_location,
                        const BR2::usize2& imageSize, VkImage swap_img, VkFormat swap_format, const BR2::vec4& clearColor, BlendFunc blending, float clearDepth, uint32_t clearStencil, bool clear);
  virtual ~FramebufferAttachment() override;

  bool init(std::shared_ptr<Framebuffer> fbo);
  VkImageView getVkImageView() { return _outputImageView; }
  const string_t& name() { return _name; }
  uint32_t location() { return _location; }
  //VkFormat fboFormat() { return _fboFormat; }
  BlendFunc blendFunc() { return _blending; }
  FBOType type() { return _fboType; }
  const BR2::vec4& clearColor() { return _clearColor; }  // If this is a depth FBO x = clear color depth
  const BR2::usize2& imageSize() { return _imageSize; }
  float clearDepth() { return _clearDepth; }  // If this is a depth FBO x = clear color depth
  uint32_t clearStencil() { return _clearStencil; }
  bool clear() { return _clear; }

private:
  OutputDescription _output;
  string_t _name = "";
  uint32_t _location = 0;
  BR2::vec4 _clearColor = BR2::vec4(0, 0, 0, 1);
  float _clearDepth = 1;
  uint32_t _clearStencil = 0;
  BlendFunc _blending = BlendFunc::Disabled;
  VkFormat _fboFormat = VK_FORMAT_UNDEFINED;
  VkImage _outputImage = VK_NULL_HANDLE;
  VkFormat _outputImageFormat = VK_FORMAT_UNDEFINED;
  VkImageView _outputImageView = VK_NULL_HANDLE;
  VkImage _image = VK_NULL_HANDLE;
  FBOType _fboType = FBOType::Undefined;
  BR2::usize2 _imageSize{ 0, 0 };
  bool _clear = true;
};
/**
 * @class Framebuffer
 * @brief Shader output. Tied to a shader.
 * */
class Framebuffer : public VulkanObject {
public:
  Framebuffer(std::shared_ptr<Vulkan> v);
  virtual ~Framebuffer() override;

  VkFramebuffer getVkFramebuffer() { return _framebuffer; }
  VkRenderPass getVkRenderPass() { return _renderPass; }

  bool create(std::shared_ptr<RenderFrame> frame, std::shared_ptr<PassDescription> desc);
  std::vector<std::shared_ptr<FramebufferAttachment>> attachments() { return _attachments; }
  std::shared_ptr<PassDescription> passDescription() { return _passDescription; }
  bool pipelineError(const string_t& msg);
  bool valid() { return _bValid; }
  const BR2::usize2& imageSize();

private:
  bool createAttachments();
  bool createRenderPass(std::shared_ptr<Framebuffer> fbo);
  bool getOutputImageDataForMRTType(std::shared_ptr<RenderFrame> frame, std::shared_ptr<OutputDescription> out_att, VkImage& out_image, VkFormat& out_format);

  VkFramebuffer _framebuffer = VK_NULL_HANDLE;
  std::vector<std::shared_ptr<FramebufferAttachment>> _attachments;
  std::shared_ptr<RenderFrame> _frame;  //storing this is safe since we destroy the FBO when a new RenderFrame is made.
  std::shared_ptr<PassDescription> _passDescription;
  VkRenderPass _renderPass = VK_NULL_HANDLE;
  bool _bValid = true;
};
/**
 * @class Pipeline
 * @brief Essentially, a GL ShaderProgram with VAO state.
 * */
class Pipeline : public VulkanObject {
public:
  Pipeline(std::shared_ptr<Vulkan> v,
           VkPrimitiveTopology topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
           VkPolygonMode mode = VK_POLYGON_MODE_FILL);
  virtual ~Pipeline() override;
  bool init(std::shared_ptr<PipelineShader> shader,
            std::shared_ptr<BR2::VertexFormat> vtxFormat,
            std::shared_ptr<Framebuffer> fbo);

  VkPipeline getVkPipeline() { return _pipeline; }
  VkPipelineLayout getVkPipelineLayout() { return _pipelineLayout; }
  VkPrimitiveTopology primitiveTopology() { return _primitiveTopology; }
  VkPolygonMode polygonMode() { return _polygonMode; }
  std::shared_ptr<BR2::VertexFormat> vertexFormat() { return _vertexFormat; }
  std::shared_ptr<Framebuffer> fbo() { return _fbo; }

private:
  std::shared_ptr<BR2::VertexFormat> _vertexFormat = nullptr;
  VkPrimitiveTopology _primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkPolygonMode _polygonMode = VK_POLYGON_MODE_FILL;
  VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;
  VkPipeline _pipeline = VK_NULL_HANDLE;
  std::shared_ptr<Framebuffer> _fbo = nullptr;
};
/**
 * @class PipelineShader
 * @brief This is pipeline + shader modules rolled into a single object.
 * */
class PipelineShader : public VulkanObject {
public:
  static std::shared_ptr<PipelineShader> create(std::shared_ptr<Vulkan> v, const string_t& name, const std::vector<string_t>& files);
  PipelineShader(std::shared_ptr<Vulkan> v, const string_t& name, const std::vector<string_t>& files);
  virtual ~PipelineShader() override;

  const string_t& name() { return _name; }
  bool valid() { return _bValid; }
  bool shaderError(const string_t& msg);
  bool renderError(const string_t& msg);
  const std::vector<std::shared_ptr<ShaderOutputBinding>>& outputBindings() { return _outputBindings; }

  VkDescriptorSetLayout getVkDescriptorSetLayout() { return _descriptorSetLayout; }
  std::vector<VkPipelineShaderStageCreateInfo> getShaderStageCreateInfos();
  VkPipelineVertexInputStateCreateInfo getVertexInputInfo(std::shared_ptr<BR2::VertexFormat> fmt);

  std::shared_ptr<Pipeline> getPipeline(std::shared_ptr<BR2::VertexFormat> vertexFormat, VkPrimitiveTopology topo, VkPolygonMode mode);
  std::shared_ptr<VulkanBuffer> getUBO(const string_t& name, std::shared_ptr<RenderFrame> frame);
  bool createUBO(const string_t& name, const string_t& var_name, unsigned long long bufsize = VK_WHOLE_SIZE);
  std::shared_ptr<Pipeline> boundPipeline() { return _pBoundPipeline; }
  void clearShaderDataCache(std::shared_ptr<RenderFrame> frame);

  std::shared_ptr<PassDescription> getPass(std::shared_ptr<RenderFrame> frame);
  bool beginRenderPass(std::shared_ptr<CommandBuffer> buf, std::shared_ptr<RenderFrame> frame, std::shared_ptr<PassDescription> desc, BR2::urect2* extent = nullptr);
  void endRenderPass(std::shared_ptr<CommandBuffer> buf);
  bool bindUBO(const string_t& name, std::shared_ptr<VulkanBuffer> buf, VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE);  //buf =  Optionally, update.
  bool bindSampler(const string_t& name, std::shared_ptr<VulkanTextureImage> texture, uint32_t arrayIndex = 0);
  bool bindPipeline(std::shared_ptr<CommandBuffer> cmd, std::shared_ptr<BR2::VertexFormat> v_fmt, VkPolygonMode mode = VK_POLYGON_MODE_FILL, VkPrimitiveTopology topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  bool bindPipeline(std::shared_ptr<CommandBuffer> cmd, std::shared_ptr<Pipeline> pipe);
  void bindViewport(std::shared_ptr<CommandBuffer> cmd, const BR2::urect2& size);
  bool bindDescriptors(std::shared_ptr<CommandBuffer> cmd);
  void drawIndexed(std::shared_ptr<CommandBuffer> cmd, std::shared_ptr<Mesh> m, uint32_t numInstances);

private:
  bool init();
  bool checkGood();
  bool createInputs();
  bool createOutputs();
  bool createDescriptors();
  //bool createRenderPass();
  void cleanupDescriptors();
  std::shared_ptr<Framebuffer> getOrCreateFramebuffer(std::shared_ptr<RenderFrame> frame, std::shared_ptr<ShaderData> data, std::shared_ptr<PassDescription> desc);
  std::shared_ptr<Framebuffer> findFramebuffer(std::shared_ptr<ShaderData> data, std::shared_ptr<PassDescription> desc);
  std::shared_ptr<ShaderData> getShaderData(std::shared_ptr<RenderFrame> frame);
  OutputMRT parseShaderOutputTag(const string_t& tag);
  DescriptorFunction classifyDescriptor(const string_t& name);
  BR2::VertexUserType parseUserType(const string_t& err);
  std::shared_ptr<ShaderModule> getModule(VkShaderStageFlagBits stage, bool throwIfNotFound = false);
  std::shared_ptr<Descriptor> getDescriptor(const string_t& name);
  VkFormat spvReflectFormatToVulkanFormat(SpvReflectFormat fmt);

  string_t _name = "*undefined*";
  std::vector<string_t> _files;
  VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;
  VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> _descriptorSets;  // TODO: put these on ShaderData (one per frame)
  std::vector<VkVertexInputAttributeDescription> _attribDescriptions;
  VkVertexInputBindingDescription _bindingDesc;
  std::vector<std::shared_ptr<ShaderModule>> _modules;
  std::unordered_map<string_t, std::shared_ptr<Descriptor>> _descriptors;
  std::vector<std::shared_ptr<VertexAttribute>> _attributes;
  std::vector<std::shared_ptr<ShaderOutputBinding>> _outputBindings;
  std::shared_ptr<Framebuffer> _pBoundFBO = nullptr;
  std::shared_ptr<Pipeline> _pBoundPipeline = nullptr;
  std::shared_ptr<ShaderData> _pBoundData = nullptr;
  std::shared_ptr<RenderFrame> _pBoundFrame = nullptr;

  bool _bInstanced = false;  //True if we find gl_InstanceIndex (gl_instanceID) in the shader - and we will bind vertexes per instance.
  bool _bValid = true;
  //std::vector<std::shared_ptr<ShaderData>> _shaderData;
  std::map<uint32_t, std::shared_ptr<ShaderData>> _shaderData;
};
/**
 * @class ShaderData
 * @brief Swapchain Frame (async) shader data.
 * */
class ShaderData {
public:
  std::unordered_map<std::string, std::shared_ptr<ShaderDataUBO>> _uniformBuffers;
  std::shared_ptr<ShaderDataUBO> getUBOData(const string_t& name);
  std::vector<std::shared_ptr<Framebuffer>> _framebuffers;  //In the future we can optimize this search.
  std::vector<std::shared_ptr<Pipeline>> _pipelines;        // All pipelines bound to this data.
};
/**
 * @class RenderFrame
 *  RenderPass is Shadred among FBOS and shaders
 */
class RenderFrame : public VulkanObject {
public:
  RenderFrame(std::shared_ptr<Vulkan> v, std::shared_ptr<Swapchain> ps, uint32_t frameIndex, VkImage img, VkSurfaceFormatKHR fmt);
  virtual ~RenderFrame() override;

  const BR2::usize2& imageSize();
  std::shared_ptr<Swapchain> getSwapchain() { return _pSwapchain; }
  //VkImageView getVkImageView() { return _swapImageView; }
  std::shared_ptr<CommandBuffer> commandBuffer() { return _pCommandBuffer; }     //Possible to have multiple buffers as vkQUeueSubmit allows for multiple. Need?
  uint32_t currentRenderingImageIndex() { return _currentRenderingImageIndex; }  //TODO: remove later
  uint32_t frameIndex() { return _frameIndex; }                                  //Image index in the swapchain array

  void init();
  bool beginFrame();
  void endFrame();

  VkImage swapImage() { return _swapImage; }
  VkFormat imageFormat();  //rename swapFormat
  VkImage depthImage() { return _depthImage->image(); }
  VkFormat depthFormat() { return _depthImage->format(); }

private:
  void createSyncObjects();

  FrameState _frameState = FrameState::Unset;

  std::shared_ptr<Swapchain> _pSwapchain = nullptr;
  std::shared_ptr<CommandBuffer> _pCommandBuffer = nullptr;

  uint32_t _frameIndex = 0;

  //* Default FBO
  VkImage _swapImage = VK_NULL_HANDLE;
  VkSurfaceFormatKHR _imageFormat{ .format = VK_FORMAT_UNDEFINED, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
  std::shared_ptr<VulkanImage> _depthImage = nullptr;

  VkFence _inFlightFence = VK_NULL_HANDLE;
  VkFence _imageInFlightFence = VK_NULL_HANDLE;
  VkSemaphore _imageAvailableSemaphore = VK_NULL_HANDLE;
  VkSemaphore _renderFinishedSemaphore = VK_NULL_HANDLE;
  uint32_t _currentRenderingImageIndex = 0;
};
/**
 * @class Swapchain
 * */
class Swapchain : public VulkanObject {
public:
  Swapchain(std::shared_ptr<Vulkan> v);  //TODO: bool vsync -> use FIFO swapchain mode.
  virtual ~Swapchain() override;

  std::shared_ptr<RenderFrame> acquireFrame();
  bool beginFrame(const BR2::usize2& windowsize);
  void endFrame();
  const BR2::usize2& imageSize();
  VkFormat imageFormat();
  void outOfDate() { _bSwapChainOutOfDate = true; }
  bool isOutOfDate() { return _bSwapChainOutOfDate; }
  void waitImage(uint32_t imageIndex, VkFence myFence);
  void initSwapchain(const BR2::usize2& window_size);
  VkSwapchainKHR getVkSwapchain() { return _swapChain; }
  const std::vector<std::shared_ptr<RenderFrame>>& frames() { return _frames; }
  void registerShader(std::shared_ptr<PipelineShader> shader);

private:
  void createSwapChain(const BR2::usize2& window_size);
  void cleanupSwapChain();
  bool findValidSurfaceFormat(std::vector<VkFormat> fmts, VkSurfaceFormatKHR& fmt_out);
  bool findValidPresentMode(VkPresentModeKHR& pm_out);

  FrameState _frameState = FrameState::Unset;
  std::unordered_set<std::shared_ptr<PipelineShader>> _shaders;
  std::vector<std::shared_ptr<RenderFrame>> _frames;
  size_t _currentFrame = 0;
  std::vector<VkFence> _imagesInFlight;  //Shared handle do not delete
  VkSwapchainKHR _swapChain = VK_NULL_HANDLE;
  BR2::usize2 _imageSize{ 0, 0 };
  bool _bSwapChainOutOfDate = false;
};

}  // namespace VG

#endif
