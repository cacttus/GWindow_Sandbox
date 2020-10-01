
#include <iostream>
#include <memory>
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

double randomFloat() {
  static bool _frand_init = false;
  if (!_frand_init) {
    srand(18093050563);
    _frand_init = true;
  }
  return double((double)rand() / (double)RAND_MAX);
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

void doHardWork(float f) {
  std::this_thread::sleep_for(std::chrono::milliseconds(FPS60_IN_MS(f)));
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
//class Gu {
//public:
//  std::vector<GScene> _scenes;
//};

//Dummy
class RenderWindow {
public:
  void setScene(std::shared_ptr<GScene> s) {
    _scene = s;
  }
  void renderScene() {
    //Force access to the scene's GameObject(s)

    //Dummy rendering
    //We could actually add some GLContext stuff here if we wantd.

    //GLMakeCurrent(this)
    //Draw something
    //GLSwapBuffers
  }
  bool processInput() {

    //Dummy input to simulate some main-thread stuff
#ifdef _WIN32
    string_t st = "";
    if (GetAsyncKeyState(VK_UP) & 0x8000) { st += "Up"; }
    else if (GetAsyncKeyState(VK_DOWN) & 0x8000) { st += "Dn"; }
    if (GetAsyncKeyState(VK_LEFT) & 0x8000) { st += "Lt"; }
    else if (GetAsyncKeyState(VK_RIGHT) & 0x8000) { st += "Rt"; }
    std::cout << st << std::endl;

    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) return false;
    return true;

#else

#endif
  }

private:
  std::shared_ptr<GScene> _scene;


};

//////////////////////////////////////////////////////

class AsyncEngineComponent : public std::enable_shared_from_this<AsyncEngineComponent> {
public:
  AsyncEngineComponent(const string_t& name) {
    _name = name;
  }
  virtual void update(double elapsed) = 0;
  void start() {
    //Start this async component.
    std::weak_ptr<AsyncEngineComponent> weak = shared_from_this();

    //Update this component asynchronously.
    std::future<bool> fut = std::async(std::launch::async, [weak] {
      int64_t last = getMilliseconds();
      if (std::shared_ptr<AsyncEngineComponent> ua = weak.lock()) {
        std::unique_lock<std::mutex> ulock(ua->getMtx());
        ua->getCv().wait(ulock);

        double elapsed_seconds = (double)(getMilliseconds() - last) / 1000.0f;
        ua->update(elapsed_seconds);
      }

      return true;
      });
  }
  void pause() {

  }
  void resume() {
    _cv.notify_one();
  }

  std::condition_variable& getCv() { return _cv; }
  std::mutex& getMtx() { return _mtx; }

private:
  std::condition_variable _cv;
  std::mutex _mtx;
  string_t _name = "<unset>";
};
class GObject {
public:
  vec3 _pos;
  vec3 _vel;
  // Update + Render must use the same variables to test this 
  void update() {
    doWork_Using_Shared_Vars(0);
  }
  void render() {
    doWork_Using_Shared_Vars(0);
  }
  void doWork_Using_Shared_Vars(double elapsed) {
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
 // int_fast64_t _lastFrameTimeMs = 0;
  //Update + Render share data across threads.
  void render() {
    for (auto obj : _objs) {
      obj->render();
    }
  }
  void update(double elapsed) override {
    //This is running asynchronously.
    for (auto obj : _objs) {
      obj->update();
    }
  }
private:
  std::vector<std::shared_ptr<GObject>> _objs;
};

int main() {
  //This code should mimic the new asynchronous architexture.
  // Windows = Main thread

  std::shared_ptr<GScene> gs = std::make_shared<GScene>("MyScene");
  std::vector<std::shared_ptr<RenderWindow>> windows;

  int num_windows = 1;

  for (int i = 0; i < num_windows; ++i) {
    std::shared_ptr<RenderWindow> rw = std::make_shared<RenderWindow>();
    rw->setScene(gs);
    windows.push_back(rw);
  }

  gs->start();

  //Update loop
  int64_t update_msg_timer = 0;
  while (true) {
    int64_t last = getMilliseconds();
    for (auto win : windows) {
      if (!win->processInput()) {
        break;
      }
      win->renderScene();
    }
    int64_t elapsed = getMilliseconds() - last;
    update_msg_timer += elapsed;
    if (update_msg_timer > 2000) {
      std::cout << "Running.. last frame took " << elapsed << "ms" << std::endl;
    }
  }

  return 0;
}