# GWindow_Sandbox
Sandbox for testing the [VulkanGame](https://github.com/metalmario971/VulkanGame) window system.

# TODO
* Note: Saved the class files for future reference (_saved)

* Move Geom Vertex info  to Shaders.

**Test VK_VERTEX_INPUT_RATE_INSTANCE in the attribute binding description
  * not sure what this does - instancing appears to work without it.
** Test out attribute input binding order in SpvReflect

* RenderFrame Class
    std::vector<RenderFrame>
  * Not a HUGE overhaul. Starting small. 
    * We will abstract Pipeline and Swapchain later.
  * Move shader data into a RenderFrame.
    * Uniform buffers (de-array)
    * Descriptor sets.
    * Framebuffers (de-array)
  Access this stuff via Frame->getShaderData(_pShader)

  * Move command buffer creation into RenderFrame.

* TBH - we could make pipeline a subclass of Shader  this would organize better when we integrate with VG


* Problem - shader vertex information isn't the way to go - 
because input vertexes can be of any format.
How would we bind a slective vertex buffer to the shader?
  * just colors & v's ??

* Mesh Class
* Swapchain
* Multiple Vulkan Windows.
* Move pipeline stuff

* Move Descriptors into Shader & Pipeline 

* Separate UBO / Image into ShaderTexture ShaderUniform Classes

## Backlog

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
