#pragma once

#include "sdl_painter/renderer.h"

#include <SDL3/SDL.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <gtest/gtest.h>
#include <string>

#ifdef SDLPAINTER_HAS_VULKAN
#include "vulkan/vk_context.h"
#endif

namespace sdl_painter::testing {

/// @brief SDL video alt sistemi + gizli pencere için RAII sarmalayıcı.
///
/// Gizli pencere (`SDL_WINDOW_HIDDEN`) kullanılır; test koşarken ekranda
/// pencere belirmez ama gerçek bir sürücü context'i oluşturulur.
///
/// Pencere/context oluşturulamayan ortamlarda `Get()` `nullptr` döner ve
/// çağıran testi `GTEST_SKIP()` ile atlar (`SDLPAINTER_REQUIRE_BACKEND`
/// ayarlıysa düşürür, bkz. `BackendIsRequired()`):
/// - Linux: `SDL_VIDEODRIVER=offscreen` + Mesa EGL ile çalışır (CI böyle).
/// - Windows: `offscreen` sürücüsü EGL yükleyemiyor; gerçek sürücü üzerinden
///   çalışır, masaüstü oturumu yoksa atlanır.
class HiddenWindow {
 public:
  explicit HiddenWindow(RendererBackend backend, int32_t width = 64,
                        int32_t height = 64) {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
      mError = SDL_GetError();
      return;
    }
    mVideoInitialized = true;

    // Uygulama çatısıyla aynı sıra: GL attribute'ları pencereden once.
    if (backend == RendererBackend::kOpenGL) {
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                          SDL_GL_CONTEXT_PROFILE_CORE);
    }

    const SDL_WindowFlags kBackendFlag = (backend == RendererBackend::kVulkan)
                                             ? SDL_WINDOW_VULKAN
                                             : SDL_WINDOW_OPENGL;
    mWindow = SDL_CreateWindow("sdl_painter test", width, height,
                               kBackendFlag | SDL_WINDOW_HIDDEN);
    if (mWindow == nullptr) {
      mError = SDL_GetError();
    }
  }

  ~HiddenWindow() {
    if (mWindow != nullptr) {
      SDL_DestroyWindow(mWindow);
    }
    if (mVideoInitialized) {
      SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
  }

  HiddenWindow(const HiddenWindow&) = delete;
  HiddenWindow& operator=(const HiddenWindow&) = delete;

  [[nodiscard]] SDL_Window* Get() const { return mWindow; }
  [[nodiscard]] const char* Error() const { return mError; }

 private:
  SDL_Window* mWindow{nullptr};
  bool mVideoInitialized{false};
  const char* mError{""};
};

/// @brief Vulkan bağlamı validation'sız kurulduğunda test düşmeli mi?
///
/// `ValidationGuard` validation hatalarını ancak validation etkinken görür.
/// Katman kurulu değilse (ör. CI imajında paket yoksa) hata sayacı hiç artmaz
/// ve Vulkan testleri "hata yok" diye sessizce geçer.
///
/// `SDLPAINTER_REQUIRE_VALIDATION=1` ayarlandığında koruyucu, kapsamında
/// validation'ı tam etkin olmayan bir Vulkan bağlamı kurulduysa testi düşürür
/// (bkz. `vk_detail::ValidationState`). Yalnızca Vulkan bağlamı kuran testler
/// etkilenir; OpenGL testleri ve Vulkan'sız derleme etkilenmez. CI bunu
/// yalnızca Debug + Vulkan Linux işlerinde ayarlar.
///
/// Release derlemede validation tasarım gereği kapalıdır; değişken yine de
/// ayarlıysa test atlanmaz, açık mesajla düşer. Değişken "bu koşu doğrulanmış
/// olmalı" iddiasıdır: atlamak aynı sessizliği geri getirirdi, üstelik
/// koruyucu test gövdesi koştuktan sonra yıkıldığı için atlama anlamsızdır.
inline bool ValidationIsRequired() {
  const char* env = std::getenv("SDLPAINTER_REQUIRE_VALIDATION");
  return env != nullptr && env[0] != '\0' && env[0] != '0';
}

/// @brief Bir blok boyunca yeni Vulkan validation hatası çıkmadığını doğrular.
///
/// Validation mesajları yalnızca log'a yazılıyor, testleri düşürmüyordu;
/// yeşil bir takımın altında gerçek spec ihlalleri birikebiliyordu (render
/// target çalışmasında bir render-pass uyumsuzluğu böyle ortaya çıktı).
/// Bu koruyucu, kapsamı bittiğinde sayaç arttıysa testi düşürür.
///
/// Vulkan derlenmemişse veya validation katmanı yoksa sayaç sabit kalır ve
/// koruyucu hiçbir şey yapmaz; yani OpenGL-only ortamda zararsızdır.
/// `SDLPAINTER_REQUIRE_VALIDATION` ayarlıysa ikinci durum hatadır (bkz.
/// `ValidationIsRequired()`).
class ValidationGuard {
 public:
  ValidationGuard() : mBefore(CurrentCount()) {
#ifdef SDLPAINTER_HAS_VULKAN
    for (std::size_t i = 0; i < kUnvalidatedStates.size(); ++i) {
      mUnvalidatedBefore[i] = vk_detail::ContextCount(kUnvalidatedStates[i]);
    }
#endif
  }

  ~ValidationGuard() {
    const uint64_t after = CurrentCount();
    if (after != mBefore) {
      ADD_FAILURE() << "Vulkan validation " << (after - mBefore)
                    << " yeni hata raporladi (ayrinti icin test ciktisindaki "
                       "[Vulkan] satirlarina bakin).";
    }
#ifdef SDLPAINTER_HAS_VULKAN
    if (ValidationIsRequired()) {
      ExpectNoUnvalidatedContext();
    }
#endif
  }

  ValidationGuard(const ValidationGuard&) = delete;
  ValidationGuard& operator=(const ValidationGuard&) = delete;

 private:
  static uint64_t CurrentCount() {
#ifdef SDLPAINTER_HAS_VULKAN
    return vk_detail::ValidationErrorCount();
#else
    return 0;
#endif
  }

#ifdef SDLPAINTER_HAS_VULKAN
  static constexpr std::size_t kUnvalidatedCount = 3;
  static constexpr std::array<vk_detail::ValidationState, kUnvalidatedCount>
      kUnvalidatedStates = {vk_detail::ValidationState::kDisabledInBuild,
                            vk_detail::ValidationState::kLayerMissing,
                            vk_detail::ValidationState::kMessengerFailed};

  static const char* Reason(vk_detail::ValidationState state) {
    switch (state) {
      case vk_detail::ValidationState::kDisabledInBuild:
        return "kutuphane Release (NDEBUG) derlenmis, validation tasarim "
               "geregi kapali. Degiskeni yalnizca Debug derlemede ayarlayin.";
      case vk_detail::ValidationState::kLayerMissing:
        return "VK_LAYER_KHRONOS_validation katmani bulunamadi. Katmani "
               "kurun (Debian/Ubuntu: vulkan-validationlayers) ya da "
               "VK_LAYER_PATH ile gosterin.";
      case vk_detail::ValidationState::kMessengerFailed:
        return "katman acik ama debug messenger kurulamadi; validation "
               "hatalari sayilamaz (log'daki messenger uyarisina bakin).";
      case vk_detail::ValidationState::kEnabled:
      case vk_detail::ValidationState::kCount:
        break;
    }
    return "";
  }

  void ExpectNoUnvalidatedContext() const {
    for (std::size_t i = 0; i < kUnvalidatedStates.size(); ++i) {
      const uint64_t created = vk_detail::ContextCount(kUnvalidatedStates[i]) -
                               mUnvalidatedBefore[i];
      if (created != 0) {
        ADD_FAILURE() << created
                      << " Vulkan baglami validation'siz kuruldu, ancak "
                         "SDLPAINTER_REQUIRE_VALIDATION ayarli: "
                      << Reason(kUnvalidatedStates[i]);
      }
    }
  }

  std::array<uint64_t, kUnvalidatedCount> mUnvalidatedBefore{};
#endif

  uint64_t mBefore{0};
};

/// @brief Font bulunamadığında test atlanmak yerine başarısız mı olmalı?
///
/// Metin testleri sistemde bir TTF fontu yoksa `GTEST_SKIP()` eder. Bu,
/// geliştiricinin çıplak makinesinde makul; CI'da ise tehlikeli: testler
/// yeşil görünürken hiç koşmaz. Nitekim ilk kapsama ölçümünde
/// `glyph_atlas.cpp` %0 çıktı: CI imajında font paketi yoktu ve 28 test
/// sessizce atlanıyordu.
///
/// `SDLPAINTER_REQUIRE_FONT=1` ayarlandığında (CI bunu yapar) font yokluğu
/// artık atlama değil hatadır; sorun ilk çalıştırmada görünür olur.
inline bool FontIsRequired() {
  const char* env = std::getenv("SDLPAINTER_REQUIRE_FONT");
  return env != nullptr && env[0] != '\0' && env[0] != '0';
}

/// @brief Backend kurulamadığında test atlanmak yerine başarısız mı olmalı?
///
/// GPU testleri pencere/context oluşturulamayan ortamlarda `GTEST_SKIP()`
/// eder. CI'da bu tehlikeli: lavapipe ICD'si ya da EGL bulunamazsa testlerin
/// hepsi atlanır, ctest yine de başarılı döner.
///
/// `SDLPAINTER_REQUIRE_BACKEND=1` ayarlandığında backend yokluğu atlama
/// değil hatadır. Derlenmiş tüm backend'ler için geçerlidir. Windows
/// yerelinde `offscreen` sürücüsü EGL yükleyemediği için orada ayarlanmaz.
inline bool BackendIsRequired() {
  const char* env = std::getenv("SDLPAINTER_REQUIRE_BACKEND");
  return env != nullptr && env[0] != '\0' && env[0] != '0';
}

/// @brief Sistemde kullanılabilir bir TTF fontu arar.
/// @return Bulunan fontun yolu; hiçbiri yoksa boş string.
inline std::string FindSystemFont() {
  const char* kCandidates[] = {
#ifdef _WIN32
      "C:/Windows/Fonts/arial.ttf",
      "C:/Windows/Fonts/tahoma.ttf",
      "C:/Windows/Fonts/segoeui.ttf",
      "C:/Windows/Fonts/calibri.ttf",
#else
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
      "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
      "/usr/share/fonts/TTF/DejaVuSans.ttf",
      "/usr/share/fonts/dejavu/DejaVuSans.ttf",
#endif
  };
  for (const char* path : kCandidates) {
    SDL_IOStream* io = SDL_IOFromFile(path, "rb");
    if (io != nullptr) {
      SDL_CloseIO(io);
      return path;
    }
  }
  return {};
}

}  // namespace sdl_painter::testing

/// @brief Font gerektiren testin başına konur: yolu `var` içine alır.
///
/// Font yoksa `SDLPAINTER_REQUIRE_FONT` ayarlıysa testi düşürür, değilse
/// atlar. Böylece CI'da sessiz atlama olmaz, yerelde geliştirici engellenmez.
#define SDLPAINTER_REQUIRE_FONT_OR_SKIP(var)                               \
  const std::string var = sdl_painter::testing::FindSystemFont();          \
  if ((var).empty()) {                                                     \
    if (sdl_painter::testing::FontIsRequired()) {                          \
      FAIL() << "Sistemde TTF font bulunamadi, ancak "                     \
                "SDLPAINTER_REQUIRE_FONT ayarli. Metin testleri sessizce " \
                "atlanamaz; ortama bir font paketi kurun "                 \
                "(orn. fonts-dejavu-core).";                               \
    }                                                                      \
    GTEST_SKIP() << "Sistemde TTF font bulunamadi.";                       \
  }                                                                        \
  static_assert(true, "")

/// @brief Backend/pencere kurulamadığında testi atlar ya da düşürür.
///
/// `SDLPAINTER_REQUIRE_BACKEND` ayarlıysa testi düşürür, değilse atlar.
/// `reason` akışa yazılabilir bir ifadedir. Yalnızca `void` dönen test
/// gövdelerinde kullanılabilir.
#define SDLPAINTER_BACKEND_UNAVAILABLE(reason)                          \
  do {                                                                  \
    if (sdl_painter::testing::BackendIsRequired()) {                    \
      FAIL() << "Backend kurulamadi, ancak SDLPAINTER_REQUIRE_BACKEND " \
                "ayarli. GPU testleri sessizce atlanamaz: "             \
             << (reason);                                               \
    }                                                                   \
    GTEST_SKIP() << (reason);                                           \
  } while (false)
