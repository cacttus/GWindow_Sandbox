# GWindow_Sandbox
Sandbox for testing the Vulkan rendering system.

## Test Preview
![Image](./screenshot-01.png)

## How to run
### Install into vscode if not installed
* c/c++ (microsoft)
* cmake language support
* cmake tools
* shader language support

### Install these packages if not installed (copy paste)
sudo apt install cmake &&
sudo apt install libsdl2-dev && 
sudo apt install libsdl2-net-dev &&
sudo apt install libvulkan-dev &&
sudo apt install libncurses-dev &&
sudo apt install ninja-build

### Install Shaderc inside of /GWindow_Sandbox directory (copy paste)
git clone https://github.com/google/shaderc.git  &&
cd shaderc &&
./utils/git-sync-deps &&
mkdir build &&
cmake -GNinja -DCMAKE_BUILD_TYPE=Release -S ./ -B ./build &&
cd ./build &&
ninja



