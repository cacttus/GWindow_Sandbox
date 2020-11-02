/**
*  @file VulkanHeader.h
*  @date 09/15/2020
*  @author MetalMario971
*/
#pragma once
#ifndef __VULKANHEADER_1602796034935462942505497138_H__
#define __VULKANHEADER_1602796034935462942505497138_H__

//We remove this when we plug this back into VG
#include "./SandboxHeader.h"

namespace VG {

/////////////////////////////////////////////////////////////////////////////////
//Macros
#define CheckVKR(fname_, ...)                     \
  do {                                            \
    VkResult res__ = (fname_(__VA_ARGS__));       \
    vulkan()->validateVkResult(res__, (#fname_)); \
  } while (0)
#define CheckVKRV(fname_, ...)              \
  do {                                      \
    VkResult res__ = (fname_(__VA_ARGS__)); \
    validateVkResult(res__, (#fname_));     \
  } while (0)
#define VkLoadExt(_i, _v)                          \
  do {                                             \
    _v = (PFN_##_v)vkGetInstanceProcAddr(_i, #_v); \
  } while (0)
//Find Vk Extension
#define VkExtFn(_vkFn) PFN_##_vkFn _vkFn = nullptr;

/////////////////////////////////////////////////////////////////////////////////
//Enums
//Texture filter mode.
enum class TexFilter {
  Nearest,
  Linear,
  Cubic,
  Filter_Count,
};
enum class MipmapMode {
  Disabled,
  Nearest,
  Linear,
  MipmapMode_Count
};
enum class AttachmentType {
  ColorAttachment,
  DepthAttachment
};
enum class VulkanBufferPoolType {
  Gpu,
  GpuAndHost,
  Host
};
enum class VulkanBufferType {
  VertexBuffer,
  IndexBuffer,
  UniformBuffer,
  ImageBuffer
};
enum class RenderMode {
  TriangleList
};
enum class IndexType {
  IndexTypeUint16,
  IndexTypeUint32
};
enum class CommandBufferState {
  Unset,
  Begin,
  End,
  BeginPass,
  EndPass,
  Submit
};
enum class DescriptorFunction {
  Unset,
  Custom,
  ViewProjMatrixUBO,
  InstnaceMatrixUBO,
  LightsUBO
};
enum class FramebufferBlendMode {
  Global, Independent
};
enum class BlendFunc {
  Disabled,
  AlphaBlend
};
enum class FBOType {
  Undefined,
  Color,
  Depth
};
enum class OutputMRT {
  RT_Undefined,
  RT_DefaultColor,
  RT_DefaultDepth,
  RT_DF_Position,
  RT_DF_Color,
  RT_DF_Depth_Plane,
  RT_DF_Normal,
  RT_DF_Pick,
  RT_Custom0,
  RT_Custom1,
  RT_Custom2,
  RT_Custom3,
  RT_Custom4,
  RT_Custom5,
  RT_Custom6,
  RT_Custom7,
  RT_Custom8,
  RT_Custom9,
  RT_Enum_Count,
};
enum class CompareOp {
  Never,
  Less,
  Equal,
  Less_Or_Equal,
  Greater,
  Not_Equal,
  Greater_or_Equal,
  CompareAlways
};
enum class FrameState {
  Unset,
  FrameBegin,
  FrameEnd,
};
enum class MSAA {
  //This enum converts to VkSampleCountFlagBits
  Unset,
  Disabled,  // 1
  MS_2_Samples,
  MS_4_Samples,
  MS_8_Samples,
  MS_16_Samples,
  MS_32_Samples,
  MS_64_Samples,
  MS_Enum_Count,
};
struct MipLevels {
  static const uint32_t Unset = 0;
};
enum class TextureType {
  Unset,
  ColorTexture,
  DepthAttachment,
  ColorAttachment,
  SwapchainImage
};
//X.h defines None
#ifdef None
#undef None
#endif
enum SamplerType {
  None,
  Sampled
};
/////////////////////////////////////////////////////////////////////////////////
//FWD

//Vulakn
class Vulkan;
class VulkanObject;
class VulkanDebug;
class VulkanDeviceBuffer;
class VulkanBuffer;
class Sampler;
class Texture2D;
class VulkanCommands;
class ShaderModule;
class Descriptor;
class ShaderOutputBinding;
class OutputDescription;
class ShaderOutputArray;
class ShaderOutputCache;
class FramebufferAttachment;
class Framebuffer;
class PipelineShader;
class Pipeline;
class CommandBuffer;
class InstanceUBOClassData;
class UBOClassData;
class ShaderDataUBO;
class ShaderData;
class RenderFrame;
class Swapchain;
class RenderTexture;
class RenderTarget;
class PassDescription;
class Extensions;

//Dummies
class Mesh;
class MeshComponent;
class MaterialDummy;
class GameDummy;

/////////////////////////////////////////////////////////////////////////////////
//Typedefs
/////////////////////////////////////////////////////////////////////////////////
//Classes

struct ViewProjUBOData { 
  //alignas(16) BR2::vec2 foo;
  //alignas(16) BR2::mat4 view;
  //alignas(16) BR2::mat4 proj;
  BR2::mat4 view;
  BR2::mat4 proj;
  BR2::vec3 camPos;
  float pad;
};
struct InstanceUBOData { 
  alignas(16) BR2::mat4 model;
};
struct GPULight {
  BR2::vec3 pos;
  float radius;
  BR2::vec3 color;
  float rotation;
  BR2::vec3 specColor;
  float specIntensity;
  BR2::vec3 pad;
  float specHardness;
};
class InstanceUBOClassData {
  uint32_t _maxInstances = 1;  //The maximum instances specified in the UBO
  uint32_t _curInstances = 1;  //Current number of instnaces in the scene.
};
template <typename Tx>
class SharedObject : public std::enable_shared_from_this<Tx> {
protected:
  template <typename Ty = Tx>
  std::shared_ptr<Ty> getThis() {
    return std::dynamic_pointer_cast<Ty>(this->shared_from_this());
  }
};
class VectorUtils {
public:
  template <typename T_From, typename T_To>
  static std::vector<T_To> convertVector(const std::vector<T_From>& from, std::function<T_To(const T_From&)> func) {
    std::vector<T_To> ret(from.size());
    std::transform(from.begin(), from.end(), ret.begin(), func);
    return ret;
  }

};

}  // namespace VG

/*

  This is following the Vulkan Tutorial https://vulkan-tutorial.com.

  Notes
    Aliasing - using the same chunk of memory for multiple resources.
    Queue family - a queue with a common set of characteristics, and a number of possible queues.
    Logical device - the features of the physical device that we are using.
      Multiple per phsical device.  
    Opaque pointer / opaque handle - handles that are interpreted by the API and don't represent anything the client knows about.
    Handle - abstarct refernce to some underlying implementation 
    Subpass - render pass that depends on the contents of the framebuffers of previous passes.
    ImageView - You can't access images directly you need to use an image view.
    Push Constants - A small bank of values writable via the API and accessible in shaders. Push constants allow the application to set values used in shaders without creating buffers or modifying and binding descriptor sets for each update.
      https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#glossary
    Descriptor - (Resource Descriptor) - This is like a GL Buffer Binding. Tells Pipeline how to access and lay out memory for a SSBO or UBO or Attribute.
    Fences - CPU->GPU synchronization - finish rendering all frames before next loop
    Semaphores -> GPU->GPU sync, allows finish one pipeline stage before continuing.
    Mesh Shaders - An NVidia extension that combines the primitive assembly stages as a single dispatched compute "mesh" stage (multiple threads) and a "task" sage.
      https://www.geeks3d.com/20200519/introduction-to-mesh-shaders-opengl-and-vulkan/
      - Meshlets are small meshes of a big mesh to render.
      - Meshes are decmoposed because each mesh shader is limited on the number of output meshes.
    Nvidia WARP unit
      https://www.geeks3d.com/hacklab/20180705/demo-visualizing-nvidia-gl_threadinwarpnv-gl_warpidnv-and-gl_smidnv-gl_nv_shader_thread_group/
      - it is a set of 32 fragment (pixel) threads.
      - warps are grouped in SM's (streaming multiprocessors)
      - each SM contains 64 warps - 2048 threads.
      - gtx 1080 has 20 SMs
      - rtx 3080 has 68 SMs
        - Each GPU core can run 16 threads
    Recursive Preprocessor
      - Deferred expression
      - Disabling Context
    Sparse getVkBuffer binding
      - Buffer is physically allocated in multiple separate chunks.
      - This may be good for allocating getVkBuffer pools as pools are pretty much a requirement.
    Descriptor sets vs descriptor pools.
    *Indirect functions - Parameters that would be passed are read by a getVkBuffer (UBO) during execution.
    Tessellation is performed via subdivision of concentric triangles of the input triangles.
    oversampling / undersampling = bilinear filtering / anisotropic filtering
  Command Buffers vs Command pools
    command pools batch commands into differnet types like compute vs graphics vs presentation.
    command buffers exist in command pools and they package a series of dependent rendering commands that cannot be run async (begin, bind buffers, bind uniforms, bind textures, draw.. end)

*/

#endif
