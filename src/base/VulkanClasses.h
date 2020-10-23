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
enum class DescriptorFunction {
  Unset,
  Custom,
  ViewProjMatrixUBO,
  InstnaceMatrixUBO,
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
  DescriptorFunction _function = DescriptorFunction::Unset;
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
enum class BlendFunc {
  Disabled,
  AlphaBlend
};
enum class FBOType {
  Color,
  Depth
};

/**
 * @class ShaderOutput
 * @brief Intermediary FBO metadata.
 * */
class ShaderOutput {
public:
  uint32_t _location = 0;
  string_t _name = "";
  VkFormat _format;  // This is incorrect from spv-reflect. Don't use.
  BlendFunc _blending = BlendFunc::AlphaBlend;
  FBOType _type = FBOType::Color;
  BR2::vec4 _clearColor{ 0, 0, 0, 1 };  //R&G are Depth&Stencil(casted to integer for stencil)
};
/**
 * @class FramebufferAttachment
 * */
class FramebufferAttachment : public VulkanObject {
public:
  FramebufferAttachment(std::shared_ptr<Vulkan> v, FBOType type, const string_t& name, VkFormat fbo_fmt, uint32_t glsl_location,
                        const BR2::usize2& imageSize, VkImage swap_img, VkFormat swap_format, const BR2::vec4& clearColor, BlendFunc blending);
  virtual ~FramebufferAttachment() override;

  bool init();
  VkImageView getVkImageView() { return _swapchainImageView; }
  const string_t& name() { return _name; }
  uint32_t location() { return _location; }
  //VkFormat fboFormat() { return _fboFormat; }
  BlendFunc blendFunc() { return _blending; }
  FBOType type() { return _fboType; }
  const BR2::vec4& clearColor() { return _clearColor; }  // If this is a depth FBO x = clear color depth
  const BR2::usize2& imageSize() { return _imageSize; }

private:
  string_t _name = "";
  uint32_t _location = 0;
  BR2::vec4 _clearColor = BR2::vec4(0, 0, 0, 1);
  BlendFunc _blending = BlendFunc::Disabled;
  VkFormat _fboFormat = VK_FORMAT_UNDEFINED;
  VkImage _swapchainImage = VK_NULL_HANDLE;
  VkFormat _swapchainImageFormat = VK_FORMAT_UNDEFINED;
  VkImageView _swapchainImageView = VK_NULL_HANDLE;
  VkImage _image = VK_NULL_HANDLE;
  FBOType _fboType = FBOType::Color;
  BR2::usize2 _imageSize{ 0, 0 };
};
/**
 * @class Framebuffer
 * @brief Shader output. Tied to a shader.
 * */
class Framebuffer : public VulkanObject {
public:
  Framebuffer(std::shared_ptr<Vulkan> v, std::shared_ptr<PipelineShader> s, const BR2::usize2& size);
  virtual ~Framebuffer() override;

  void addAttachment(std::shared_ptr<FramebufferAttachment> at);
  bool createFBO();
  VkFramebuffer getVkFramebuffer() { return _framebuffer; }
  BR2::vec2 _imageSize = BR2::vec2(0, 0);
  std::vector<std::shared_ptr<FramebufferAttachment>> attachments() { return _attachments; }
  const BR2::usize2& size() { return _size; }

private:
  VkFramebuffer _framebuffer = VK_NULL_HANDLE;
  std::shared_ptr<PipelineShader> _pShader = nullptr;
  std::vector<std::shared_ptr<FramebufferAttachment>> _attachments;
  BR2::usize2 _size{ 0, 0 };
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
  const std::vector<ShaderOutput>& outputFBOs() { return _shaderOutputs; }
  VkRenderPass renderPass() { return _renderPass; }

  VkDescriptorSetLayout getVkDescriptorSetLayout() { return _descriptorSetLayout; }
  std::vector<VkPipelineShaderStageCreateInfo> getShaderStageCreateInfos();
  bool bindUBO(const string_t& name, uint32_t swapchainImageIndex, std::shared_ptr<VulkanBuffer> buf, VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE);  //buf =  Optionally, update.
  bool bindSampler(const string_t& name, uint32_t swapchainImageIndex, std::shared_ptr<VulkanTextureImage> texture, uint32_t arrayIndex = 0);
  VkPipelineVertexInputStateCreateInfo getVertexInputInfo(std::shared_ptr<BR2::VertexFormat> fmt);
  bool beginRenderPass(std::shared_ptr<CommandBuffer> buf, std::shared_ptr<RenderFrame> frame, BR2::urect2* extent = nullptr);
  std::shared_ptr<Pipeline> getPipeline(VkPrimitiveTopology topo, VkPolygonMode mode);
  void bindDescriptors(std::shared_ptr<CommandBuffer> cmd, std::shared_ptr<Pipeline> pipe, uint32_t swapchainImageIndex);
  bool recreateShaderDataFBO(uint32_t frameIndex, VkImage swap_image, VkFormat swap_format, const BR2::usize2& swap_siz);
  std::shared_ptr<VulkanBuffer> getUBO(const string_t& name, std::shared_ptr<RenderFrame> frame);
  
  bool createUBO(const string_t& name, const string_t& var_name, unsigned long long bufsize = VK_WHOLE_SIZE);
  std::shared_ptr<ShaderData> getShaderData(std::shared_ptr<RenderFrame> frame);
  
private:
  bool init();
  bool checkGood();
  bool createInputs();
  bool createOutputs();
  bool createDescriptors();
  bool createRenderPass();
  void cleanupDescriptors();
  DescriptorFunction classifyDescriptor(const string_t& name);
  BR2::VertexUserType parseUserType(const string_t& err);
  std::shared_ptr<ShaderModule> getModule(VkShaderStageFlagBits stage, bool throwIfNotFound = false);
  std::shared_ptr<Descriptor> getDescriptor(const string_t& name);
  VkFormat spvReflectFormatToVulkanFormat(SpvReflectFormat fmt);

  string_t _name = "*undefined*";
  std::vector<string_t> _files;
  VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;
  VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> _descriptorSets;
  VkRenderPass _renderPass = VK_NULL_HANDLE;
  std::vector<VkVertexInputAttributeDescription> _attribDescriptions;
  VkVertexInputBindingDescription _bindingDesc;
  std::vector<std::shared_ptr<ShaderModule>> _modules;
  std::unordered_map<string_t, std::shared_ptr<Descriptor>> _descriptors;
  std::vector<std::shared_ptr<VertexAttribute>> _attributes;
  std::vector<ShaderOutput> _shaderOutputs;
  bool _bInstanced = false;  //True if we find gl_InstanceIndex (gl_instanceID) in the shader - and we will bind vertexes per instance.
  std::vector<std::shared_ptr<Pipeline>> _pipelines;
  bool _bValid = true;
  std::vector<std::shared_ptr<ShaderData>> _shaderData;
};
/**
 * @class Pipeline
 * Essentially, a GL ShaderProgram with VAO state.
 * */
class Pipeline : public VulkanObject {
public:
  Pipeline(std::shared_ptr<Vulkan> v,
           std::shared_ptr<PipelineShader> shader,
           std::shared_ptr<BR2::VertexFormat> vtxFormat,
           VkPrimitiveTopology topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
           VkPolygonMode mode = VK_POLYGON_MODE_FILL);
  virtual ~Pipeline() override;

  void bind(std::shared_ptr<CommandBuffer> cmd);
  void drawIndexed(std::shared_ptr<CommandBuffer> cmd, std::shared_ptr<Mesh> m, uint32_t numInstances);

  VkPipeline getVkPipeline() { return _pipeline; }
  VkPipelineLayout getVkPipelineLayout() { return _pipelineLayout; }

private:
  std::shared_ptr<BR2::VertexFormat> _vertexFormat = nullptr;
  VkPrimitiveTopology _primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkPolygonMode _polygonMode = VK_POLYGON_MODE_FILL;
  VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;
  VkPipeline _pipeline = VK_NULL_HANDLE;
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
  bool beginPass(std::shared_ptr<PipelineShader> shader, std::shared_ptr<RenderFrame> frame, BR2::urect2* extent = nullptr);
  void endPass();

private:
  void validateState(bool b);
  CommandBufferState _state = CommandBufferState::Unset;
  std::shared_ptr<RenderFrame> _pRenderFrame = nullptr;
  VkCommandPool _sharedPool = VK_NULL_HANDLE;       //Do not free
  VkCommandBuffer _commandBuffer = VK_NULL_HANDLE;  //_commandBuffers;
};
// struct ViewProjUBOData {
//   BR2::mat4 matrix;
// };
struct InstanceUBOClassData {
  uint32_t _maxInstances = 1; //The maximum instances specified in the UBO
  uint32_t _curInstances = 1; //Current number of instnaces in the scene.
};
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
  std::shared_ptr<Descriptor> _descriptor=nullptr;  //shared ptr to the descriptor in the shader.
  std::shared_ptr<VulkanBuffer> _buffer=nullptr;
  UBOClassData _data;
};
/**
 * @class ShaderData
 * @brief Swapchain Frame (async) shader data.
 * */
class ShaderData {
public:
  std::shared_ptr<Framebuffer> _framebuffer = nullptr;
  std::unordered_map<std::string, std::shared_ptr<ShaderDataUBO>> _uniformBuffers;
  std::shared_ptr<ShaderDataUBO> getUBOData(const string_t& name);
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
  VkFormat imageFormat();
  std::shared_ptr<Swapchain> getSwapchain() { return _pSwapchain; }
  VkImageView getVkImageView() { return _imageView; }
  std::shared_ptr<CommandBuffer> commandBuffer() { return _pCommandBuffer; }     //Possible to have multiple buffers as vkQUeueSubmit allows for multiple. Need?
  uint32_t currentRenderingImageIndex() { return _currentRenderingImageIndex; }  //TODO: remove later
  uint32_t frameIndex() { return _frameIndex; }                                  //Image index in the swapchain array

  void init();
  void beginFrame();
  void endFrame();
  void registerShader(std::shared_ptr<PipelineShader> shader);
  bool rebindShader(std::shared_ptr<PipelineShader> shader);

private:
  void createSyncObjects();
  void cleanupSyncObjects();

  std::shared_ptr<Swapchain> _pSwapchain = nullptr;
  std::shared_ptr<CommandBuffer> _pCommandBuffer = nullptr;

  uint32_t _frameIndex = 0;

  //* Default FBO
  VkImage _image = VK_NULL_HANDLE;
  VkImageView _imageView = VK_NULL_HANDLE;
  VkSurfaceFormatKHR _imageFormat{ .format = VK_FORMAT_UNDEFINED, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };

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
  Swapchain(std::shared_ptr<Vulkan> v);//TODO: bool vsync -> use FIFO swapchain mode.
  virtual ~Swapchain() override;

  //Get the next available frame if one is available. Non-Blocking
  std::shared_ptr<RenderFrame> acquireFrame();

  void beginFrame(const BR2::usize2& windowsize);  //Remove
  void endFrame();    //Remove

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
  void rebindShaders();

  std::unordered_set<std::shared_ptr<PipelineShader>> _shaders;
  //std::map<std::shared_ptr<PipelineShader>, std::vector<std::shared_ptr<ShaderData>>> _shaderData;
  std::vector<std::shared_ptr<RenderFrame>> _frames;
  size_t _currentFrame = 0;
  std::vector<VkFence> _imagesInFlight;  //Shared handle do not delete
  VkSwapchainKHR _swapChain = VK_NULL_HANDLE;
  BR2::usize2 _imageSize{ 0, 0 };
  bool _bSwapChainOutOfDate = false;
};

}  // namespace VG

#endif
