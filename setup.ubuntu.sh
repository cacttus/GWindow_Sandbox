#!/bin/bash
var1=$(echo "${PWD##*/}")
var2="GWindow_Sandbox"
if [[ "$var1" == "$var2" ]]; then
echo "installing packages"
# Install these packages if not installed (copy paste)
sudo apt install git 
sudo apt install cmakea 
sudo apt install libsdl2-dev 
sudo apt install libsdl2-net-dev 
sudo apt install libvulkan-dev 
sudo apt install libncurses-dev 
sudo apt install ninja-build

# Install Shaderc inside of /GWindow_Sandbox directory (copy paste)
echo "installing shaderc"
git clone https://github.com/google/shaderc.git  
cd shaderc 
./utils/git-sync-deps 
mkdir build 
cmake -GNinja -DCMAKE_BUILD_TYPE=Release -S ./ -B ./build 
cd ./build 
ninja
echo "Done."
else
echo "Please run this script in the GWindow_Sandbox folder."
fi