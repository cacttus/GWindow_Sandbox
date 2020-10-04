/**
 *  @file SandboxHeader.h
 *  @date 09/02/2020
 *  @author MetalMario971
 */
#pragma once
#ifndef __SANDBOXHEADER_160169492510355883169343686315_H__
#define __SANDBOXHEADER_160169492510355883169343686315_H__

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
#include <dirent.h>
#include <unistd.h>
#include <limits.h>

#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_net.h>

#ifndef BR2_OS_WINDOWS
//For Sigtrap.
#include <signal.h>
#endif

#ifdef _WIN32
#define BR2_OS_WINDOWS 1
#else
#define BR2_OS_LINUX 1
#endif

namespace VG {

//Defines
#define BRThrowException(x) throw std::string(x);
static void log_log(const std::string& str) {
  std::cout << str << std::endl;
}
#define BRLogInfo(xx) VG::log_log(Stz xx)
#define BRLogError(xx) BRLogInfo(Stz "Error:" + xx)
#define BRLogWarn(xx) BRLogInfo(Stz "Warning: " + xx)
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

//Classes
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
};
class GraphicsWindowCreateParameters {
public:
  static constexpr int Wintype_Desktop =
      0;  // X11 doesn't encourage disabling buttons like win32, so we're going
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
