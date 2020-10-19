# GWindow_Sandbox
Sandbox for testing the [VulkanGame](https://github.com/metalmario971/VulkanGame) window system.

# TODO
* Saved the class files for future reference (_saved)

* Move Geom Vertex info and Descriptors to Shaders.
* Shader -> descriptor creation.
* Descriptor writes within shader.

* Mesh Class
* Swapchain
* Multiple Vulkan Windows.
* Move pipeline stuff

* Move Descriptors into Shader & Pipeline 

* Separate UBO / Image into ShaderTexture ShaderUniform Classes

* Mipmap testing is broken right now because the texture isn't recreated when we update the swapchain.
  * Since the imageview and sampler depends on mipmap information, its best to reallocate the texture image (for now).
  * We want something like
    * _texture = std::make_shared<VulkanTextureImage>
    * _pipeline->bindTexture(_texture)



(x) Kronecker
(+) Direct sum = lined up, zeroed new dimensions
A = { 0 1 } B = {2 3 4}
A (+) B = 0 1 2 3 4
Det(a (+) b ) = Det(a)*Det(b)
Tr(a (+) b) = Tr(a)+Tr(b)
Tensor Product = Kronecker product
Particles move as waves, not linearly. (Matter-waves)
