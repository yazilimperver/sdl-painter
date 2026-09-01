/// @brief render_target — ekran yerine dokuya çizmek.
///
/// `RenderTarget`, çizimi ekran yerine bir dokuya yönlendirir. Bu, tek bir
/// özellik değil bir kapı: bir kez dokuya çizebiliyorsanız sonucu
/// ölçekleyebilir, tekrar tekrar basabilir, üzerinde biriktirebilir ve geri
/// okuyabilirsiniz.
///
/// Sahne üç bölümden oluşuyor, üçü de aynı hedefi farklı biçimde kullanıyor:
///
///   1. Kaynak — dönen bir şekil grubu bir hedefe çizilir ve olduğu gibi
///      ekrana basılır. Ekrana doğrudan çizmekten farkı görünmez; amaç
///      "hedefe çizim ekrana çizimle aynı sonucu verir" demek.
///   2. Mini harita — aynı hedef, tek satırla küçültülüp köşeye basılır.
///      Sahne ikinci kez çizilmiyor; hedef bir doku olduğu için istediğiniz
///      kadar, istediğiniz boyutta basılabilir. Pahalı bir sahnenin ikinci bir
///      görünümünü elde etmenin ucuz yolu bu.
///   3. İz (trail) efekti — iki hedef sırayla kullanılır: yeni kare,
///      *önceki* hedefin biraz soluklaştırılmış içeriğinin üzerine çizilir.
///      Sonuç, hareketin arkasında sönen bir iz. İki hedef gerekli çünkü bir
///      hedefe çizerken kendi içeriğini örnekleyemezsiniz.
///
/// Kontroller:
///   SPACE — animasyonu dondur / devam ettir
///   R     — izi temizle
///   F     — mini haritanın filtresini değiştir (yeniden yaratır)
///   ESC   — çıkış

#include "sdl_painter/render_target.h"

#include "sdl_painter/app/application.h"
#include "sdl_painter/font.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "example_font.h"

namespace sp = sdl_painter;

namespace {

constexpr int32_t kSourceSize = 256;
constexpr int32_t kTrailSize = 320;

}  // namespace

class RenderTargetDemo : public sp::Application {
 public:
  using Application::Application;

 protected:
  bool OnInit() override {
    const std::string path = example::FindSystemFont();
    if (!path.empty()) {
      mFont = std::make_shared<sp::Font>(path, 14);
      if (!mFont->IsValid()) {
        mFont.reset();
      }
    }
    return true;
  }

  void OnUpdate(float delta_seconds) override {
    if (!mPaused) {
      mPhase += delta_seconds;
    }
  }

  void OnRender(sp::Painter& painter) override {
    // Hedefler ilk karede yaratilir: Painter'in gecerli oldugundan emin olmak
    // icin OnInit yerine burada. Basarisiz olursa aciklama ekrana yazilir.
    if (!mReady) {
      CreateTargets(painter);
    }

    painter.Clear(sp::Color{16, 18, 26, 255});

    if (!mReady) {
      Label(painter, 20.0F, 20.0F, 600.0F,
            "Bu backend cizim hedefi desteklemiyor.");
      return;
    }

    RenderSource(painter);
    AdvanceTrail(painter);

    // --- 1. Kaynak, olculu boyutta ---
    painter.DrawRenderTarget(mSource, sp::Rect{20.0F, 40.0F, 256.0F, 256.0F});
    Frame(painter, sp::Rect{20.0F, 40.0F, 256.0F, 256.0F});
    Label(painter, 20.0F, 16.0F, 256.0F, "1. hedefe cizilip ekrana basildi");

    // --- 2. Ayni hedef, mini harita olarak ---
    // Sahne IKINCI KEZ cizilmiyor; ayni doku farkli boyutta basiliyor.
    painter.DrawRenderTarget(mSource, sp::Rect{296.0F, 40.0F, 72.0F, 72.0F});
    Frame(painter, sp::Rect{296.0F, 40.0F, 72.0F, 72.0F});
    Label(painter, 288.0F, 16.0F, 100.0F, "2. mini harita");
    Label(painter, 288.0F, 118.0F, 100.0F, mNearest ? "kNearest" : "kLinear");

    // --- 3. Iz efekti ---
    painter.DrawRenderTarget(mTrail[mTrailIndex],
                             sp::Rect{392.0F, 40.0F, 320.0F, 320.0F});
    Frame(painter, sp::Rect{392.0F, 40.0F, 320.0F, 320.0F});
    Label(painter, 392.0F, 16.0F, 320.0F,
          "3. iz: her kare oncekinin uzerine soluk cizilir");

    Label(painter, 20.0F, 320.0F, 350.0F,
          "SPACE dondur   R izi temizle   F filtre");
  }

  void OnKeyDown(const sp::KeyEvent& event) override {
    switch (event.key) {
      case sp::Key::kSpace:
        if (!event.repeat) {
          mPaused = !mPaused;
        }
        break;
      case sp::Key::kR:
        if (!event.repeat) {
          mClearTrail = true;
        }
        break;
      case sp::Key::kF:
        if (!event.repeat) {
          mNearest = !mNearest;
          // Filtre doku YARATILIRKEN uygulanir; hedefi yeniden kurmak gerekir.
          mReady = false;
        }
        break;
      case sp::Key::kEscape:
        Quit();
        break;
      default:
        break;
    }
  }

 private:
  void CreateTargets(sp::Painter& painter) {
    const sp::TextureFilter kFilter =
        mNearest ? sp::TextureFilter::kNearest : sp::TextureFilter::kLinear;
    mSource = painter.CreateRenderTarget(kSourceSize, kSourceSize, kFilter);
    mTrail[0] = painter.CreateRenderTarget(kTrailSize, kTrailSize);
    mTrail[1] = painter.CreateRenderTarget(kTrailSize, kTrailSize);
    mReady = mSource.IsValid() && mTrail[0].IsValid() && mTrail[1].IsValid();
    mClearTrail = true;
  }

  /// @brief 1. ve 2. bölümün beslediği sahne — hedefe çizilir.
  void RenderSource(sp::Painter& painter) {
    painter.SetRenderTarget(mSource);
    painter.Clear(sp::Color{28, 32, 48, 255});

    const auto kHalf = static_cast<float>(kSourceSize) * 0.5F;
    painter.Save();
    painter.Translate(kHalf, kHalf);
    painter.Rotate(mPhase * 40.0F);
    for (int32_t i = 0; i < 6; ++i) {
      const float t = static_cast<float>(i) / 6.0F;
      painter.Save();
      painter.Rotate(t * 360.0F);
      painter.SetPen(sp::Pen::NoPen());
      painter.SetBrush(
          sp::Brush(sp::Color{static_cast<uint8_t>(80 + (i * 28)), 160,
                              static_cast<uint8_t>(240 - (i * 24)), 230}));
      painter.FillRoundedRect(40.0F, -14.0F, 70.0F, 28.0F, 10.0F);
      painter.Restore();
    }
    painter.Restore();

    // Merkez isareti: mini haritada filtre farkinin gorulecegi keskin kenar.
    painter.SetBrush(sp::Brush(sp::Color{255, 240, 200, 255}));
    painter.FillCircle(kHalf, kHalf, 16.0F);
    painter.SetPen(sp::Pen(sp::Color{255, 255, 255, 200}, 2.0F));
    painter.DrawRect(4.0F, 4.0F, static_cast<float>(kSourceSize) - 8.0F,
                     static_cast<float>(kSourceSize) - 8.0F);

    painter.ResetRenderTarget();
  }

  /// @brief İz efekti: hedefleri sırayla kullanarak birikim.
  ///
  /// Anahtar kısıt: bir hedefe çizerken onun kendi dokusunu örnekleyemezsiniz.
  /// Bu yüzden iki hedef var — yeni kare, öncekinin içeriğinin üzerine çizilir
  /// ve rol değişir.
  void AdvanceTrail(sp::Painter& painter) {
    const std::size_t previous = mTrailIndex;
    const std::size_t next = 1U - mTrailIndex;

    painter.SetRenderTarget(mTrail[next]);
    if (mClearTrail) {
      painter.Clear(sp::Color{12, 14, 22, 255});
      mClearTrail = false;
    } else {
      // Onceki kareyi hafif soluklastirarak kopyala: iz boylece soner.
      painter.Clear(sp::Color{12, 14, 22, 255});
      painter.DrawRenderTarget(
          mTrail[previous],
          sp::Rect{0.0F, 0.0F, static_cast<float>(kTrailSize),
                   static_cast<float>(kTrailSize)},
          sp::Color{255, 255, 255, 232});
    }

    // Hareket eden nokta — izin kaynagi.
    const auto kHalf = static_cast<float>(kTrailSize) * 0.5F;
    const float radius = kHalf * 0.62F;
    const float x = kHalf + (std::cos(mPhase * 1.7F) * radius);
    const float y = kHalf + (std::sin(mPhase * 2.3F) * radius);

    painter.SetPen(sp::Pen::NoPen());
    painter.SetBlendMode(sp::BlendMode::kAdditive);
    painter.SetBrush(sp::Brush(sp::Color{90, 200, 255, 255}));
    painter.FillCircle(x, y, 9.0F);
    painter.SetBrush(sp::Brush(sp::Color{255, 140, 90, 255}));
    painter.FillCircle(kTrailSize - x, y, 6.0F);
    painter.SetBlendMode(sp::BlendMode::kAlpha);

    painter.ResetRenderTarget();
    mTrailIndex = next;
  }

  /// @brief Panelin çevresine ince çerçeve.
  void Frame(sp::Painter& painter, const sp::Rect& rect) {
    painter.SetPen(sp::Pen(sp::Color{90, 100, 125, 200}, 1.0F));
    painter.DrawRect(rect.x, rect.y, rect.w, rect.h);
  }

  /// @brief Etiket — font yoksa sessizce atlanır.
  void Label(sp::Painter& painter, float x, float y, float w,
             const std::string& text) {
    if (!mFont) {
      return;
    }
    painter.SetFont(mFont);
    painter.SetPen(sp::Pen(sp::Color{175, 186, 208, 235}, 1.0F));
    painter.DrawText(sp::Rect{x, y, w, 18.0F}, text, sp::Alignment::kLeft);
  }

  std::shared_ptr<sp::Font> mFont;

  sp::RenderTarget mSource;
  std::array<sp::RenderTarget, 2> mTrail;
  std::size_t mTrailIndex{0};

  float mPhase{0.0F};
  bool mPaused{false};
  bool mReady{false};
  bool mClearTrail{true};
  bool mNearest{false};
};

int main() {
  sp::AppConfig config;
  config.title = "SDLPainter — render_target: SPACE dondur, R iz, F filtre";
  config.width = 740;
  config.height = 390;
  config.stats_overlay = sp::StatsOverlayMode::kDetailed;

  RenderTargetDemo app(config);
  return app.Run();
}
