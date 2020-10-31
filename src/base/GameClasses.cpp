#include "./GameClasses.h"
#include "./VulkanClasses.h"
#include "./SandboxHeader.h"
#include "./Vulkan.h"

namespace VG {

bool FpsMeter::deltaMs(uint64_t& __inout_ last, uint64_t ms) {
  uint64_t cur = Gu::getMicroseconds();
  if (cur - last >= ms) {
    last = cur;
    return true;
  }
  return false;
}
void FpsMeter::update() {
  //Only call this once per frame.
  uint64_t cur = Gu::getMicroseconds();

  //Average FPS over 1/2s (new method, less choppy)
  uint64_t delta = cur - _last;
  double fps_numerator = 1000000;
  if (delta == 0) {
    delta = (uint64_t)fps_numerator;
  }
  accum += fps_numerator / delta;
  divisor += 1.0f;
  _last = cur;

  if (cur - _tmr > 500000) {  //Update .5s
    if (divisor == 0) {
      divisor = accum;
    }
    _fpsLast = (float)(accum/ divisor);
    _tmr = cur;
    accum = 0;
    divisor = 0;
  }

  _iFrame++;
}

#pragma region Mesh

Mesh::Mesh(std::shared_ptr<Vulkan> v) : VulkanObject(v) {
}
Mesh::~Mesh() {
}
uint32_t Mesh::maxRenderInstances() { return _maxRenderInstances; }




//void Mesh::makePlane() {
//  //    vec2(0.0, -.5),
//  //vec2(.5, .5),
//  //vec2(-.5, .5)
//  _planeVerts = {
//    { { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f, 1 } },
//    { { 0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f, 1 } },
//    { { 0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f, 1 } },
//    { { -0.5f, 0.5f }, { 1.0f, 1.0f, 1.0f, 1 } }
//  };
//  _planeInds = {
//    0, 1, 2, 2, 3, 0
//  };
//
//  // _planeVerts = {
//  //   { { 0.0f, -0.5f }, { 1, 0, 0, 1 } },
//  //   { { 0.5f, 0.5f }, { 0, 1, 0, 1 } },
//  //   { { -0.5f, 0.5f }, { 0, 0, 1, 1 } }
//  // };
//
//  size_t v_datasize = sizeof(v_v2c4) * _planeVerts.size();
// 
//  _vertexBuffer = std::make_shared<VulkanBuffer>(
//    vulkan(),
//    VulkanBufferType::VertexBuffer,
//    true,
//    v_datasize,  
// 
//  size_t i_datasize = sizeof(uint32_t) * _planeInds.size();
//  _indexBuffer = std::make_shared<VulkanBuffer>(
//    vulkan(),
//    VulkanBufferType::IndexBuffer,
//    true,
//    i_datasize,
//    _planeInds.data(),  i_datasize);
//}
void Mesh::makeBox() {
  //      6     7
  //  2      3
  //      4     5    
  //  0      1   
  std::vector<v_v3c4> bv = { 
    { { 0, 0, 0 }, { 1, 1, 1, 1 } },
    { { 1, 0, 0 }, { 1, 1, 1, 1 } },  
    { { 0, 1, 0 }, { 1, 1, 1, 1 } },  
    { { 1, 1, 0 }, { 1, 1, 1, 1 } },  
    { { 0, 0, 1 }, { 1, 1, 1, 1 } },   
    { { 1, 0, 1 }, { 1, 1, 1, 1 } },  
    { { 0, 1, 1 }, { 1, 1, 1, 1 } }, 
    { { 1, 1, 1 }, { 1, 1, 1, 1 } },
  };    
  //Construct box from the old box coordintes (no texture)
  //Might be wrong - opengl coordinates.
#define BV_VFACE(bl, br, tl, tr, nx)            \
  { bv[bl]._pos, bv[bl]._color, { 0, 1 }, nx }, \
  { bv[br]._pos, bv[br]._color, { 1, 1 }, nx }, \
  { bv[tl]._pos, bv[tl]._color, { 0, 0 }, nx }, \
  { bv[tr]._pos, bv[tr]._color, { 1, 0 }, nx }
         
  _boxVerts = {  
    BV_VFACE(0, 1, 2, 3, BR2::vec3( 0, 0, -1) ),  //F 
    BV_VFACE(1, 5, 3, 7, BR2::vec3( 1, 0, 0 ) ),  //R
    BV_VFACE(5, 4, 7, 6, BR2::vec3( 0, 0, 1 ) ),  //A
    BV_VFACE(4, 0, 6, 2, BR2::vec3( -1, 0, 0)   ),  //L
    BV_VFACE(4, 5, 0, 1, BR2::vec3( 0, -1, 0)   ),  //B
    BV_VFACE(2, 3, 6, 7, BR2::vec3( 0, 1, 0 ) )   //T
  };  
   
//   CW
//  2------>3
//  |    / 
//  | /
//  0------>1
#define BV_IFACE(idx) ((idx * 4) + 0), ((idx * 4) + 3), ((idx * 4) + 1), ((idx * 4) + 0), ((idx * 4) + 2), ((idx * 4) + 3)
  _boxInds = {
    BV_IFACE(0),
    BV_IFACE(1),
    BV_IFACE(2),
    BV_IFACE(3),
    BV_IFACE(4),
    BV_IFACE(5),
  };
  _vertexBuffer = std::make_shared<VulkanBuffer>(
    vulkan(),
    VulkanBufferType::VertexBuffer,
    true,
    sizeof(VertType),
    _boxVerts.size(),
    _boxVerts.data(),
    _boxVerts.size());

  _indexBuffer = std::make_shared<VulkanBuffer>(
    vulkan(),
    VulkanBufferType::IndexBuffer,
    true,
    sizeof(uint32_t),
    _boxInds.size(),
    _boxInds.data(),
    _boxInds.size());
}
void Mesh::recopyData() {
  //Testing the gpu buffer
  //      6     7
  //  2      3
  //      4     5
  //  0      1
  std::vector<v_v3c4> bv = {
    { { 0, 0, 0 }, { 1, 1, 1, 1 } },
    { { 1, 0, 0 }, { 1, 1, 1, 1 } },
    { { 0, 1, 0 }, { 1, 1, 1, 1 } },
    { { 1, 1, 0 }, { 1, 1, 1, 1 } },
    { { 0, 0, 1 }, { 1, 1, 1, 1 } },
    { { 1, 0, 1 }, { 1, 1, 1, 1 } },
    { { 0, 1, 1 }, { 1, 1, 1, 1 } },
    { { 1, 1, 1 }, { 1, 1, 1, 1 } },
  };
  //Construct box from the old box coordintes (no texture)
  //Might be wrong - opengl coordinates.
#define BV_VFACE(bl, br, tl, tr, nx)              \
  { bv[bl]._pos, bv[bl]._color, { 0, 1 }, nx },   \
    { bv[br]._pos, bv[br]._color, { 1, 1 }, nx }, \
    { bv[tl]._pos, bv[tl]._color, { 0, 0 }, nx }, \
    { bv[tr]._pos, bv[tr]._color, { 1, 0 }, nx }

  _boxVerts = {
    BV_VFACE(0, 1, 2, 3, BR2::vec3(0, 0, -1)),  //F
    BV_VFACE(1, 5, 3, 7, BR2::vec3(1, 0, 0)),   //R
    BV_VFACE(5, 4, 7, 6, BR2::vec3(0, 0, 1)),   //A
    BV_VFACE(4, 0, 6, 2, BR2::vec3(-1, 0, 0)),  //L
    BV_VFACE(4, 5, 0, 1, BR2::vec3(0, -1, 0)),  //B
    BV_VFACE(2, 3, 6, 7, BR2::vec3(0, 1, 0))    //T
  };

//   CW
//  2------>3
//  |    /
//  | /
//  0------>1
#define BV_IFACE(idx) ((idx * 4) + 0), ((idx * 4) + 3), ((idx * 4) + 1), ((idx * 4) + 0), ((idx * 4) + 2), ((idx * 4) + 3)
  _boxInds = {
    BV_IFACE(0),
    BV_IFACE(1),
    BV_IFACE(2),
    BV_IFACE(3),
    BV_IFACE(4),
    BV_IFACE(5),
  };
  _vertexBuffer->vulkan()->waitIdle();  
  _vertexBuffer->writeData(_boxVerts.data(), _boxVerts.size());
  _indexBuffer->writeData(_boxInds.data(), _boxInds.size());
    
    //= std::make_shared<VulkanBuffer>(
    //vulkan(),
    //VulkanBufferType::VertexBuffer,
    //true,
    //sizeof(VertType),
    //_boxVerts.size(),
    //_boxVerts.data(),
    //_boxVerts.size());

   
    //std::make_shared<VulkanBuffer>(
    //vulkan(),
    //VulkanBufferType::IndexBuffer,
    //true,
    //sizeof(uint32_t),
    //_boxInds.size(),
    //_boxInds.data(),
    //_boxInds.size());
}


#pragma endregion

}  // namespace VG
