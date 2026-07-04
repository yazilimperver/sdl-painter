#include "sdl_painter/app/application.h"

#include <SDL3/SDL.h>

#include <algorithm>
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

/// @brief Ardışık frame'ler arası maksimum delta zaman (saniye).
/// Debugger duraklaması / pencere sürüklemesi gibi büyük sıçramaları sınırlar.
constexpr float kMaxDeltaSeconds = 0.25F;

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

  SDL_GetWindowSize(mWindow, &mWidth, &mHeight);
  return true;
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

      case SDL_EVENT_WINDOW_RESIZED:
        mWidth = event.window.data1;
        mHeight = event.window.data2;
        OnResize(ResizeEvent{mWidth, mHeight});
        break;

      default:
        break;
    }
  }
}

int Application::Run() {
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
  mLastTickNs = SDL_GetTicksNS();

  while (mRunning) {
    ProcessEvents();

    const uint64_t now_ns = SDL_GetTicksNS();
    const float dt = std::min(static_cast<float>(now_ns - mLastTickNs) * 1e-9F,
                              kMaxDeltaSeconds);
    mLastTickNs = now_ns;

    OnUpdate(dt);

    mPainter->Begin();
    OnRender(*mPainter);
    mPainter->End();
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
