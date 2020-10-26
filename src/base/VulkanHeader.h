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

//#include "../base/BaseHeader.h"

namespace VG {

/////////////////////////////////////////////////////////////////////////////////
//Macros
//Validate a Vk Result.
#define CheckVKR(fname_, ...)                     \
  do {                                            \
    VkResult res__ = (fname_(__VA_ARGS__));       \
    vulkan()->validateVkResult(res__, (#fname_)); \
  } while (0)

//Find Vk Extension
#define VkExtFn(_vkFn) PFN_##_vkFn _vkFn = nullptr;

/////////////////////////////////////////////////////////////////////////////////
//Enums
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
  Gpu, GpuAndHost, Host
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
  EndPass
};
enum class DescriptorFunction {
  Unset,
  Custom,
  ViewProjMatrixUBO,
  InstnaceMatrixUBO,
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
//"_outFBO_ ..
enum class OutputMRT {
  RT_Undefined,
  RT_DefaultColor,  //Default FBO
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
  MaxOutputs,
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
/////////////////////////////////////////////////////////////////////////////////
//FWD

//Vulakn
class Vulkan;
class VulkanObject;
class VulkanDeviceBuffer;
class VulkanBuffer;
class VulkanImage;
class VulkanCommands;
class VulkanTextureImage;
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
  alignas(16) BR2::mat4 view;
  alignas(16) BR2::mat4 proj;
};
struct InstanceUBOData {
alignas(16) BR2::mat4 model;
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
    Sparse buffer binding
      - Buffer is physically allocated in multiple separate chunks.
      - This may be good for allocating buffer pools as pools are pretty much a requirement.
    Descriptor sets vs descriptor pools.
    *Indirect functions - Parameters that would be passed are read by a buffer (UBO) during execution.
    Tessellation is performed via subdivision of concentric triangles of the input triangles.
    oversampling / undersampling = bilinear filtering / anisotropic filtering
  Command Buffers vs Command pools
    command pools batch commands into differnet types like compute vs graphics vs presentation.
    command buffers exist in command pools and they package a series of dependent rendering commands that cannot be run async (begin, bind buffers, bind uniforms, bind textures, draw.. end)

*/

#endif
