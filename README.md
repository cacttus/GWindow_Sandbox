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

### Ubuntu, Linux Mint, 
* Replaced setup with a script.
* Run setup.ubuntu.sh in terminal as root IN the GWindow_Sandbox folder for Mint and Ubuntu versions.

### Else
* Copy packages from setup.ubuntu.sh for pacman
* Install shaderc see setup.ubuntu.sh

### Build VSCode
Ctrl+Shift+P
cmake: build
cmake: debug

### Note
If you get "X Error of failed request: BadValue (integer parameter out of range for operation)" try restarting
