#pragma once

#include "sdl_painter/app/app_config.h"
#include "sdl_painter/app/events.h"
#include "sdl_painter/app/export.h"
#include "sdl_painter/painter.h"

#include <cstdint>
#include <memory>
#include <string>

// SDL forward declaration'ları — public başlığa SDL sızmasın.
struct SDL_Window;
union SDL_Event;  // SDL3'te SDL_Event bir union'dır (typedef ile aynı etiket).

namespace sdl_painter {

namespace app_detail {
class StatsOverlay;
}  // namespace app_detail

/// @brief SDL pencere/olay-döngüsü boilerplate'ini soyutlayan uygulama çatısı.
///
/// Kullanıcı bu sınıftan türeyip korumalı sanal metotları override eder;
/// SDL init, pencere oluşturma, GL context, olay döngüsü, zamanlama ve
/// yıkım tamamen çatı tarafından yönetilir.
///
/// @code
/// class MyApp : public sdl_painter::Application {
///  public:
///   using Application::Application;
///  protected:
///   void OnRender(sdl_painter::Painter& p) override {
///     p.Clear({30, 30, 30, 255});
///     p.DrawCircle(400, 300, 80);
///   }
///   void OnKeyDown(const sdl_painter::KeyEvent& e) override {
///     if (e.key == sdl_painter::Key::kEscape) Quit();
///   }
/// };
///
/// int main() {
///   sdl_painter::AppConfig cfg;
///   cfg.title = "Demo";
///   MyApp app(cfg);
///   return app.Run();
/// }
/// @endcode
///
/// @warning **Yaşam döngüsü:** Türeyen sınıfın @ref Image / @ref Font üyeleri,
/// temel sınıf yıkıcısı çalışmadan önce (yani içteki Painter yıkılmadan
/// önce) yok edilir. Bu sıralama kasıtlıdır ve @ref Painter yaşam sözleşmesini
/// otomatik olarak sağlar — kaynakları düz üye olarak tutmak güvenlidir.
class SDLPAINTER_APP_API Application {
 public:
  /// @brief Verilen yapılandırma ile uygulamayı oluştur (init @ref Run içinde).
  explicit Application(AppConfig config = {});

  /// @brief Painter → pencere → SDL_Quit sırasıyla kaynakları serbest bırakır.
  virtual ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
  Application(Application&&) = delete;
  Application& operator=(Application&&) = delete;

  /// @brief Uygulamayı başlat ve olay döngüsünü çalıştır.
  /// @return Başarıda 0; init veya @ref OnInit hatasında 1.
  int Run();

  /// @brief Olay döngüsünden çıkışı iste (bir sonraki iterasyonda durur).
  void Quit() noexcept;

  /// @brief Güncel çizim yüzeyi genişliği (framebuffer piksel).
  /// @note @ref AppConfig::high_dpi açıkken bu değer, mantıksal pencere
  ///       genişliğinden ekran ölçek faktörü kadar büyüktür.
  [[nodiscard]] int32_t Width() const noexcept { return mWidth; }

  /// @brief Güncel çizim yüzeyi yüksekliği (framebuffer piksel).
  [[nodiscard]] int32_t Height() const noexcept { return mHeight; }

  /// @brief Pencere başlığını güncelle.
  void SetTitle(const std::string& title);

  /// @brief Alttaki SDL penceresi — belgelenmiş kaçış kapısı (ileri kullanım).
  [[nodiscard]] SDL_Window* GetWindow() const noexcept { return mWindow; }

  /// @brief Yumuşatılmış kare hızı (kare/saniye).
  ///
  /// Çeyrek saniyelik pencerede ortalanır; ham `1/dt` değerinden çok daha
  /// okunabilirdir. @ref AppConfig::stats_overlay kapalı olsa da hesaplanır.
  [[nodiscard]] double Fps() const noexcept;

  /// @brief Son tamamlanan karenin çizim istatistikleri.
  [[nodiscard]] const FrameStats& GetFrameStats() const noexcept;

  /// @brief Ekran üstü gösterge modunu değiştir.
  void SetStatsOverlay(StatsOverlayMode mode) noexcept { mStatsMode = mode; }

  /// @brief Güncel gösterge modu.
  [[nodiscard]] StatsOverlayMode GetStatsOverlay() const noexcept {
    return mStatsMode;
  }

 protected:
  /// @brief Aktif Painter — yalnızca @ref Run çalışırken (OnInit dahil) geçerli.
  [[nodiscard]] Painter& GetPainter() { return *mPainter; }

  /// @brief Döngü başlamadan bir kez çağrılır (kaynak yükleme vb.).
  /// @return `false` dönerse @ref Run 1 ile çıkar.
  virtual bool OnInit() { return true; }

  /// @brief Her frame'de güncelleme — @p dt saniye cinsinden delta zaman.
  /// @note @ref TimingMode::kFixed modunda sabit adımla (`1/fixed_update_hz`)
  ///       frame başına 0..N kez çağrılır; kVariable'da her frame bir kez.
  virtual void OnUpdate(float dt) { (void)dt; }

  /// @brief Her frame'de çizim.
  /// @note En az bir @c OnRender override edilmelidir. Basit uygulamalar bu
  ///       tek-argümanlı sürümü override eder.
  virtual void OnRender(Painter& painter) { (void)painter; }

  /// @brief İnterpolasyonlu çizim — @p alpha son sabit adımdan bu yana biriken
  ///        artık zamanın oranı [0,1].
  /// @note Döngü her zaman bu sürümü çağırır. Varsayılanı @p alpha'yı
  ///       yoksayarak tek-argümanlı @ref OnRender'a delege eder — böylece
  ///       mevcut kod değişmeden çalışır. @ref TimingMode::kVariable modunda
  ///       @p alpha daima 1.0'dır. Oyunvari uygulamalar bu sürümü override edip
  ///       önceki/geçerli state arasında interpolasyon yapabilir.
  /// @warning Yalnızca bu sürümü override eden türeyen sınıflar, `-Woverloaded-`
  ///          `virtual` etkinken tek-argümanlı sürümü gizlediğine dair uyarı
  ///          alabilir; gerekirse `using Application::OnRender;` ekleyin.
  virtual void OnRender(Painter& painter, float alpha) {
    (void)alpha;
    OnRender(painter);
  }

  /// @brief Tuşa basıldı.
  virtual void OnKeyDown(const KeyEvent& event) { (void)event; }

  /// @brief Tuş bırakıldı.
  virtual void OnKeyUp(const KeyEvent& event) { (void)event; }

  /// @brief Fare düğmesine basıldı.
  virtual void OnMouseButtonDown(const MouseButtonEvent& event) { (void)event; }

  /// @brief Fare düğmesi bırakıldı.
  virtual void OnMouseButtonUp(const MouseButtonEvent& event) { (void)event; }

  /// @brief Fare hareket etti.
  virtual void OnMouseMove(const MouseMoveEvent& event) { (void)event; }

  /// @brief Fare tekerleği döndü.
  virtual void OnMouseWheel(const MouseWheelEvent& event) { (void)event; }

  /// @brief Pencere yeniden boyutlandı.
  virtual void OnResize(const ResizeEvent& event) { (void)event; }

  /// @brief Ham SDL olayı — kaçış kapısı.
  /// @return `true` dönerse olay tüketilmiş sayılır; çeviri ve varsayılan
  ///         (quit dahil) işleme atlanır. Kullanan .cpp `<SDL3/SDL.h>` dahil
  ///         etmelidir (SDL3, sdl_painter'a PUBLIC bağlıdır).
  virtual bool OnRawEvent(const SDL_Event& event) {
    (void)event;
    return false;
  }

  /// @brief Döngü bittikten sonra bir kez çağrılır (temizlik).
  virtual void OnShutdown() {}

 private:
  bool Initialize();
  void Teardown() noexcept;
  void ProcessEvents();

  /// @brief mWidth/mHeight'i pencerenin framebuffer (piksel) boyutuna eşitle.
  void UpdateDrawableSize();

  /// @brief Pencere başlığındaki FPS'i (gerekiyorsa) güncelle.
  void UpdateTitleFps(uint64_t frame_ns);

  AppConfig mConfig;
  SDL_Window* mWindow{nullptr};
  std::unique_ptr<Painter> mPainter;
  bool mRunning{false};
  bool mSdlInitialized{false};
  bool mHasRun{false};  ///< Run() tek seferlik; ikinci cagri reddedilir.
  uint64_t mLastTickNs{0};
  int32_t mWidth{0};
  int32_t mHeight{0};

  std::unique_ptr<app_detail::StatsOverlay> mStatsOverlay;
  StatsOverlayMode mStatsMode{StatsOverlayMode::kNone};
  uint64_t mTitleUpdateNs{0};
};

}  // namespace sdl_painter
