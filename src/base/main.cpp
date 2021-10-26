/*
  This is a test of the Vulkan API.
*/
#include "./SandboxHeader.h" 
#include "./VulkanHeader.h"
#include "./GWindowHeader.h" 
#include "./GWindow.h"
#include "./AsyncSceneTest.h"

int main(int argc, char** argv) {
  VG::App::_appRoot.assign(VG::App::getDirectoryNameFromPath(argv[0])+ "/..") ;

  BRLogInfo("Creating vulkan.");
  VG::GSDL gsdl;
  try {
    gsdl.init();
    gsdl.createWindow();
    gsdl.createWindow();
    gsdl.start();
    gsdl.renderLoop();
    //std::shared_ptr<VG::GWindow> win1 = sv.createWindow();
    //std::shared_ptr<VG::GWindow> win2 = sv.createWindow();
  }
  catch (std::exception& ex) {
  }
  //asyncSceneTest();

  return 0;
}