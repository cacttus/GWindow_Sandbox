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
class VulkanObject {
  std::shared_ptr<Vulkan> _vulkan = nullptr;

protected:
  std::shared_ptr<Vulkan> vulkan() { return _vulkan; }

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
  void writeData(void* data, size_t off, size_t datasize) ;
  std::shared_ptr<VulkanDeviceBuffer> hostBuffer();
  std::shared_ptr<VulkanDeviceBuffer> gpuBuffer() ;
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
  void end() ;
  void blitImage(VkImage srcImg,
                 VkImage dstImg,
                 const BR2::iext2& srcRegion,
                 const BR2::iext2& dstRegion,
                 VkImageLayout srcLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 VkImageLayout dstLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 uint32_t srcMipLevel = 0,
                 uint32_t dstMipLevel = 0,
                 VkImageAspectFlagBits aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT, VkFilter filter = VK_FILTER_LINEAR) ;
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
//class VulkanShaderModule
//You can bind multiple pipelines in one command buffer.
class VulkanPipeline {
public:
  //Shader Pipeline.
  //VkPipeline
  //Shaders
};
class VulkanSwapchainImage {
public:
  //Framebuffer
  //Uniform buffer
  //Descriptor sets.

  //VkImage
  //VkImageView
  //VkCommandBuffer commands
  //VkFramebuffer frameBuffers
};
//In-Fligth Frames
//Semaphores.
//Fences
//This is one of the most important classes as it gives us the "extents" of all our images, buffers and viewports.
class VulkanSwapchain : public VulkanObject {
public:
  VkExtent2D _swapChainExtent;
  VkFormat _swapChainImageFormat;
  std::vector<VulkanSwapchainImage> _swapchainImages;

  VulkanSwapchain(std::shared_ptr<Vulkan> v) : VulkanObject(v) {
    //     //void createSwapChain() {
    //     BRLogInfo("Creating Swapchain.");
    //
    //     uint32_t formatCount;
    //     CheckVKR(vkGetPhysicalDeviceSurfaceFormatsKHR, vulkan()->physicalDevice(), v->getWindowSurface(), &formatCount, nullptr);
    //     std::vector<VkSurfaceFormatKHR> formats(formatCount);
    //     if (formatCount != 0) {
    //       CheckVKR(vkGetPhysicalDeviceSurfaceFormatsKHR, vulkan()->physicalDevice(), _main_window_surface, &formatCount, formats.data());
    //     }
    //     string_t fmts = "Surface formats: " + Os::newline();
    //     for (int i = 0; i < formats.size(); ++i) {
    //       fmts += " Format " + i;
    //       fmts += "  Color space: " + VulkanDebug::VkColorSpaceKHR_toString(formats[i].colorSpace);
    //       fmts += "  Format: " + VulkanDebug::VkFormat_toString(formats[i].format);
    //     }
    //
    //     // How the surfaces are presented from the swapchain.
    //     uint32_t presentModeCount;
    //     CheckVKR(vkGetPhysicalDeviceSurfacePresentModesKHR, vulkan()->physicalDevice(), _main_window_surface, &presentModeCount, nullptr);
    //     std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    //     if (presentModeCount != 0) {
    //       CheckVKR(vkGetPhysicalDeviceSurfacePresentModesKHR, vulkan()->physicalDevice(), _main_window_surface, &presentModeCount, presentModes.data());
    //     }
    //     //This is cool. Directly query the color space
    //     VkSurfaceFormatKHR surfaceFormat;
    //     for (const auto& availableFormat : formats) {
    //       if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
    //         surfaceFormat = availableFormat;
    //         break;
    //       }
    //     }
    //     //VK_PRESENT_MODE_FIFO_KHR mode is guaranteed to be available
    //     VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    //     for (const auto& availablePresentMode : presentModes) {
    //       if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
    //         presentMode = availablePresentMode;
    //         break;
    //       }
    //     }
    //     if (presentMode == VK_PRESENT_MODE_FIFO_KHR) {
    //       BRLogWarn("Mailbox present mode was not found for presenting swapchain.");
    //     }
    //
    //     VkSurfaceCapabilitiesKHR caps;
    //     CheckVKR(vkGetPhysicalDeviceSurfaceCapabilitiesKHR, vulkan()->physicalDevice(), _main_window_surface, &caps);
    //
    //     //Image count, double buffer = 2
    //     uint32_t imageCount = caps.minImageCount + 1;
    //     if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
    //       imageCount = caps.maxImageCount;
    //     }
    //     if (imageCount > 2) {
    //       BRLogDebug("Supported Swapchain Image count > 2 : " + imageCount);
    //     }
    //
    //     auto m = std::numeric_limits<uint32_t>::max();
    //
    //     int win_w = 0, win_h = 0;
    //     SDL_GetWindowSize(_pSDLWindow, &win_w, &win_h);
    //     _swapChainExtent.width = win_w;
    //     _swapChainExtent.height = win_h;
    //
    //     //Extent = Image size
    //     //Not sure what this as for.
    //     // if (caps.currentExtent.width != m) {
    //     //   _swapChainExtent = caps.currentExtent;
    //     // }
    //     // else {
    //     //   VkExtent2D actualExtent = { 0, 0 };
    //     //   actualExtent.width = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, actualExtent.width));
    //     //   actualExtent.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, actualExtent.height));
    //     //   _swapChainExtent = actualExtent;
    //     // }
    //
    //     //Create swapchain
    //     VkSwapchainCreateInfoKHR swapChainCreateInfo = {
    //       .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    //       .surface = _main_window_surface,
    //       .minImageCount = imageCount,
    //       .imageFormat = surfaceFormat.format,
    //       .imageColorSpace = surfaceFormat.colorSpace,
    //       .imageExtent = _swapChainExtent,
    //       .imageArrayLayers = 1,  //more than 1 for stereoscopic application
    //       .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    //       .preTransform = caps.currentTransform,
    //       .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    //       .presentMode = presentMode,
    //       .clipped = VK_TRUE,
    //       .oldSwapchain = VK_NULL_HANDLE,  // ** For window resizing.
    //     };
    //
    //     CheckVKR(vkCreateSwapchainKHR, vulkan()->device(), &swapChainCreateInfo, nullptr, &_swapChain);
    //     CheckVKR(vkGetSwapchainImagesKHR, vulkan()->device(), _swapChain, &imageCount, nullptr);
    //
    //     _swapChainImages.resize(imageCount);
    //     CheckVKR(vkGetSwapchainImagesKHR, vulkan()->device(), _swapChain, &imageCount, _swapChainImages.data());
    //
    //     _swapChainImageFormat = surfaceFormat.format;
    //     //}
    //     //void createSwapchainImageViews() {
    //     BRLogInfo("Creating Image Views.");
    //     for (size_t i = 0; i < _swapChainImages.size(); i++) {
    //       auto view = vulkan()->createImageView(_swapChainImages[i], _swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    //       _swapChainImageViews.push_back(view);
    //     }
    // }
  }

  //**Original data directly from the big class
  // //Asynchronous computing depends on the swapchain count.
  // //_iConcurrentFrames should be set to the swapchain count as vkAcquireNExtImageKHR gets the next usable image.
  // //You can't have more than #swapchain images rendering at a time.
  // int32_t _iConcurrentFrames = 3;
  // size_t _currentFrame = 0;
  // std::vector<VkFence> _inFlightFences;
  // std::vector<VkFence> _imagesInFlight;
  // std::vector<VkSemaphore> _imageAvailableSemaphores;
  // std::vector<VkSemaphore> _renderFinishedSemaphores;
  // std::vector<std::shared_ptr<VulkanBuffer>> _uniformBuffers;  //One per swapchain image since theres multiple frames in flight.
  // VkSwapchainKHR _swapChain = VK_NULL_HANDLE;
  // VkExtent2D _swapChainExtent;
  // VkFormat _swapChainImageFormat;
  // std::vector<VkImage> _swapChainImages;
  // std::vector<VkImageView> _swapChainImageViews;
  // std::vector<VkFramebuffer> _swapChainFramebuffers;

  //Should allow the creation and destcuction of swapchain.
  // Vulkan
  //  VulkanSwapchain s(..args)
  // when window is resized.
  //  _swapchain = new Swapchain
  // ~VulkanSwapchain
  //    cleanupSwapchain()
  // extent() { return _extent (width and height)}
};
class MeshComponent {
public:
};

}  // namespace VG

#endif
