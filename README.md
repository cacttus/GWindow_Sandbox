# GWindow_Sandbox
Sandbox for testing the [VulkanGame](https://github.com/metalmario971/VulkanGame) window system.

# TODO
* Move pipeline stuff to pipeline.
* Move Descriptors into Shader & Pipeline 

* Separate UBO / Image into ShaderTexture ShaderUniform Classes

* Mipmap testing is broken right now because the texture isn't recreated when we update the swapchain.
  * Since the imageview and sampler depends on mipmap information, its best to reallocate the texture image (for now).
  * We want something like
    * _texture = std::make_shared<VulkanTextureImage>
    * _pipeline->bindTexture(_texture)
