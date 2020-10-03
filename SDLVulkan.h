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
 *  @brief
 */
class SDLVulkan_Internal;

class VirtualMemory {
  public:
  VirtualMemory() {}
  virtual ~VirtualMemory() {}
};

class SDLVulkan : public VirtualMemory {
 public:
  SDLVulkan();
  virtual ~SDLVulkan() override;

  void init();

  std::unique_ptr<SDLVulkan_Internal> _pInt;
};

}  // namespace VG

#endif
