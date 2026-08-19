#include "sdl_painter/app/application.h"

#include <SDL3/SDL.h>

#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/spdlog.h>

#include "app/event_translator.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace sdl_painter {

namespace {

/// @brief spdlog varsayılan logger'ını kur (renkli konsol + Win32 VT modu).
void InitDefaultLogger() {
#ifdef _WIN32
  HANDLE h_out = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode = 0;
  if (GetConsoleMode(h_out, &mode)) {
    SetConsoleMode(h_out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
  }
#endif
  auto sink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
  sink->set_pattern("[%H:%M:%S][%L] %v");
  auto logger = std::make_shared<spdlog::logger>("sdlpainter", sink);
  logger->set_level(spdlog::level::trace);
  spdlog::set_default_logger(logger);
}

/// @brief Ardışık frame'ler arası azami delta zaman (nanosaniye).
/// Debugger duraklaması / pencere sürüklemesi gibi büyük sıçramaları sınırlar
/// ("spiral of death" koruması).
constexpr uint64_t kMaxDeltaNs = 250'000'000ULL;  // 0.25 s

/// @brief kFixed modda bir frame'de izin verilen azami sabit güncelleme sayısı.
constexpr int32_t kMaxFixedStepsPerFrame = 5;

/// @brief Saniyedeki nanosaniye.
constexpr uint64_t kNsPerSecond = 1'000'000'000ULL;

}  // namespace

Application::Application(AppConfig config) : mConfig(std::move(config)) {}

Application::~Application() {
  Teardown();
}

bool Application::Initialize() {
  if (mConfig.init_logger) {
    InitDefaultLogger();
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    spdlog::error("SDL_Init başarısız: {}", SDL_GetError());
    return false;
  }
  mSdlInitialized = true;

  // OpenGL: pencere piksel formatını etkileyen attribute'lar pencereden
  // ÖNCE ayarlanmalı. Context sürüm attribute'larını renderer da set eder
  // (tekrar zararsız); MSAA yalnızca burada ayarlanabilir.
  if (mConfig.backend == RendererBackend::kOpenGL) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
    if (mConfig.msaa_samples > 0) {
      SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
      SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, mConfig.msaa_samples);
    }
  }

  SDL_WindowFlags flags = (mConfig.backend == RendererBackend::kVulkan)
                              ? SDL_WINDOW_VULKAN
                              : SDL_WINDOW_OPENGL;
  if (mConfig.resizable) {
    flags |= SDL_WINDOW_RESIZABLE;
  }
  if (mConfig.high_dpi) {
    flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
  }

  mWindow = SDL_CreateWindow(mConfig.title.c_str(), mConfig.width,
                             mConfig.height, flags);
  if (mWindow == nullptr) {
    spdlog::error("SDL_CreateWindow başarısız: {}", SDL_GetError());
    return false;
  }

  mPainter = std::make_unique<Painter>(mWindow, mConfig.backend);
  if (!mPainter->IsValid()) {
    spdlog::error("Painter başlatılamadı.");
    return false;
  }

  // vsync: GL context artık Painter tarafından oluşturuldu ve aktif.
  if (mConfig.backend == RendererBackend::kOpenGL) {
    SDL_GL_SetSwapInterval(mConfig.vsync ? 1 : 0);
  } else if (mConfig.vsync) {
    spdlog::debug(
        "vsync yapılandırması Vulkan'da yoksayıldı "
        "(sunum modu swapchain tarafından seçilir).");
  }

  // Width()/Height() daima framebuffer (piksel) boyutunu bildirir; Painter
  // koordinat sistemi de piksel tabanlıdır (bkz. AppConfig::high_dpi).
  UpdateDrawableSize();
  // Boyut yonetimini olay tabanli yola devret; Painter artik her karede
  // pencereyi yoklamaz.
  mPainter->SetDrawableSize(mWidth, mHeight);
  return true;
}

void Application::UpdateDrawableSize() {
  if (mWindow == nullptr) {
    return;
  }
  int w = 0;
  int h = 0;
  SDL_GetWindowSizeInPixels(mWindow, &w, &h);
  mWidth = w;
  mHeight = h;
}

void Application::Teardown() noexcept {
  mPainter.reset();
  if (mWindow != nullptr) {
    SDL_DestroyWindow(mWindow);
    mWindow = nullptr;
  }
  if (mSdlInitialized) {
    SDL_Quit();
    mSdlInitialized = false;
  }
}

void Application::ProcessEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (OnRawEvent(event)) {
      continue;  // Olay tüketildi.
    }

    switch (event.type) {
      case SDL_EVENT_QUIT:
        Quit();
        break;

      case SDL_EVENT_KEY_DOWN:
        OnKeyDown(KeyEvent{internal::TranslateKey(event.key.key),
                           internal::TranslateModifiers(event.key.mod),
                           event.key.repeat});
        break;

      case SDL_EVENT_KEY_UP:
        OnKeyUp(KeyEvent{internal::TranslateKey(event.key.key),
                         internal::TranslateModifiers(event.key.mod),
                         event.key.repeat});
        break;

      case SDL_EVENT_MOUSE_BUTTON_DOWN:
        OnMouseButtonDown(MouseButtonEvent{
            internal::TranslateMouseButton(event.button.button), event.button.x,
            event.button.y, event.button.clicks});
        break;

      case SDL_EVENT_MOUSE_BUTTON_UP:
        OnMouseButtonUp(MouseButtonEvent{
            internal::TranslateMouseButton(event.button.button), event.button.x,
            event.button.y, event.button.clicks});
        break;

      case SDL_EVENT_MOUSE_MOTION:
        OnMouseMove(MouseMoveEvent{event.motion.x, event.motion.y,
                                   event.motion.xrel, event.motion.yrel});
        break;

      case SDL_EVENT_MOUSE_WHEEL:
        OnMouseWheel(MouseWheelEvent{event.wheel.x, event.wheel.y,
                                     event.wheel.mouse_x, event.wheel.mouse_y});
        break;

      // Piksel boyutu değişimi hem yeniden boyutlandırmada hem de pencere
      // farklı ölçek faktörlü bir ekrana taşındığında tetiklenir. Mantıksal
      // SDL_EVENT_WINDOW_RESIZED yerine bunu dinlemek, HiDPI'da doğru olan.
      case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        mWidth = event.window.data1;
        mHeight = event.window.data2;
        // Painter'a acikca bildir: boylece kare basina pencere boyutu
        // yoklamasi yapmasina gerek kalmaz (bkz. Painter::SetDrawableSize).
        if (mPainter != nullptr) {
          mPainter->SetDrawableSize(mWidth, mHeight);
        }
        OnResize(ResizeEvent{mWidth, mHeight});
        break;

      default:
        break;
    }
  }
}

int Application::Run() {
  // Run tek seferliktir: ikinci cagri SDL_Init ve pencere olusturmayi
  // tekrarlayip oncekini sizdirirdi.
  if (mHasRun) {
    spdlog::error("Application::Run() birden fazla kez cagrildi; yoksayildi.");
    return 1;
  }
  mHasRun = true;

  if (!Initialize()) {
    Teardown();
    return 1;
  }
  if (!OnInit()) {
    OnShutdown();
    Teardown();
    return 1;
  }

  mRunning = true;

  // kFixed modda sabit adım süresi; kVariable'da 0 (devre dışı).
  const uint64_t fixed_step_ns =
      (mConfig.timing == TimingMode::kFixed && mConfig.fixed_update_hz > 0)
          ? kNsPerSecond / static_cast<uint64_t>(mConfig.fixed_update_hz)
          : 0;
  const bool fixed = fixed_step_ns > 0;
  const float fixed_step_dt =
      fixed ? 1.0F / static_cast<float>(mConfig.fixed_update_hz) : 0.0F;
  // target_fps freni için asgari kare süresi; 0 = fren yok.
  const uint64_t frame_min_ns =
      (mConfig.target_fps > 0)
          ? kNsPerSecond / static_cast<uint64_t>(mConfig.target_fps)
          : 0;

  uint64_t lag_ns = 0;
  mLastTickNs = SDL_GetTicksNS();

  while (mRunning) {
    const uint64_t frame_start_ns = SDL_GetTicksNS();
    ProcessEvents();

    const uint64_t now_ns = SDL_GetTicksNS();
    uint64_t elapsed_ns = now_ns - mLastTickNs;
    mLastTickNs = now_ns;
    if (elapsed_ns > kMaxDeltaNs) {
      elapsed_ns = kMaxDeltaNs;
    }

    float alpha = 1.0F;
    if (fixed) {
      lag_ns += elapsed_ns;
      int32_t steps = 0;
      while (lag_ns >= fixed_step_ns && steps < kMaxFixedStepsPerFrame) {
        OnUpdate(fixed_step_dt);
        lag_ns -= fixed_step_ns;
        ++steps;
      }
      // Catch-up üst sınırına ulaşıldıysa biriken artık zamanı düşür.
      if (steps == kMaxFixedStepsPerFrame) {
        lag_ns = 0;
      }
      alpha = static_cast<float>(lag_ns) / static_cast<float>(fixed_step_ns);
    } else {
      OnUpdate(static_cast<float>(elapsed_ns) * 1e-9F);
    }

    mPainter->Begin();
    OnRender(*mPainter, alpha);
    mPainter->End();

    // Hedef FPS freni (vsync yoksa/kapalıysa etkin).
    if (frame_min_ns > 0) {
      const uint64_t frame_ns = SDL_GetTicksNS() - frame_start_ns;
      if (frame_ns < frame_min_ns) {
        SDL_DelayNS(frame_min_ns - frame_ns);
      }
    }
  }

  OnShutdown();
  return 0;  // Kaynak yıkımı ~Application içinde (Painter, alt sınıf
             // üyelerinden sonra yok edilsin diye).
}

void Application::Quit() noexcept {
  mRunning = false;
}

void Application::SetTitle(const std::string& title) {
  if (mWindow != nullptr) {
    SDL_SetWindowTitle(mWindow, title.c_str());
  }
}

}  // namespace sdl_painter
