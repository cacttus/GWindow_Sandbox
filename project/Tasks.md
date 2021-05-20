TODO: get validation layers to work. No debug info is present.
TODO: pass->cullMode(CullMode::Front)
      pass->polygonMode(PolygonMode::Fill)
      pass->setAlpha(BlendState::AlphaBlend)
TODO: The OutputMRT is a problem because it specifies BOTH the shader's bind point AND a type of texture image. Fix this.
TODO: Bugs: RTT is broken
      Bugs: Multipass rendering is broken
TODO: sample rate shading.
  VkPhysicalDeviceFeatures sampleRateShading - tells us if it's enabled.

## Roadmap
1. continue implementing supported BR2 pipeline features
  * MRT's 
    * Picking
  * Shadowmapping
  * Deferred Lighting.
  * Vertex Formats, and Fill/Mode data (in getPipeline)
    * Integrate BR2::VertexFormat
    * getVertexInputInfo
2. Implement the GWindow_Sandbox UI
  * Multiple Vulkan Windows.
3. Move GWindow_Sandbox Vulkan code to VG
    * Alternatively - move VG to GWindow Sandbox
    * Isolate the GL classes.

## Bugs 
* Address concern of Shader output format not matching FBO format
* Multiple FBOs (deferred MRTs)
* UBO Copy for instance data is very slow. Figure out how to optimize this.

## Wishlist (Backlog)
* Instanced meshes as a system.
    * __Instance Dispatch__ - Updating JUST 2000 instances ubo causes severe lag (2400fps -> 80fps in debug, 2400fps -> 600fps RT) due to matrix multiplication (It's not the UBO copy I tested it)
    * Dispatch instance updates to the GPU. This may not be necessary really but it's fun to think about.
        * Fun Note the GPU is so freaking fast that I have yet to make a swapchain semaphore wait drawing thousands of instances...
* Separate UBO / Image into ShaderTexture ShaderUniform Classes
* PBR (testing)
* Auto UBO creation
* UBO Pooling. Texture Pooling.  https://gpuopen.com/learn/vulkan-device-memory/
    * Host memory heap. 
    * Mappable Gpu Memory Heap
    * Unmappable GPU memory heap
* Uniforms to Shaderdata
* Render Loop Optimization
  * Change Drawing Semaphores from Waiting -> Passive waiting 
    * Right now we have:
      * Swapchain->beginFrame
      * Swapchain->endFrame
      * This blocks the game from updating when we don't have images to draw.
    * Switch this to 
      double t_begin = now()
      frame = Swapchain-> acquireFrame
      if(frame){
        frame->beginFrame()
        frame->endFrame()
      }
      double t_elapsed = now() - t_begin
      double timestep = 1000.0 / 60 // 13s
      while((t_elapsed-timestep)>0){
        updategame(timestep)
        t_elapsed -= timestep
      }
* Shader Skinning
* Component model (replace old Spec/Instance system)
* Mipmap testing is broken right now because the texture isn't recreated when we update the swapchain.
  * Since the imageview and sampler depends on mipmap information, its best to reallocate the texture image (for now).
  * We want something like
    * _texture = std::make_shared<VulkanTextureImage>
    * _pipeline->bindTexture(_texture)
* Replace parseUserType with the code from VG
* Stereoscopic rendering (VR) - swapChainCreateInfo->imageArrayLayers
