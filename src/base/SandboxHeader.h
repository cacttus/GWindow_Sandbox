/**
 *  @file SandboxHeader.h
 *  @date 09/02/2020
 *  @author MetalMario971
 */
#pragma once
#ifndef __SANDBOXHEADER_160169492510355883169343686315_H__
#define __SANDBOXHEADER_160169492510355883169343686315_H__

#include "../../../VulkanGame/src/math/Mat4x.h"
#include "../../../VulkanGame/src/math/Vec4x.h"
#include "../../../VulkanGame/src/math/Vec3x.h"
#include "../../../VulkanGame/src/math/Vec2x.h"
#include "../../../VulkanGame/src/math/MathAll.h"
#include "../../../VulkanGame/src/ext/lodepng.h"

#include "../ext/spirv-reflect/spirv_reflect.h"

//
// #ifdef _WIN32
// #define BR2_OS_WINDOWS 1
// #elif defined(linux)
// #define BR2_OS_LINUX 1
// #endif

#ifdef BR2_OS_WINDOWS
//This is need as std::numeric_limits::max conflicts with #define max()
#define NOMINMAX
#endif

#include <string>
#include <thread>
#include <vector>
#include <memory>
#include <string>
#include <iostream>
#include <condition_variable>
#include <cstdlib>
#include <future>
#include <deque>
#include <optional>
#include <algorithm>
#include <set>
#include <unordered_map>
#include <fstream>
#include <optional>
#include <limits>
#include <filesystem>
#include <array>

#ifdef _WIN32
//#define WIN32_LEAN_AND_MEAN
//#include <Windows.h>
#elif defined(__UNIX__)
#include <X11/Xlib.h>
#include <X11/keysym.h>
// Use XQueryKeymap()
#endif

//#define BR2_CPP17

#ifdef BR2_OS_WINDOWS
#include <direct.h>
#else
#include <dirent.h>
#include <unistd.h>
//For Sigtrap.
#include <signal.h>
#endif

#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <SDL.h>
#include <SDL_syswm.h>

#ifdef main
#undef main
#endif

//using namespace BR2;

namespace VG {

//This is a stub namespace for the base BR2 functions.
typedef std::string string_t;
//Defines
#ifdef BRThrowException
#undef BRThrowException
#endif
#define BRThrowException(x) throw std::string(x);
static void log_log(const std::string& str) {
  std::cout << str << std::endl;
}
#define BRLogInfo(xx) VG::log_log(Stz xx)
#define BRLogError(xx) BRLogInfo(Stz "Error:" + xx)
#define BRLogWarn(xx) BRLogInfo(Stz "Warning: " + xx)
#define BRLogWarnOnce(xx) BRLogWarn(xx)
#define BRLogDebug(xx) BRLogInfo(Stz "Debug: " + xx)
#ifdef AssertOrThrow2
#undef AssertOrThrow2
#endif
#define AssertOrThrow2(x)         \
  do {                            \
    VG::assertOrThrow((bool)(x)); \
  } while (0);

//BRLogWarn("oops") expands to
//VG::log_log(std::string("") + (std::string("") + "Warning: " + (std::string("") + "oops")))

//String
typedef std::string string_t;
#define Stz std::string("") +
std::string operator+(const std::string& str, const char& rhs);
std::string operator+(const std::string& str, const int8_t& rhs);
std::string operator+(const std::string& str, const int16_t& rhs);
std::string operator+(const std::string& str, const int32_t& rhs);
std::string operator+(const std::string& str, const int64_t& rhs);
std::string operator+(const std::string& str, const uint8_t& rhs);
std::string operator+(const std::string& str, const uint16_t& rhs);
std::string operator+(const std::string& str, const uint32_t& rhs);
std::string operator+(const std::string& str, const uint64_t& rhs);
std::string operator+(const std::string& str, const double& rhs);
std::string operator+(const std::string& str, const float& rhs);

#define VFMT_VEC2 VK_FORMAT_R32G32_SFLOAT
#define VFMT_VEC3 VK_FORMAT_R32G32B32_SFLOAT
#define VFMT_VEC4 VK_FORMAT_R32G32B32A32_SFLOAT

//Classes
class v_v2c4 {
public:
  BR2::vec2 _pos;
  BR2::vec4 _color;
  v_v2c4() {}
  v_v2c4(const BR2::vec2& pos, const BR2::vec4& color) {
    _pos = pos;
    _color = color;
  }
  static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;  // layout location=
    attributeDescriptions[0].format = VFMT_VEC2;
    attributeDescriptions[0].offset = offsetof(v_v2c4, _pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;  // layout location=
    attributeDescriptions[1].format = VFMT_VEC4;
    attributeDescriptions[1].offset = offsetof(v_v2c4, _color);
    return attributeDescriptions;
  }
  static VkVertexInputBindingDescription getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription = {
      .binding = 0,              // uint32_t   index of the binding in the array of bindings -- Ex we can have like an OBJ file with multiple buffers for v,c,n.. instead of interleaved arrays
      .stride = sizeof(v_v2c4),  // uint32_t
      // **Instanced rendering.
      //**use VK_VERTEX_INPUT_RATE_INSTANCE
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,  // VkVertexIputRate
    };

    return bindingDescription;
  }
};
class v_v3c4 {
public:
  BR2::vec3 _pos;
  BR2::vec4 _color;
  v_v3c4() {}
  v_v3c4(const BR2::vec3& pos, const BR2::vec4& color) {
    _pos = pos;
    _color = color;
  }
  static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;  // layout(location=)
    attributeDescriptions[0].format = VFMT_VEC3;
    attributeDescriptions[0].offset = offsetof(v_v3c4, _pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VFMT_VEC4;
    attributeDescriptions[1].offset = offsetof(v_v3c4, _color);

    return attributeDescriptions;
  }
  static VkVertexInputBindingDescription getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription = {
      .binding = 0,              // uint32_t         -- this is the layout location
      .stride = sizeof(v_v3c4),  // uint32_t
      // **Instanced rendering.
      //**use VK_VERTEX_INPUT_RATE_INSTANCE
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,  // VkVertexIputRate

    };

    return bindingDescription;
  }
};
class v_v3c4x2 {
public:
  BR2::vec3 _pos;
  BR2::vec4 _color;
  BR2::vec2 _tcoord;
  v_v3c4x2() {}
  v_v3c4x2(const BR2::vec3& pos, const BR2::vec4& color, const BR2::vec2& tcoord) {
    _pos = pos;
    _color = color;
    _tcoord = tcoord;
  }
  static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions(3);

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;  // layout(location=)
    attributeDescriptions[0].format = VFMT_VEC3;
    attributeDescriptions[0].offset = offsetof(v_v3c4x2, _pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VFMT_VEC4;
    attributeDescriptions[1].offset = offsetof(v_v3c4x2, _color);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VFMT_VEC2;
    attributeDescriptions[2].offset = offsetof(v_v3c4x2, _tcoord);

    return attributeDescriptions;
  }
  static VkVertexInputBindingDescription getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription = {
      .binding = 0,                // uint32_t         -- this is the layout location
      .stride = sizeof(v_v3c4x2),  // uint32_t
      // **Instanced rendering.
      //**use VK_VERTEX_INPUT_RATE_INSTANCE
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,  // VkVertexIputRate
    };

    return bindingDescription;
  }
};

class App {
public:
  static std::string _appRoot;
  static string_t combinePath(const std::string& a, const std::string& b);
  static string_t getFileNameFromPath(const string_t& name);
  static string_t replaceAll(const string_t& str, char charToRemove, char charToAdd);
  static string_t formatPath(const string_t& p);
  static string_t getDirectoryNameFromPath(const string_t& pathName);
  static string_t toHex(int value, bool bIncludePrefix);
  static std::string rootFile(const std::string file) {
    //Return the filename relative to project root directory.
#ifdef BR2_OS_LINUX
    //cmake - /bin
    return Stz "./../" + file;
#else
    //msvc - /bin/debug
    return Stz "./../../" + file;
#endif
  }
};
class Img32 {
public:
  unsigned char* _data = nullptr;
  std::size_t data_len_bytes = 0;
  uint32_t _width = 0;
  uint32_t _height = 0;
};
class Os {
public:
  static string_t newline() {
#ifdef BR2_OS_LINUX
    return string_t("\n");
#else
    return string_t("\r\n");
#endif
  }
};
class GraphicsWindow {
};
class SDLUtils {
public:
  static void checkSDLErr(bool bLog = true, bool bBreak = true);
};
class Gu {
public:
  static void debugBreak();
  static std::vector<char> readFile(const std::string& file);
};
static void assertOrThrow(bool b) {
  if (!b) {
    VG::Gu::debugBreak();
    throw new std::runtime_error("Runtime Error thrown.");
  }
}
class GraphicsWindowCreateParameters {
public:
  static constexpr int Wintype_Desktop = 0;  // X11 doesn't encourage disabling buttons like win32, so we're going
                                             // with 'window types' instead of disabling properties.
  static constexpr int Wintype_Utility = 1;
  static constexpr int Wintype_Noborder = 2;

  string_t _title = "My_Window";
  int32_t _x = 100;
  int32_t _y = 100;
  int32_t _width = 800;
  int32_t _height = 600;
  int32_t _type = Wintype_Desktop;
  bool _fullscreen = false;
  bool _show = true;  // Show after creating
  bool _forceAspectRatio =
    false;  // Forces the window buffer to be the same aspect as the screen.
  std::shared_ptr<GraphicsWindow> _parent = nullptr;
  GraphicsWindowCreateParameters(
    const string_t& title, int32_t x, int32_t y, int32_t width,
    int32_t height, int32_t type, bool fullscreen, bool show,
    bool forceAspectRatio, std::shared_ptr<GraphicsWindow> parent = nullptr) {
    _title = title;
    _x = x;
    _y = y;
    _width = width;
    _height = height;
    _fullscreen = fullscreen;
    _show = show;
    _forceAspectRatio = forceAspectRatio;
    _parent = parent;
    _type = type;
  }
};

}  // namespace VG

#endif
