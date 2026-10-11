// Application davranış testleri.
//
// İki bölüm var:
//  - Zamanlama hesabı (app_detail, frame_timing.h) ve Run öncesi erişimciler:
//    GPU'suz, saf birim testleri. Gerçek uyku kullanılmaz.
//  - Run() üzerinden yaşam döngüsü ve olay iletimi: gerçek pencere ve backend
//    kuran entegrasyon testleridir. Backend kurulamazsa
//    SDLPAINTER_BACKEND_UNAVAILABLE kuralına uyar. Olaylar OnInit içinde SDL
//    kuyruğuna sentetik olarak itilir ve ilk karede işlenir.

#include "app/frame_timing.h"

#include "sdl_painter/app/app_config.h"
#include "sdl_painter/app/application.h"
#include "sdl_painter/app/events.h"
#include "sdl_painter/frame_stats.h"
#include "sdl_painter/painter.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <functional>
#include <gtest/gtest.h>
#include <string>
#include <utility>
#include <vector>

#include "pixel_test_support.h"
#include "test_support.h"

using sdl_painter::AppConfig;
using sdl_painter::Application;
using sdl_painter::FrameStats;
using sdl_painter::Key;
using sdl_painter::KeyEvent;
using sdl_painter::KeyModifier;
using sdl_painter::MouseButton;
using sdl_painter::MouseButtonEvent;
using sdl_painter::MouseMoveEvent;
using sdl_painter::MouseWheelEvent;
using sdl_painter::Painter;
using sdl_painter::RendererBackend;
using sdl_painter::StatsOverlayMode;
using sdl_painter::TimingMode;
using sdl_painter::testing::AvailableBackends;
using sdl_painter::testing::HiddenWindow;
using sdl_painter::testing::ValidationGuard;

namespace timing = sdl_painter::app_detail;

namespace {

constexpr uint64_t kMs = 1'000'000ULL;

}  // namespace

// --- Zamanlama hesabı ---

TEST(FrameTiming, VariableModeOrNonPositiveRateHasNoFixedStep) {
  EXPECT_EQ(timing::FixedStepNs(TimingMode::kVariable, 60), 0U);
  EXPECT_EQ(timing::FixedStepNs(TimingMode::kFixed, 0), 0U);
  EXPECT_EQ(timing::FixedStepNs(TimingMode::kFixed, -30), 0U);
  EXPECT_EQ(timing::FixedStepNs(TimingMode::kFixed, 50), 20 * kMs);
  EXPECT_EQ(timing::FixedStepNs(TimingMode::kFixed, 60), 16'666'666U);
}

TEST(FrameTiming, DeltaIsClampedAtQuarterSecond) {
  EXPECT_EQ(timing::ClampFrameDelta(0), 0U);
  EXPECT_EQ(timing::ClampFrameDelta(250 * kMs - 1), 250 * kMs - 1);
  EXPECT_EQ(timing::ClampFrameDelta(250 * kMs), 250 * kMs);
  EXPECT_EQ(timing::ClampFrameDelta(250 * kMs + 1), 250 * kMs);
  EXPECT_EQ(timing::ClampFrameDelta(10'000 * kMs), 250 * kMs);
}

TEST(FrameTiming, NoStepUntilLagReachesOneStep) {
  const timing::FixedStepResult r =
      timing::AdvanceFixedStep(0, 9 * kMs, 10 * kMs);
  EXPECT_EQ(r.steps, 0);
  EXPECT_EQ(r.lag_ns, 9 * kMs);
  EXPECT_FLOAT_EQ(r.alpha, 0.9F);
}

TEST(FrameTiming, ExactStepBoundaryConsumesTheStep) {
  const timing::FixedStepResult r =
      timing::AdvanceFixedStep(4 * kMs, 6 * kMs, 10 * kMs);
  EXPECT_EQ(r.steps, 1);
  EXPECT_EQ(r.lag_ns, 0U);
  EXPECT_FLOAT_EQ(r.alpha, 0.0F);
}

TEST(FrameTiming, SeveralStepsKeepTheRemainder) {
  const timing::FixedStepResult r =
      timing::AdvanceFixedStep(0, 35 * kMs, 10 * kMs);
  EXPECT_EQ(r.steps, 3);
  EXPECT_EQ(r.lag_ns, 5 * kMs);
  EXPECT_FLOAT_EQ(r.alpha, 0.5F);
}

TEST(FrameTiming, JustBelowStepCapKeepsTheRemainder) {
  const timing::FixedStepResult r =
      timing::AdvanceFixedStep(0, 49 * kMs, 10 * kMs);
  EXPECT_EQ(r.steps, 4);
  EXPECT_EQ(r.lag_ns, 9 * kMs);
}

TEST(FrameTiming, ReachingStepCapDropsTheRemainder) {
  // 57 ms = 5 adım + 7 ms; sınıra ulaşıldığı için 7 ms düşürülür.
  const timing::FixedStepResult r =
      timing::AdvanceFixedStep(0, 57 * kMs, 10 * kMs);
  EXPECT_EQ(r.steps, timing::kMaxFixedStepsPerFrame);
  EXPECT_EQ(r.lag_ns, 0U);
  EXPECT_FLOAT_EQ(r.alpha, 0.0F);
}

TEST(FrameTiming, LagCarriesOverBetweenFrames) {
  timing::FixedStepResult r = timing::AdvanceFixedStep(0, 6 * kMs, 10 * kMs);
  EXPECT_EQ(r.steps, 0);
  r = timing::AdvanceFixedStep(r.lag_ns, 6 * kMs, 10 * kMs);
  EXPECT_EQ(r.steps, 1);
  EXPECT_EQ(r.lag_ns, 2 * kMs);
}

TEST(FrameTiming, ClampedSpikeYieldsAtMostCapSteps) {
  const uint64_t step = timing::FixedStepNs(TimingMode::kFixed, 60);
  const timing::FixedStepResult r = timing::AdvanceFixedStep(
      0, timing::ClampFrameDelta(10'000 * kMs), step);
  EXPECT_EQ(r.steps, timing::kMaxFixedStepsPerFrame);
  EXPECT_EQ(r.lag_ns, 0U);
}

TEST(FrameTiming, AlphaStaysInUnitRange) {
  uint64_t lag = 0;
  for (uint64_t elapsed = 0; elapsed < 40 * kMs; elapsed += 3 * kMs) {
    const timing::FixedStepResult r =
        timing::AdvanceFixedStep(lag, elapsed, 10 * kMs);
    EXPECT_GE(r.alpha, 0.0F);
    EXPECT_LT(r.alpha, 1.0F);
    lag = r.lag_ns;
  }
}

// --- Run öncesi erişimciler (GPU'suz) ---

TEST(ApplicationAccessors, BeforeRunReportEmptyState) {
  AppConfig config;
  config.stats_overlay = StatsOverlayMode::kFps;
  config.init_logger = false;
  Application app(config);

  EXPECT_DOUBLE_EQ(app.Fps(), 0.0);
  const FrameStats& stats = app.GetFrameStats();
  EXPECT_EQ(stats.draw_calls, 0U);
  EXPECT_EQ(stats.batches, 0U);
  EXPECT_EQ(stats.vertices, 0U);
  EXPECT_EQ(stats.state_changes, 0U);
  EXPECT_EQ(app.Width(), 0);
  EXPECT_EQ(app.Height(), 0);
  EXPECT_EQ(app.GetWindow(), nullptr);
  EXPECT_EQ(app.GetStatsOverlay(), StatsOverlayMode::kFps);

  app.SetStatsOverlay(StatsOverlayMode::kDetailed);
  EXPECT_EQ(app.GetStatsOverlay(), StatsOverlayMode::kDetailed);
}

// --- Run: yaşam döngüsü ve olay iletimi (gerçek backend) ---

namespace {

AppConfig TestConfig(RendererBackend backend) {
  AppConfig config;
  config.title = "sdl_painter test";
  config.backend = backend;
  config.width = 64;
  config.height = 48;
  config.resizable = false;
  config.high_dpi = false;  // Yoğunluk 1: fare değerleri birebir gelsin.
  config.vsync = false;
  config.init_logger = false;
  return config;
}

/// @brief Çağrıları kaydeden ve belirli sayıda kareden sonra çıkan uygulama.
class RecordingApp : public Application {
 public:
  explicit RecordingApp(AppConfig config) : Application(std::move(config)) {}

  bool init_result{true};
  int32_t frames_before_quit{1};
  std::function<void(RecordingApp&)> push_events;
  std::function<bool(const SDL_Event&)> consume;

  std::vector<std::string> log;
  std::vector<KeyEvent> key_downs;
  std::vector<StatsOverlayMode> mode_at_key_down;
  std::vector<KeyEvent> key_ups;
  std::vector<MouseButtonEvent> button_downs;
  std::vector<MouseButtonEvent> button_ups;
  std::vector<MouseMoveEvent> moves;
  std::vector<MouseWheelEvent> wheels;

 protected:
  bool OnInit() override {
    log.emplace_back("init");
    if (push_events) {
      push_events(*this);
    }
    return init_result;
  }
  void OnUpdate(float /*dt*/) override { log.emplace_back("update"); }
  void OnRender(Painter& /*painter*/, float /*alpha*/) override {
    log.emplace_back("render");
    if (--frames_before_quit <= 0) {
      Quit();
    }
  }
  void OnKeyDown(const KeyEvent& event) override {
    key_downs.push_back(event);
    mode_at_key_down.push_back(GetStatsOverlay());
  }
  void OnKeyUp(const KeyEvent& event) override { key_ups.push_back(event); }
  void OnMouseButtonDown(const MouseButtonEvent& event) override {
    button_downs.push_back(event);
  }
  void OnMouseButtonUp(const MouseButtonEvent& event) override {
    button_ups.push_back(event);
  }
  void OnMouseMove(const MouseMoveEvent& event) override {
    moves.push_back(event);
  }
  void OnMouseWheel(const MouseWheelEvent& event) override {
    wheels.push_back(event);
  }
  bool OnRawEvent(const SDL_Event& event) override {
    return consume ? consume(event) : false;
  }
  void OnShutdown() override { log.emplace_back("shutdown"); }
};

void PushKeyDown(SDL_Window* window, SDL_Keycode key, bool repeat,
                 SDL_Keymod mod = SDL_KMOD_NONE) {
  SDL_Event e{};
  e.type = SDL_EVENT_KEY_DOWN;
  e.key.windowID = SDL_GetWindowID(window);
  e.key.key = key;
  e.key.mod = mod;
  e.key.down = true;
  e.key.repeat = repeat;
  EXPECT_TRUE(SDL_PushEvent(&e)) << SDL_GetError();
}

void PushKeyUp(SDL_Window* window, SDL_Keycode key, SDL_Keymod mod) {
  SDL_Event e{};
  e.type = SDL_EVENT_KEY_UP;
  e.key.windowID = SDL_GetWindowID(window);
  e.key.key = key;
  e.key.mod = mod;
  EXPECT_TRUE(SDL_PushEvent(&e)) << SDL_GetError();
}

/// @brief Run'ın sonucunu backend kullanılabilirliği açısından yorumla.
///
/// `Initialize` başarısızsa `OnInit` hiç çağrılmaz; bu durum pencere/backend
/// kurulamadığı anlamına gelir (bkz. SDLPAINTER_BACKEND_UNAVAILABLE).
bool InitializeFailed(const RecordingApp& app) { return app.log.empty(); }

class ApplicationRun : public ::testing::TestWithParam<RendererBackend> {};

}  // namespace

TEST_P(ApplicationRun, SuccessfulRunCallsInitUpdateRenderShutdownInOrder) {
  if (HiddenWindow(GetParam()).Get() == nullptr) {
    SDLPAINTER_BACKEND_UNAVAILABLE("Pencere olusturulamadi.");
  }
  const ValidationGuard guard;
  RecordingApp app(TestConfig(GetParam()));
  const int kResult = app.Run();
  if (InitializeFailed(app)) {
    SDLPAINTER_BACKEND_UNAVAILABLE("Application baslatilamadi.");
  }

  EXPECT_EQ(kResult, 0);
  EXPECT_EQ(app.log, (std::vector<std::string>{"init", "update", "render",
                                               "shutdown"}));
}

TEST_P(ApplicationRun, SecondRunIsRejectedWithoutCallbacks) {
  if (HiddenWindow(GetParam()).Get() == nullptr) {
    SDLPAINTER_BACKEND_UNAVAILABLE("Pencere olusturulamadi.");
  }
  const ValidationGuard guard;
  RecordingApp app(TestConfig(GetParam()));
  ASSERT_EQ(app.Run(), 0);
  if (InitializeFailed(app)) {
    SDLPAINTER_BACKEND_UNAVAILABLE("Application baslatilamadi.");
  }
  const std::vector<std::string> kAfterFirst = app.log;

  EXPECT_EQ(app.Run(), 1);
  EXPECT_EQ(app.log, kAfterFirst) << "ikinci Run callback cagirdi";
}

TEST_P(ApplicationRun, FailedOnInitSkipsLoopAndShutsDownOnce) {
  if (HiddenWindow(GetParam()).Get() == nullptr) {
    SDLPAINTER_BACKEND_UNAVAILABLE("Pencere olusturulamadi.");
  }
  const ValidationGuard guard;
  RecordingApp app(TestConfig(GetParam()));
  app.init_result = false;
  const int kResult = app.Run();
  if (InitializeFailed(app)) {
    SDLPAINTER_BACKEND_UNAVAILABLE("Application baslatilamadi.");
  }

  EXPECT_EQ(kResult, 1);
  EXPECT_EQ(app.log, (std::vector<std::string>{"init", "shutdown"}));
}

INSTANTIATE_TEST_SUITE_P(
    Backends, ApplicationRun, ::testing::ValuesIn(AvailableBackends()),
    [](const ::testing::TestParamInfo<RendererBackend>& i) {
      return i.param == RendererBackend::kOpenGL ? "OpenGL" : "Vulkan";
    });

// Olay iletimi backend'den bağımsızdır; OpenGL ile sınanır.

TEST(ApplicationEvents, StatsOverlayKeyCyclesOnFirstKeyDownAndReachesApp) {
  if (HiddenWindow(RendererBackend::kOpenGL).Get() == nullptr) {
    SDLPAINTER_BACKEND_UNAVAILABLE("OpenGL penceresi olusturulamadi.");
  }
  AppConfig config = TestConfig(RendererBackend::kOpenGL);
  config.stats_overlay_key = Key::kF1;
  RecordingApp app(config);
  app.push_events = [](RecordingApp& a) {
    SDL_Window* w = a.GetWindow();
    PushKeyDown(w, SDLK_F1, false);  // kapalı -> FPS
    PushKeyDown(w, SDLK_F1, true);   // tekrar: değişmez
    PushKeyDown(w, 0, false);        // bilinmeyen tuş: değişmez
    PushKeyDown(w, SDLK_F1, false);  // FPS -> ayrıntılı
    PushKeyDown(w, SDLK_F1, false);  // ayrıntılı -> kapalı
  };
  ASSERT_EQ(app.Run(), 0);
  if (InitializeFailed(app)) {
    SDLPAINTER_BACKEND_UNAVAILABLE("Application baslatilamadi.");
  }

  // Çatı tuşu olayı tüketmez: beşi de uygulamaya ulaşır.
  ASSERT_EQ(app.key_downs.size(), 5U);
  EXPECT_EQ(app.key_downs[0].key, Key::kF1);
  EXPECT_TRUE(app.key_downs[1].repeat);
  EXPECT_EQ(app.key_downs[2].key, Key::kUnknown);
  EXPECT_EQ(app.mode_at_key_down,
            (std::vector<StatsOverlayMode>{
                StatsOverlayMode::kFps, StatsOverlayMode::kFps,
                StatsOverlayMode::kFps, StatsOverlayMode::kDetailed,
                StatsOverlayMode::kNone}));
  EXPECT_EQ(app.GetStatsOverlay(), StatsOverlayMode::kNone);
}

TEST(ApplicationEvents, UnknownKeyDoesNotCycleWhenOverlayKeyIsDisabled) {
  if (HiddenWindow(RendererBackend::kOpenGL).Get() == nullptr) {
    SDLPAINTER_BACKEND_UNAVAILABLE("OpenGL penceresi olusturulamadi.");
  }
  AppConfig config = TestConfig(RendererBackend::kOpenGL);
  config.stats_overlay_key = Key::kUnknown;  // gösterge tuşu devre dışı
  RecordingApp app(config);
  app.push_events = [](RecordingApp& a) {
    PushKeyDown(a.GetWindow(), 0, false);
  };
  ASSERT_EQ(app.Run(), 0);
  if (InitializeFailed(app)) {
    SDLPAINTER_BACKEND_UNAVAILABLE("Application baslatilamadi.");
  }

  ASSERT_EQ(app.key_downs.size(), 1U);
  EXPECT_EQ(app.key_downs[0].key, Key::kUnknown);
  EXPECT_EQ(app.GetStatsOverlay(), StatsOverlayMode::kNone);
}

TEST(ApplicationEvents, ConsumedRawEventSkipsTypedCallbackAndOverlayKey) {
  if (HiddenWindow(RendererBackend::kOpenGL).Get() == nullptr) {
    SDLPAINTER_BACKEND_UNAVAILABLE("OpenGL penceresi olusturulamadi.");
  }
  RecordingApp app(TestConfig(RendererBackend::kOpenGL));
  app.consume = [](const SDL_Event& e) {
    return e.type == SDL_EVENT_KEY_DOWN &&
           (e.key.key == SDLK_A || e.key.key == SDLK_F1);
  };
  app.push_events = [](RecordingApp& a) {
    SDL_Window* w = a.GetWindow();
    PushKeyDown(w, SDLK_A, false);   // tüketilir
    PushKeyDown(w, SDLK_F1, false);  // tüketilir: gösterge değişmez
    PushKeyDown(w, SDLK_B, false);   // iletilir
  };
  ASSERT_EQ(app.Run(), 0);
  if (InitializeFailed(app)) {
    SDLPAINTER_BACKEND_UNAVAILABLE("Application baslatilamadi.");
  }

  ASSERT_EQ(app.key_downs.size(), 1U);
  EXPECT_EQ(app.key_downs[0].key, Key::kB);
  EXPECT_EQ(app.GetStatsOverlay(), StatsOverlayMode::kNone);
}

TEST(ApplicationEvents, KeyAndMouseFieldsAreForwarded) {
  if (HiddenWindow(RendererBackend::kOpenGL).Get() == nullptr) {
    SDLPAINTER_BACKEND_UNAVAILABLE("OpenGL penceresi olusturulamadi.");
  }
  RecordingApp app(TestConfig(RendererBackend::kOpenGL));
  app.push_events = [](RecordingApp& a) {
    SDL_Window* w = a.GetWindow();
    const SDL_WindowID kId = SDL_GetWindowID(w);
    PushKeyUp(w, SDLK_B, SDL_KMOD_LSHIFT);

    SDL_Event e{};
    e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    e.button.windowID = kId;
    e.button.button = SDL_BUTTON_RIGHT;
    e.button.down = true;
    e.button.clicks = 2;
    e.button.x = 10.0F;
    e.button.y = 20.0F;
    EXPECT_TRUE(SDL_PushEvent(&e));

    e = SDL_Event{};
    e.type = SDL_EVENT_MOUSE_BUTTON_UP;
    e.button.windowID = kId;
    e.button.button = SDL_BUTTON_LEFT;
    e.button.clicks = 1;
    e.button.x = 11.0F;
    e.button.y = 21.0F;
    EXPECT_TRUE(SDL_PushEvent(&e));

    e = SDL_Event{};
    e.type = SDL_EVENT_MOUSE_MOTION;
    e.motion.windowID = kId;
    e.motion.x = 30.0F;
    e.motion.y = 31.0F;
    e.motion.xrel = -1.5F;
    e.motion.yrel = 2.5F;
    EXPECT_TRUE(SDL_PushEvent(&e));

    e = SDL_Event{};
    e.type = SDL_EVENT_MOUSE_WHEEL;
    e.wheel.windowID = kId;
    e.wheel.x = 1.0F;
    e.wheel.y = -2.0F;
    e.wheel.mouse_x = 5.0F;
    e.wheel.mouse_y = 6.0F;
    EXPECT_TRUE(SDL_PushEvent(&e));
  };
  ASSERT_EQ(app.Run(), 0);
  if (InitializeFailed(app)) {
    SDLPAINTER_BACKEND_UNAVAILABLE("Application baslatilamadi.");
  }

  ASSERT_EQ(app.key_ups.size(), 1U);
  EXPECT_EQ(app.key_ups[0].key, Key::kB);
  EXPECT_EQ(app.key_ups[0].modifiers, KeyModifier::kShift);

  // SDL kendi fare olaylarını da üretebilir; itilen değerler aranır.
  bool found_down = false;
  for (const auto& b : app.button_downs) {
    found_down = found_down || (b.button == MouseButton::kRight &&
                                b.x == 10.0F && b.y == 20.0F && b.clicks == 2);
  }
  EXPECT_TRUE(found_down);
  bool found_up = false;
  for (const auto& b : app.button_ups) {
    found_up = found_up || (b.button == MouseButton::kLeft && b.x == 11.0F &&
                            b.y == 21.0F && b.clicks == 1);
  }
  EXPECT_TRUE(found_up);
  bool found_move = false;
  for (const auto& m : app.moves) {
    found_move = found_move || (m.x == 30.0F && m.y == 31.0F &&
                                m.dx == -1.5F && m.dy == 2.5F);
  }
  EXPECT_TRUE(found_move);
  bool found_wheel = false;
  for (const auto& w : app.wheels) {
    found_wheel = found_wheel || (w.dx == 1.0F && w.dy == -2.0F &&
                                  w.mouse_x == 5.0F && w.mouse_y == 6.0F);
  }
  EXPECT_TRUE(found_wheel);
}
