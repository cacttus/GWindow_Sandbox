/**
 *  @file SDLVulkan.h
 *  @date 09/02/2020
 *  @author MetalMario971
 */
#pragma once
#ifndef __SDLVULKAN_160169401317456691411060057473_H__
#define __SDLVULKAN_160169401317456691411060057473_H__

#include "./SandboxHeader.h"

namespace VG {
/**
 *  @class SDLVulkan
 *  @brief Test class for Vulkan.
 */
class SDLVulkan  {
public:
  SDLVulkan();
  virtual ~SDLVulkan();
   
  bool doInput();
  void init();
  void renderLoop();

class SDLVulkan_Internal;
  std::unique_ptr<SDLVulkan_Internal> _pInt;
};

}  // namespace VG

#endif
