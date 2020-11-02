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

  * No point of running update async anyway, if we can't render
  asynchronously (without real intervention).

*/

#include "./SandboxHeader.h"
#include "./VulkanHeader.h"
#include "./GWindowHeader.h"
#include "./GWindow.h"

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
  vec3() { x = y = z = 0; }
  vec3(float dx, float dy, float dz) {
    x = dx;
    y = dy;
    z = dz;
  }
  static vec3 random() {
    return vec3(randomFloat(), randomFloat(), randomFloat());
  }
  vec3& operator=(const vec3& v) {
    this->x = v.x;
    this->y = v.y;
    this->z = v.z;
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

// Returns time equivalent to a number of frames at 60fps
#define FPS60_IN_MS(frames) ((int)((1.0f / 60.0f * frames) * 1000.0f))

void doHardWork_Frame(float f) {
  int ms = FPS60_IN_MS(f);
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

//////////////////////////////////////////////////////

class GObject;
class GScene;

//////////////////////////////////////////////////////
// VG Dummies

// Dummy
class RenderWindow {
public:
  string_t _name = "<unset>";
  RenderWindow(const string_t& name) { _name = name; }
  void setScene(std::shared_ptr<GScene> s) { _scene = s; }
  int64_t _last = 0;
  int64_t _update_msg_timer = 0;
  int64_t _render_count = 0;

  void renderScene();
  bool processInput() {
    // Dummy input to simulate some main-thread stuff
    return true;
  }

  std::shared_ptr<GScene> _scene;

private:
};

// Execute std::cout on main thread.
class Cout {
public:
  static std::deque<std::packaged_task<void()>> _msgs;
  static std::mutex _mutex;

  static void print(const string_t& str) {
    std::packaged_task<void()> task(
      [str]() { std::cout << str << std::endl; });  //!!
    //  std::future<void> result = task.get_future();
    {
      std::lock_guard<std::mutex> lock(Cout::_mutex);
      _msgs.push_back(std::move(task));
    }
    // So were in a deadlock.
    // Block until other thread
    // result.get();
  }
  static void process() {
    std::unique_lock<std::mutex> lock(_mutex);
    while (!_msgs.empty()) {
      std::packaged_task<void()> task = std::move(_msgs.front());
      _msgs.pop_front();

      // unlock during the task
      lock.unlock();
      task();
      lock.lock();
    }
  }
};
std::deque<std::packaged_task<void()>> Cout::_msgs;
std::mutex Cout::_mutex;
//////////////////////////////////////////////////////

class AsyncEngineComponent
    : public std::enable_shared_from_this<AsyncEngineComponent> {
public:
  AsyncEngineComponent(const string_t& name) { _name = name; }
  std::thread _myThread;
  void launch() {
    std::weak_ptr<AsyncEngineComponent> weak = shared_from_this();
    _myThread = std::thread([weak] {
      int64_t count = 0;

      if (std::shared_ptr<AsyncEngineComponent> ua = weak.lock()) {
        if (ua->_bTerminate == true) {
          Cout::print("Async Component " + ua->_name + " terminated." + "\n");
          return false;
        }
        std::lock_guard<decltype(ua->getMtx())> lock(ua->getMtx());
        {
          ua->update();
          count++;

          int64_t cur = VG::Gu::getMilliseconds();
          ua->elapsed += (cur - ua->last);
          ua->last = cur;
          if (ua->elapsed > 2000) {
            Cout::print(std::string() + ua->_name + " updated " +
                        std::to_string(count) + " times in 2s.." + "\n");
            ua->elapsed = 0;
            count = 0;
          }
          ua->getMtx().unlock();
        }
      }

      return true;
    });
  }
  void wait() { _myThread.join(); }

  void terminate() { _bTerminate = true; }

  std::mutex& getMtx() { return _mtx; }

protected:
  virtual void update() = 0;
  int64_t last = VG::Gu::getMilliseconds();
  int64_t elapsed = 0;

  // std::condition_variable _cv;
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

  GObject(const string_t& name) { _name = name; }
  void update() { doWork_Using_Shared_Vars(); }
  void render() {
    doWork_Using_Shared_Vars();

    //**Make the rendering frame more realistic.
    // doHardWork_Frame(.9);
  }
  void doWork_Using_Shared_Vars() {
    // pointer nonsense to make errors happen.
    if (_error_monster != nullptr) {
      _error_monster_last = *_error_monster;
      delete _error_monster;
    }
    _error_monster = new std::string();
    *_error_monster = std::string(128, 'A');
    _error_monster_last =
      _error_monster_last.substr(0, _error_monster_last.length() / 2);

    // Do some BS operation. to update position/velocity
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
  GScene(const string_t& name) : AsyncEngineComponent(name) {}
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
  void update() override {
    // This is running asynchronously.
    for (auto obj : _objs) {
      obj->update();
    }
  }

private:
  std::vector<std::shared_ptr<GObject>> _objs;
};
void RenderWindow::renderScene() {
  int64_t elapsed = (VG::Gu::getMilliseconds() - _last);
  if (elapsed > 2000) {
    Cout::print(_name + " rendered " + std::to_string(_render_count) +
                " times in 2s.. last window family frame took " +
                std::to_string(elapsed) + "ms" + "\n");
    _last = VG::Gu::getMilliseconds();
    _render_count = 0;
  }
  if (_scene) {
    std::lock_guard<std::mutex> guard(_scene->getMtx());
    _scene->render();

    doHardWork_Frame(1);  // Take up exactly 60fps.

    _render_count++;
  }
}

void asyncSceneTest() {
#define CRAZINESS 20

  std::cout << "starting test.." << std::endl;
  // Windows = Main thread
  std::vector<std::shared_ptr<GScene>> scenes;
  std::vector<std::shared_ptr<RenderWindow>> windows;

  for (int i = 0; i < CRAZINESS; ++i) {
    scenes.push_back(std::make_shared<GScene>(std::string("") + "Scene" +
                                              std::to_string(i)));
    for (int iObj = 0; iObj < 10; ++iObj) {
      scenes[scenes.size() - 1]->addObject(std::make_shared<GObject>(
        std::string("") + "Scene" + std::to_string(i) + "_obj" +
        std::to_string(iObj)));
    }

    std::shared_ptr<RenderWindow> rw = std::make_shared<RenderWindow>(
      std::string("") + "Window" + std::to_string(i));
    rw->setScene(scenes[scenes.size() - 1]);
    windows.push_back(rw);
  }
  // Update loop
  while (true) {
    bool window_requested_exit = false;

    for (auto win : windows) {
      if (!win->processInput()) {
        break;
      }
      win->_scene->launch();
    }
    // fence.
    for (auto win : windows) {
      win->_scene->wait();
    }
    for (auto win : windows) {
      win->renderScene();
    }
    Cout::process();
  }
  std::cout << "..Done" << std::endl;
}

int main(int argc, char** argv) {
  VG::App::_appRoot.assign(VG::App::getDirectoryNameFromPath(argv[0]));

  BRLogInfo("Creating vulkan.");

  VG::GSDL sv;
  try {
    sv.init();
    sv.start();
    sv.renderLoop();
    //std::shared_ptr<VG::GWindow> win1 = sv.createWindow();
    //std::shared_ptr<VG::GWindow> win2 = sv.createWindow();
  }
  catch (std::exception& ex) {
  }
  //TODO:
  //catch (VG::Exception& ex) {
  //}

  //sv.init();


  //asyncSceneTest();

  return 0;
}