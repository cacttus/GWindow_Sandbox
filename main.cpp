/*
  Design a graphics system where:
    Scenes update independently of rendering.
      Perform unlimited updates.
      Render scene.
    Window families share the same scene.
      Window Family = sharing same GL context.
    Multiple Scenes supported.
    Rendering Pipeline shared across
      GSurface
      GWindow

  * What is the point of running update async anyway, if we can't render asynchronously (without real intervention).

*/
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <condition_variable>
#include <vector>
#include <cstdlib>
#include <future>

#ifdef _WIN32
#include <Windows.h>
#else
#include <X11/Xlib.h>
#include <X11/keysym.h>
//Use XQueryKeymap()
#endif

typedef std::string string_t;

float randomFloat() {
  static bool _frand_init = false;
  if (!_frand_init) {
    srand((unsigned int)18093050563);
    _frand_init = true;
  }
  return float(float(rand()) / float(RAND_MAX));
}

class vec3 {
public:
  float x, y, z;
  vec3() {
    x = y = z = 0;
  }
  vec3(float dx, float dy, float dz) {
    x = dx; y = dy; z = dz;
  }
  static vec3 random() {
    return vec3(randomFloat(), randomFloat(), randomFloat());
  }
  vec3& operator=(const vec3& v) {
    this->x = v.x; this->y = v.y; this->z = v.z;
    return *this;
  }
  vec3 operator+(const vec3& v) const {
    vec3 tmp = *this;
    tmp += v;
    return tmp;
  }
  vec3& operator+=(const vec3& v) {
    x += v.x;
    y += v.y;
    z += v.z;
    return *this;
  }
  vec3 operator-(const vec3& v) const {
    vec3 tmp = *this;
    tmp -= v;
    return tmp;
  }
  vec3& operator-=(const vec3& v) {
    x -= v.x;
    y -= v.y;
    z -= v.z;
    return *this;
  }
  vec3 operator/(const float& v) const {
    vec3 tmp = *this;
    tmp /= v;
    return tmp;
  }
  vec3& operator/=(const float& v) {
    x /= v;
    y /= v;
    z /= v;
    return *this;
  }
};

//Returns time equivalent to a number of frames at 60fps
#define FPS60_IN_MS(frames) ((int)((1.0f / 60.0f * frames) * 1000.0f))

void doHardWork_Frame(float f) {
  int ms = FPS60_IN_MS(f);
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
int64_t getMilliseconds() {
  int64_t ret;
  std::chrono::nanoseconds ns = std::chrono::high_resolution_clock::now().time_since_epoch();
  ret = std::chrono::duration_cast<std::chrono::microseconds>(ns).count();
  return ret / 1000;
}
//////////////////////////////////////////////////////

class GObject;
class GScene;

//////////////////////////////////////////////////////
// VG Dummies

//Dummy
class RenderWindow {
public:
  string_t _name = "<unset>";
  RenderWindow(const string_t& name) {
    _name = name;
  }
  void setScene(std::shared_ptr<GScene> s) {
    _scene = s;
  }
  int64_t _last = 0;
  int64_t _update_msg_timer = 0;
  int64_t _render_count = 0;

  void renderScene();
  bool processInput() {

    //Dummy input to simulate some main-thread stuff
#ifdef _WIN32
    string_t st = "";
    if (GetAsyncKeyState(VK_UP) & 0x8000) { st += "Up"; }
    else if (GetAsyncKeyState(VK_DOWN) & 0x8000) { st += "Dn"; }
    if (GetAsyncKeyState(VK_LEFT) & 0x8000) { st += "Lt"; }
    else if (GetAsyncKeyState(VK_RIGHT) & 0x8000) { st += "Rt"; }
    if (st.length()) {
      std::cout << st;
    }

    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
      return false;
    }

    return true;
#else

#endif
  }

  std::shared_ptr<GScene> _scene;
private:
};

//////////////////////////////////////////////////////

class AsyncEngineComponent : public std::enable_shared_from_this<AsyncEngineComponent> {
public:
  AsyncEngineComponent(const string_t& name) {
    _name = name;
  }
  std::thread _myThread;
  void launch() {
    std::weak_ptr<AsyncEngineComponent> weak = shared_from_this();
    _myThread = std::thread([weak] {
      int64_t elapsed = 0;
      int64_t count = 0;
      int64_t last = getMilliseconds();

      if (std::shared_ptr<AsyncEngineComponent> ua = weak.lock()) {

        if (ua->_bTerminate == true) {
          std::cout << "Async Component " << ua->_name << " terminated.";
          return false;
        }

        if (ua->getMtx().try_lock()) {
          double elapsed_seconds = (double)(getMilliseconds() - last) / 1000.0f;
          ua->update(elapsed_seconds);
          count++;

          int64_t cur = getMilliseconds();
          elapsed += (cur - last);
          if (elapsed > 2000) {
            std::cout << ua->_name << " updated " << count << " times in 2s.." << std::endl;
            elapsed = 0;
            count = 0;
          }
          ua->getMtx().unlock();
        }
      }

      return true;
      });
  }
  void wait() {
    _myThread.join();
  }

  void terminate() {
    _bTerminate = true;
  }

  std::mutex& getMtx() { return _mtx; }
protected:
  virtual void update(double elapsed) = 0;

  //std::condition_variable _cv;
  std::mutex _mtx;
  std::atomic_bool _bTerminate = false;
  string_t _name = "<unset>";
};
class GObject {
public:
  // Update + Render must use the same variables to test for races.
  vec3 _pos;
  vec3 _vel;
  string_t _name = "<unset>";

  std::string* _error_monster = nullptr;
  std::string _error_monster_last = "";

  GObject(const string_t& name) {
    _name = name;
  }
  void update() {
    doWork_Using_Shared_Vars();
  }
  void render() {
    doWork_Using_Shared_Vars();

    //**Make the rendering frame more realistic.
   // doHardWork_Frame(.9);
  }
  void doWork_Using_Shared_Vars() {
    //pointer nonsense to make errors happen.
    if (_error_monster != nullptr) {
      _error_monster_last = *_error_monster;
      delete _error_monster;
    }
    _error_monster = new std::string();
    *_error_monster = std::string(128, 'A');
    _error_monster_last = _error_monster_last.substr(0, _error_monster_last.length() / 2);

    //Do some BS operation. to update position/velocity
    for (int i = 0; i < 100; ++i) {
      vec3 v = vec3::random();
      vec3 v2 = vec3::random();
      vec3 v3 = vec3::random();
      v += v2;
      v -= v3;
      _vel = (_vel + v) / 2.0f;
      _pos = (_pos + v) / 2.0f;
    }
  }
};
class GScene : public AsyncEngineComponent {
public:
  GScene(const string_t& name) : AsyncEngineComponent(name) {
  }
  void addObject(std::shared_ptr<GObject> ob) {
    std::lock_guard<std::mutex>(this->_mtx);
    _objs.push_back(ob);
  }
  void render() {
    for (auto obj : _objs) {
      obj->render();
    }
  }
protected:
  void update(double elapsed) override {
    //This is running asynchronously.
    for (auto obj : _objs) {
      obj->update();
    }
  }
private:
  std::vector<std::shared_ptr<GObject>> _objs;
};
void RenderWindow::renderScene() {
  int64_t elapsed = (getMilliseconds() - _last);
  if (elapsed > 2000) {
    std::cout << _name << " rendered " << _render_count << " times in 2s.. last window family frame took " << elapsed << "ms" << std::endl;
    _last = getMilliseconds();
    _render_count = 0;
  }
  if (_scene) {
    std::lock_guard<std::mutex> guard(_scene->getMtx());
    //_scene->render();

    doHardWork_Frame(1);//Take up exactly 60fps.

    _render_count++;
  }
}

int main() {
  // Windows = Main thread
  std::vector<std::shared_ptr<GScene>> scenes;
  std::vector<std::shared_ptr<RenderWindow>> windows;
  for (int i = 0; i < 30; ++i) {
    scenes.push_back(std::make_shared<GScene>(std::string("") + "Scene" + std::to_string(i)));
    for (int iObj = 0; iObj < 10; ++iObj) {
      scenes[scenes.size() - 1]->addObject(std::make_shared<GObject>(std::string("") + "Scene" + std::to_string(i) + "_obj" + std::to_string(iObj)));
    }

    std::shared_ptr<RenderWindow> rw = std::make_shared<RenderWindow>(std::string("") + "Window" + std::to_string(i));
    rw->setScene(scenes[scenes.size() - 1]);
    windows.push_back(rw);
  }

  //Update loop
  while (true) {
    bool window_requested_exit = false;

    for (auto win : windows) {
      if (!win->processInput()) {
        break;
      }
      win->_scene->launch();
    }
    //fence.
    for (auto win : windows) {
      win->_scene->wait();
    }
    for (auto win : windows) {
      win->renderScene();
    }
  }

  std::cout << "Press any key to exit..." << std::endl;
  std::cin.get();

  return 0;
}