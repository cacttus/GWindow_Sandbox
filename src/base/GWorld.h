/**
*  @file GameClasses.h
*  @date 09/19/2020
*  @author Derek Page
*/
#pragma once
#ifndef __GAMECLASSES_16031607627136953668913090414_H__
#define __GAMECLASSES_16031607627136953668913090414_H__

#include "./VulkanHeader.h"
#include "./VulkanClasses.h"

namespace VG {

class FpsMeter {
public:
  bool deltaMs(uint64_t& __inout_ last, uint64_t ms);
  float getFps() { return _fpsLast; }
  float getFpsAvg() { return _fpsLast; }
  void update();
  uint64_t getFrameNumber() { return _iFrame; }
  bool frameMod(uint64_t i) {
    return (_iFrame % i) == 0;
  }

private:
  double accum = 0;
  double divisor = 0;

  uint64_t _last = 0;
  uint64_t _tmr = 0;
  float _fpsLast = 60;
  uint64_t _iFrame = 0;  //Current frame number
};

#pragma region GameDummy

//A dummy game with meshes &c to test command getVkBuffer rendering
class GameDummy {
public:
  std::shared_ptr<Mesh> _mesh1 = nullptr;
  std::shared_ptr<Mesh> _mesh2 = nullptr;
  void update(double time) {
  }
};

#pragma endregion

/**
 * @class Mesh
 * */
class Mesh : public VulkanObject {
public:
  typedef v_v3c4x2n3 VertType;

public:
  Mesh(Vulkan* v);
  virtual ~Mesh() override;

  MaterialDummy* material() { return _material.get(); }
  VulkanBuffer* vertexBuffer() { return _vertexBuffer.get(); }
  VulkanBuffer* indexBuffer() { return _indexBuffer.get(); }
  IndexType indexType() { return _indexType; }

  uint32_t maxRenderInstances();
  void makeBox();
  void makePlane();
  void recopyData();

private:
  std::vector<v_v3c4x2n3> _boxVerts;
  std::vector<uint32_t> _boxInds;
  std::unique_ptr<VulkanBuffer> _vertexBuffer = nullptr;
  std::unique_ptr<VulkanBuffer> _indexBuffer = nullptr;
  RenderMode _renderMode = RenderMode::TriangleList;
  IndexType _indexType = IndexType::IndexTypeUint32;
  uint32_t _maxRenderInstances;
  VkVertexInputBindingDescription _bindingDesc;
  std::vector<VkVertexInputAttributeDescription> _attribDesc;
  std::unique_ptr<MaterialDummy> _material = nullptr;
  std::shared_ptr<BR2::VertexFormat> _vertexFormat;
};

class MaterialDummy {
public:
  std::shared_ptr<TextureImage> _texture = nullptr;
};

}  // namespace VG

#endif
