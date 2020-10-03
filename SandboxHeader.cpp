#include "./SandboxHeader.h"


namespace VG {
void Gu::debugBreak() {
#if defined(BR2_OS_WINDOWS)
  DebugBreak();
#elif defined(BR2_OS_LINUX)
  raise(SIGTRAP);
#else
  OS_NOT_SUPPORTED_ERROR
#endif
}
std::string operator+(const std::string& str, const char& rhs) {
  return str + std::to_string(rhs);
}
std::string operator+(const std::string& str, const int8_t& rhs) {
  return str + std::to_string(rhs);
}
std::string operator+(const std::string& str, const int16_t& rhs) {
  return str + std::to_string(rhs);
}
std::string operator+(const std::string& str, const int32_t& rhs) {
  return str + std::to_string(rhs);
}
std::string operator+(const std::string& str, const int64_t& rhs) {
  return str + std::to_string(rhs);
}

std::string operator+(const std::string& str, const uint8_t& rhs) {
  return str + std::to_string(rhs);
}
std::string operator+(const std::string& str, const uint16_t& rhs) {
  return str + std::to_string(rhs);
}
std::string operator+(const std::string& str, const uint32_t& rhs) {
  return str + std::to_string(rhs);
}
std::string operator+(const std::string& str, const uint64_t& rhs) {
  return str + std::to_string(rhs);
}

std::string operator+(const std::string& str, const double& rhs) {
  return str + std::to_string(rhs);
}
std::string operator+(const std::string& str, const float& rhs) {
  return str + std::to_string(rhs);
}

void SDLUtils::checkSDLErr(bool bLog, bool bBreak) {
  // Do SDL errors here as well
  const char* c;
  while ((c = SDL_GetError()) != nullptr && *c != 0) {
    // linux : GLXInfo
    if (bLog == true) {
      BRLogError("SDL: " + c);
    }

    //   if (Gu::getEngineConfig()->getBreakOnSDLError() == true && bBreak) {
    Gu::debugBreak();
    //}
    SDL_ClearError();
  }
}

}  // namespace VG
