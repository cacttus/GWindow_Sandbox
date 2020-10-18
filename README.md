# GWindow_Sandbox
Sandbox for testing the [VulkanGame](https://github.com/metalmario971/VulkanGame) window system.

# TODO
Separate UBO / Image into ShaderTexture ShaderUniform

Mipmap testing is broken right now because the texture isn't recreated when we update the swapchain.
Since the imageview and sampler depends on mipmap information, its best to reallocate the texture image (for now).
We want something like
_texture = std::make_shared<VulkanTextureImage>
_pipeline->bindTexture(_texture)
  ** creates descriptors for pipeline.
  The only time we need to link Texture with Descriptors is when we WRITE the descriptor data (vkUpdateDescriptorSets)

  vkCmdBindDescriptorSets

  Shader
    DescriptorSets (one per swapchain image)
      < Texture->view Texture->sampler
      < UBO->buffer

  class ShaderDescriptor{
    public:
    //We create shader descriptors when we create the shader.
    bool _bIsWritten << Make >>

  }

What I don't understand:
We hard set the shader in createGraphicsPipeline, yet the data we provide to the shader is specified by the descriptor sets and we set those dynamically.
Why? 
1. Different descriptor sets for different textures, etc.
2. Different descriptors for different UBOs.

How I understand it:
Descriptor Layout - static - for the shader itself.
Descriptor set - dynamic - can be changed per draw.

**I have a texture I want to swap it out, how?


