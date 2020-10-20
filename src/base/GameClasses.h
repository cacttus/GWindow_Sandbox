/**
*  @file GameClasses.h
*  @date 09/19/2020
*  @author MetalMario971
*/
#pragma once
#ifndef __GAMECLASSES_16031607627136953668913090414_H__
#define __GAMECLASSES_16031607627136953668913090414_H__

#include "./VulkanClasses.h"

namespace VG {
class Mesh;
class GameDummy;

#pragma region GameDummy

//A dummy game with meshes &c to test command buffer rendering
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
  typedef v_v3c4x2 VertType;

public:
  Mesh(std::shared_ptr<Vulkan> v);
  virtual ~Mesh() override;

  std::shared_ptr<MaterialDummy>& material() { return _material; }

  uint32_t maxRenderInstances();
  void makeBox();
  void makePlane();
  void drawIndexed(std::shared_ptr<CommandBuffer> cmd, uint32_t instanceCount);
  void bindBuffers(std::shared_ptr<CommandBuffer> cmd);

private:
  std::vector<v_v3c4x2> _boxVerts;
  std::vector<uint32_t> _boxInds;
  std::vector<v_v2c4> _planeVerts;
  std::vector<uint32_t> _planeInds;

  std::shared_ptr<VulkanBuffer> _vertexBuffer = nullptr;
  std::shared_ptr<VulkanBuffer> _indexBuffer = nullptr;

  RenderMode _renderMode = RenderMode::TriangleList;
  IndexType _indexType = IndexType::IndexTypeUint32;

  uint32_t _maxRenderInstances;

  VkVertexInputBindingDescription _bindingDesc;
  std::vector<VkVertexInputAttributeDescription> _attribDesc;

  std::shared_ptr<MaterialDummy> _material = nullptr;

  std::shared_ptr<BR2::VertexFormat> _vertexFormat;
};

class MaterialDummy {
public:
  std::shared_ptr<VulkanTextureImage> _texture = nullptr;
};

}//ns Game



#endif
