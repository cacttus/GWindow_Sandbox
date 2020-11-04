#include "./VulkanDebug.h"
#include "./VulkanClasses.h"

namespace VG {

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                    void* pUserData) {
  std::string msghead = "[GPU]";
  if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
    msghead += Stz "[G]";
  }
  else if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
    msghead += Stz "[V]";
  }
  else if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
    msghead += Stz "[P]";
  }
  else {
    msghead += Stz "[?]";
  }

  std::string msg = "";
  if (pCallbackData != nullptr) {
    msg = std::string(pCallbackData->pMessage);
  }

  if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    msghead += Stz "[V]";
    msghead += Stz ":";
    BRLogInfo(msghead + msg);
  }
  else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    msghead += Stz "[I]";
    msghead += Stz ":";
    BRLogInfo(msghead + msg);
  }
  else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    msghead += Stz "[W]";
    msghead += Stz ":";
    BRLogWarn(msghead + msg);
  }
  else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    msghead += Stz "[E]";
    msghead += Stz ":";
    BRLogError(msghead + msg);
  }
  else {
    msghead += Stz "[?]";
    msghead += Stz ":";
    BRLogWarn(msghead + msg);
  }
  return VK_FALSE;
}
static VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallback(
  VkDebugReportFlagsEXT flags,
  VkDebugReportObjectTypeEXT objectType,
  uint64_t object,
  size_t location,
  int32_t messageCode,
  const char* pLayerPrefix,
  const char* pMessage,
  void* pUserData) {
  //BRLogDebug(std::string("    [GPU] p:") + std::string(pLayerPrefix) + std::string("c:") + std::to_string(messageCode) + " -> " + std::string(pMessage));

  return VK_FALSE;
}

VulkanDebug::VulkanDebug(std::shared_ptr<Vulkan> v, bool enableDebug) : VulkanObject(v) {
  _enableDebug = enableDebug;
}
VulkanDebug::~VulkanDebug() {
  if (_debugMessenger != VK_NULL_HANDLE) {
    vkDestroyDebugUtilsMessengerEXT(vulkan()->instance(), _debugMessenger, nullptr);
  }
  if (_debugReporter != VK_NULL_HANDLE) {
    vkDestroyDebugReportCallbackEXT(vulkan()->instance(), _debugReporter, nullptr);
  }
}
void VulkanDebug::createDebugObjects() {
  if (!_enableDebug) {
    return;
  }

  createDebugMessenger();
  createDebugReport();
}

void VulkanDebug::createDebugReport() {
  VkLoadExt(vulkan()->instance(), vkCreateDebugReportCallbackEXT);
  VkLoadExt(vulkan()->instance(), vkDestroyDebugReportCallbackEXT);
  if (vkCreateDebugReportCallbackEXT == nullptr || vkDestroyDebugReportCallbackEXT == nullptr) {
    BRLogWarn("Debug reporting is not supported or you forgot to load the extension.");
  }
  else {
    VkDebugReportCallbackCreateInfoEXT info = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
      .pNext = nullptr,
      .flags = VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT |
               VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT,
      .pfnCallback = &debugReportCallback,
      .pUserData = nullptr
    };

    CheckVKR(vkCreateDebugReportCallbackEXT, vulkan()->instance(), &info, nullptr, &_debugReporter);
  }
}
void VulkanDebug::createDebugMessenger() {
  VkLoadExt(vulkan()->instance(), vkCreateDebugUtilsMessengerEXT);
  VkLoadExt(vulkan()->instance(), vkDestroyDebugUtilsMessengerEXT);
  if (vkCreateDebugUtilsMessengerEXT == nullptr || vkDestroyDebugUtilsMessengerEXT == nullptr) {
    BRLogWarn("Debug messaging is not supported or you forgot to load the extension.");
  }
  else {
    VkDebugUtilsMessengerCreateInfoEXT msginfo = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .pNext = nullptr,
      .flags = 0,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback = debugCallback,
      .pUserData = nullptr,
    };

    CheckVKR(vkCreateDebugUtilsMessengerEXT, vulkan()->instance(), &msginfo, nullptr, &_debugMessenger);
  }
}

}  // namespace VG
