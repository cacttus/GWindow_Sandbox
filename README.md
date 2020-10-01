# VulkanGame
This is a game demo utilizing the SDL2 Vulkan API.

# Building 
### Visual Studio - Windows
* Open VulkanGame.sln with Visual Studio 2019.
### CMake - Windows
* Run ./cmake-windows-gcc.sh via Cygwin.
### VSCode - Linux/Windows
* Change configuration from Linux To Win32.
* Ctrl+Shift+P -> Cmake: Build -> Enter.
### CMake - Linux
* sudo apt-get update
* sudo apt-get upgrade
* sudo apt install
  * build-essential
  * libsdl2-dev
  * libsdl2-net-dev
  * libvulkan-dev
  * libopengl-dev
  * cmake
  * liblua5.3-dev
  * libluabind-dev
* Build with Cmake / VSCode.

# Tasks

### Important
* DestroyWindow is not wired up correctly (SDL_DestroyWindow is not gbeing called)
* Fix the exit in GraphicsApi if user closes child window.
* Get Windows build to work (VS).
* Fix UI to show debug text
* The width/height of the window is messed up somewhere in the engineconfig props.

### Enhancments
* Make RenderUtils be a class instance and hold the instantiated renders that we add to it.
  * Later - Add them to scene instead of using RenderUtils.
* Import Mobmaker into the app itself. We can compile it with mono or just convert the code.
* Simplified Engineconfig Properties
* Removing Default Width/Height - use percentage of screen.
* also add default X/Y
* Show window in center of screen (windowed mode)
* Test this build on Windows to make sure it still works.
* Fix the "unrecognized parameter" in the console.
* Turn Random into an instance.

### Backlog
* Change props from preprocessor to "debuggable" props.

# Design

## Folders

|  Folder | Description|
|---------|----------------------------------------------------------------------------------------------
|  base   | graphs, generic systems, memory, buffers, networking, events, OS.        |
|  ext    | External libraries.           |
|  gfx    | GPU, graphics, images, materials, lighting, framebuffers, shaders.    |
|  math   | vectors, matrices, boxes, geometry, hulls, algorithms.                    |
|  model  | meshes, animation, models, characters, skeletons, bones.                  |
|  world  | physics, scenegraph.                                     |

| Class     | Description                                                                                                                                                                                                                                                                                                                                                                                                                                             |
|-----------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
|  Package  | Application and project state inforamtion.  Scenes, models, meshes textures.                    |
|  Window   | SDL window.            |
|  Context  | Rendering context.                  |
|  Scene    | Scenegraph & meshes.          |
|  Gu       | Global static class to access core/active context.         |
|  Ui       | 2D user interface |

## Hierarchy

* Gu
  * Texture Manager (1, GLOBAL)
  * Mesh Manager (1, GLOBAL)
  * ShaderManager (1, GLOBAL)
    * Package (1, GLOBAL)
  * GraphicsApi (1, GLOBAL)
    * Context (1+)
      * GraphicsWindow (1+)
        * GraphicsWindow (child)(1+)
        * FrameSync (1)
        * Delta (1)
        * RenderPipe (1)
          * Picker (1)
        * UiScreen (1)
        * PhysicsWorld (1)
        * Scene (1)
          * LightNode (1*)
            * :PointLight
            * :DirLight
          * CameraNode (1*, only 1 active at a time)
          * PhysicsNode (1*) 
          * MeshNode (1*)
          * ModelNode (1*)
          * SceneNode (1*)
            * Component (1*)
              * BottleScript (1)
                * World25 (1)