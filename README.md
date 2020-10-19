# GWindow_Sandbox
Sandbox for testing the [VulkanGame](https://github.com/metalmario971/VulkanGame) window system.

# TODO
* Doing this whole pipeline thing at once is too much.
* Saved the class files for future reference (_saved)
  * First: we must move Geom Vertex info and Descriptors to Shaders.
  * Next test out swapping texture images.
  
* Mesh Class
* Swapchain
* Multiple Vulkan Windows.
* Move pipeline stuff
  * We really should create a RenderFrame class. Even though it is less efficient. All this for looping is hella messy.

  * What I don't understand:
    * We are referencing the ImageView in the Pipeline framebuffer, yet the pipeline is disjoint from the ImageView as we dispatch it to work on different images.

* Move Descriptors into Shader & Pipeline 

* Separate UBO / Image into ShaderTexture ShaderUniform Classes

* Mipmap testing is broken right now because the texture isn't recreated when we update the swapchain.
  * Since the imageview and sampler depends on mipmap information, its best to reallocate the texture image (for now).
  * We want something like
    * _texture = std::make_shared<VulkanTextureImage>
    * _pipeline->bindTexture(_texture)

