#include "./SandboxHeader.h"
#include <sstream>

namespace VG {
string_t App::_appRoot = "";
string_t App::toHex(int value, bool bIncludePrefix) {
  std::stringstream ss;
  if (bIncludePrefix) {
    ss << "0x" << std::hex << value;
  }
  else {
    ss << std::hex << value;
  }
  return ss.str();
}
string_t App::combinePath(const std::string& a, const std::string& b) {
#if defined(BR2_CPP17)
  //Combining paths on Linux doesn't actually treat the RHS as a path or file if there's no /
  std::string bfmt;
  if (b.length() && ((b[0] != '\\') && (b[0] != '/'))) {
    bfmt = std::string("/") + b;
  }
  else {
    bfmt = b;
  }
  std::filesystem::path pa(a);
  std::filesystem::path pc = pa.concat(bfmt);
  string_t st = pc.string();
  return st;
#else
  if ((a.length() == 0) || (b.length() == 0)) {
    return a + b;
  }

  // - Make sure path is a / path not a \\ path
  a = formatPath(a);
  b = formatPath(b);

  // Remove / from before and after strings
  a = StringUtil::trim(a, '/');
  b = StringUtil::trim(b, '/');

  return a + string_t("/") + b;
#endif
}
string_t App::getFileNameFromPath(const string_t& name) {
  //Returns the TLD for the path. Either filename, or directory.
  // Does not include the / in the directory.
  // ex ~/files/dir would return dir
  // - If formatPath is true then we'll convert the path to a / path.
  string_t fn = "";
#if defined(BR2_CPP17)
  fn = std::filesystem::path(name).filename().string();
#else

  DiskLoc l2;

  l2 = formatPath(name);

  //Index of TLD
  size_t tld_off = l2.rfind('/');

  if (tld_off != string_t::npos) {
    fn = l2.substr(tld_off + 1, l2.length() - tld_off + 1);
  }
  else {
    fn = name;
  }
#endif
  return fn;
}
string_t App::replaceAll(const string_t& str, char charToRemove, char charToAdd) {
  string_t::size_type x = 0;
  string_t ret = str;
  string_t ch;
  ch += charToAdd;
  while ((x = ret.find(charToRemove, x)) != string_t::npos) {
    ret.replace(x, 1, ch);
    x += ch.length();
  }
  return ret;
}
string_t App::formatPath(const string_t& p) {
  return replaceAll(p, '\\', '/');
}
string_t App::getDirectoryNameFromPath(const string_t& pathName) {
  // Returns the path without the filename OR top level directory.
  // See C# GetDirectoryName()
  string_t ret = "";

  string_t formattedPath;
  string_t tmpPath = pathName;
  string_t fn;

  formattedPath = formatPath(pathName);
  fn = getFileNameFromPath(formattedPath);
  size_t x = formattedPath.rfind('/');
  if (x != string_t::npos) {
    ret = formattedPath.substr(0, x);
  }
  else {
    ret = formattedPath;
  }
  return ret;
}

void Gu::debugBreak() {
#if defined(BR2_OS_WINDOWS)
  DebugBreak();
#elif defined(BR2_OS_LINUX)
  raise(SIGTRAP);
#else
  OS_NOT_SUPPORTED_ERROR
#endif
}
std::vector<char> Gu::readFile(const std::string& file) {
  string_t file_loc = App::combinePath(App::_appRoot, file);
  std::cout << "Loading file " << file_loc << std::endl;

  //chdir("~/git/GWindow_Sandbox/");
  //#endif
  //::ate avoid seeking to the end. Neat trick.
  std::ifstream fs(file_loc, std::ios::in | std::ios::binary | std::ios::ate);
  if (!fs.good() || !fs.is_open()) {
    BRThrowException(Stz "Could not open shader file '" + file_loc + "'");
    return std::vector<char>{};
  }

  auto size = fs.tellg();
  fs.seekg(0, std::ios::beg);
  std::vector<char> ret(size);
  //ret.reserve(size);
  fs.read(ret.data(), size);

  fs.close();
  return ret;
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
