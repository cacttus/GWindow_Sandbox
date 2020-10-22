
Problem with resize is that the RenderFrame classes are destroyed - shaders aren't saved when we rebind them. Save on Swapchain instead.

# TODO
* Note: Saved the class files for future reference (_saved)

* Change Drawing from Waiting -> Passive waiting 
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

* Move ViewProjUBOData buffer to FrameData (somenhow) and make it auto generated based on ubo_instance string.
  * Move InstanceUBOData the same way.

* Move Geom Vertex info to Shaders.
  * Integrate BR2::VertexFormat
    * Match Mesh VertexFormat with Shader VertexFormat
  * Create Pipeline on Shader
  * Lightweight RenderFrame classes (refactoring)
    * std::vector<RenderFrame>
    * Not a HUGE overhaul. Starting small. 
      * We will abstract Pipeline and Swapchain later.
    * Move shader data into a RenderFrame.
      * Uniform buffers (de-array)
      * Descriptor sets.
      * Framebuffers (de-array)
    * Access bindings & UBO's via RenderFrame->getShaderData(_pShader)  

    * Move command buffer creation into RenderFrame.

* Make pipeline a subclass of Shader  this would organize better when we integrate with VG

* Swapchain
* Multiple Vulkan Windows.
* Move pipeline stuff
* Separate UBO / Image into ShaderTexture ShaderUniform Classes

* MRT's  (Test)
  * Pick / Shadows

## After Integrating with VG
* User Interface

## Backlog
* Buffer pooling  https://gpuopen.com/learn/vulkan-device-memory/
    * Host memory heap. 
    * Mappable Gpu Memory Heap
    * Unmappable GPU memory heap
* Instance / Animated meshes
* Component model.
* Shader Skinning

* Mipmap testing is broken right now because the texture isn't recreated when we update the swapchain.
  * Since the imageview and sampler depends on mipmap information, its best to reallocate the texture image (for now).
  * We want something like
    * _texture = std::make_shared<VulkanTextureImage>
    * _pipeline->bindTexture(_texture)

* Replace parseUserType with the code from VG



(x) Kronecker
(+) Direct sum = lined up, zeroed new dimensions
A = { 0 1 } B = {2 3 4}
A (+) B = 0 1 2 3 4
Det(a (+) b ) = Det(a)*Det(b)
Tr(a (+) b) = Tr(a)+Tr(b)
Tensor Product = Kronecker product
Particles move as waves, not linearly. (Matter-waves)
