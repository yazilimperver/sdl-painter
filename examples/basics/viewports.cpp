/// @brief viewports — bölünmüş ekran ve mini harita.
///
/// `SetViewport`, çizimi pencerenin bir alt dikdörtgeniyle sınırlar **ve
/// koordinatları o alt dikdörtgene yerelleştirir**: viewport ayarlandıktan
/// sonra `(0, 0)` panelin sol üst köşesidir. Bu sayede dört panelin dördü de
/// aynı çizim fonksiyonunu, hiçbir ofset hesabı yapmadan çağırabiliyor —
/// aralarındaki tek fark kamera konumu.
///
/// **Kırpma (`SetClipRect`) ile karıştırılmamalı.** Kırpma koordinat sistemini
/// değiştirmeden pikselleri maskeler; viewport koordinat sisteminin kendisini
/// yeniden tanımlar. İkisi birlikte de çalışır: kırpma dikdörtgeni
/// viewport-yerel verilir (sağ alt panelde gösteriliyor).
///
/// Viewport bir GPU durumudur: her değişim biriken çizimleri flush eder, yani
/// panel sayısı kadar draw call taban maliyeti vardır. F1 katmanındaki sayaçta
/// görünür — bölünmüş ekranın bedeli budur ve gizlenmemeli.
///
/// Kontroller:
///   WASD / oklar — paylaşılan dünyada gez
///   C            — sağ alt panelde kırpmayı aç/kapat
///   M            — mini haritayı aç/kapat
///   ESC          — çıkış

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

constexpr float kWorldSize = 1600.0F;
constexpr float kSpeed = 420.0F;
constexpr int32_t kLandmarks = 40;

/// @brief Panel başına kamera ofseti — dördü dünyanın farklı yerine bakar.
const std::array<sp::Point, 4> kCameraOffsets = {
    sp::Point{0.0F, 0.0F}, sp::Point{420.0F, 0.0F}, sp::Point{0.0F, 380.0F},
    sp::Point{420.0F, 380.0F}};

const std::array<const char*, 4> kPanelNames = {
    "kamera 1", "kamera 2", "kamera 3", "kamera 4 (kirpmali)"};

const std::array<sp::Color, 4> kPanelTints = {
    sp::Color{90, 170, 255, 255}, sp::Color{255, 170, 90, 255},
    sp::Color{130, 230, 150, 255}, sp::Color{200, 140, 245, 255}};

}  // namespace

class ViewportsDemo : public sp::Application {
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

  void OnUpdate(float dt) override {
    float dx = 0.0F;
    float dy = 0.0F;
    if (mLeft) {
      dx -= 1.0F;
    }
    if (mRight) {
      dx += 1.0F;
    }
    if (mUp) {
      dy -= 1.0F;
    }
    if (mDown) {
      dy += 1.0F;
    }
    mPlayerX = Clamp(mPlayerX + dx * kSpeed * dt, 0.0F, kWorldSize);
    mPlayerY = Clamp(mPlayerY + dy * kSpeed * dt, 0.0F, kWorldSize);
    mTime += dt;
  }

  void OnRender(sp::Painter& painter) override {
    // Arka plan tüm pencereye — viewport ayarlanmadan önce.
    painter.Clear(sp::Color{14, 15, 22, 255});

    const auto w = static_cast<float>(Width());
    const auto h = static_cast<float>(Height());
    const auto half_w = static_cast<int32_t>(w * 0.5F) - 3;
    const auto half_h = static_cast<int32_t>(h * 0.5F) - 3;

    for (std::size_t i = 0; i < 4; ++i) {
      const int32_t px = (i % 2 == 0) ? 0 : static_cast<int32_t>(w * 0.5F) + 3;
      const int32_t py = (i < 2) ? 0 : static_cast<int32_t>(h * 0.5F) + 3;

      painter.SetViewport(px, py, half_w, half_h);

      // Bu noktadan itibaren (0,0) PANELIN sol ust kosesi.
      const bool clip_this_panel = (i == 3) && mClip;
      if (clip_this_panel) {
        // Kirpma dikdortgeni de viewport-yerel: panelin ortasinda bir pencere.
        painter.SetClipRect(sp::Rect{40.0F, 40.0F,
                                     static_cast<float>(half_w) - 80.0F,
                                     static_cast<float>(half_h) - 80.0F});
      }

      DrawWorldPanel(painter, static_cast<float>(half_w),
                     static_cast<float>(half_h), kCameraOffsets[i],
                     kPanelTints[i], kPanelNames[i]);

      if (clip_this_panel) {
        painter.ClearClip();
      }
    }

    // Mini harita: kendi viewport'unda, sag altta.
    if (mMinimap) {
      const int32_t kMapSize = 190;
      painter.SetViewport(static_cast<int32_t>(w) - kMapSize - 16,
                          static_cast<int32_t>(h) - kMapSize - 16, kMapSize,
                          kMapSize);
      DrawMinimap(painter, static_cast<float>(kMapSize));
    }

    // Panel ayraclari pencere koordinatinda cizilmeli → viewport sifirlanir.
    painter.ResetViewport();
    painter.SetPen(sp::Pen(sp::Color{60, 66, 86, 255}, 3.0F));
    painter.DrawLine(w * 0.5F, 0.0F, w * 0.5F, h);
    painter.DrawLine(0.0F, h * 0.5F, w, h * 0.5F);
  }

  void OnKeyDown(const sp::KeyEvent& event) override {
    SetKey(event.key, true);
    if (event.repeat) {
      return;
    }
    if (event.key == sp::Key::kC) {
      mClip = !mClip;
    } else if (event.key == sp::Key::kM) {
      mMinimap = !mMinimap;
    } else if (event.key == sp::Key::kEscape) {
      Quit();
    }
  }

  void OnKeyUp(const sp::KeyEvent& event) override { SetKey(event.key, false); }

 private:
  static float Clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
  }

  void SetKey(sp::Key key, bool down) {
    switch (key) {
      case sp::Key::kA:
      case sp::Key::kLeft:
        mLeft = down;
        break;
      case sp::Key::kD:
      case sp::Key::kRight:
        mRight = down;
        break;
      case sp::Key::kW:
      case sp::Key::kUp:
        mUp = down;
        break;
      case sp::Key::kS:
      case sp::Key::kDown:
        mDown = down;
        break;
      default:
        break;
    }
  }

  /// @brief Paylaşılan dünyayı bir panele çiz.
  ///
  /// Fonksiyon panelin nerede olduğunu **bilmiyor**: koordinatlar zaten
  /// viewport-yereldir. Viewport olmasaydı her çizime panel ofseti eklemek
  /// ya da transform yığınını kirletmek gerekirdi.
  void DrawWorldPanel(sp::Painter& painter, float pw, float ph,
                      const sp::Point& camera_offset, const sp::Color& tint,
                      const std::string& label) {
    // Panel zemini.
    painter.SetPen(sp::Pen::NoPen());
    painter.SetBrush(sp::Brush::LinearGradient({0.0F, 0.0F}, {0.0F, ph},
                                               sp::Color{26, 30, 44, 255},
                                               sp::Color{16, 18, 28, 255}));
    painter.FillRect(0.0F, 0.0F, pw, ph);

    // Kamera: oyuncuyu merkeze al, panele göre ofsetle.
    const float cam_x = mPlayerX + camera_offset.x - pw * 0.5F;
    const float cam_y = mPlayerY + camera_offset.y - ph * 0.5F;

    painter.Save();
    painter.Translate(-cam_x, -cam_y);

    // Dünya işaretleri — deterministik yerleşim.
    painter.SetPen(sp::Pen::NoPen());
    for (int32_t i = 0; i < kLandmarks; ++i) {
      const float fx = std::fmod(static_cast<float>(i) * 173.0F, kWorldSize);
      const float fy = std::fmod(static_cast<float>(i) * 271.0F, kWorldSize);
      painter.SetBrush(sp::Brush(sp::Color{tint.r, tint.g, tint.b, 90}));
      painter.FillRoundedRect(fx, fy, 70.0F, 46.0F, 10.0F);
    }

    // Oyuncu.
    painter.SetBrush(sp::Brush(sp::Color{255, 240, 200, 255}));
    painter.FillCircle(mPlayerX, mPlayerY, 12.0F);
    painter.Restore();

    // Panel cercevesi — yine viewport-yerel.
    painter.SetPen(sp::Pen(sp::Color{tint.r, tint.g, tint.b, 200}, 2.0F));
    painter.DrawRect(1.0F, 1.0F, pw - 2.0F, ph - 2.0F);

    // Etiket de viewport-yerel: her panelde ayni koordinat, farkli yer.
    if (mFont) {
      painter.SetFont(mFont);
      painter.SetPen(sp::Pen(sp::Color{tint.r, tint.g, tint.b, 235}, 1.0F));
      painter.DrawText(sp::Rect{0.0F, 8.0F, pw, 18.0F}, label,
                       sp::Alignment::kCenter);
    }
  }

  /// @brief Mini harita: dünyanın tamamı panele sığdırılır.
  void DrawMinimap(sp::Painter& painter, float size) {
    painter.SetPen(sp::Pen::NoPen());
    painter.SetBrush(sp::Brush(sp::Color{10, 12, 18, 225}));
    painter.FillRect(0.0F, 0.0F, size, size);

    const float scale = size / kWorldSize;
    painter.SetBrush(sp::Brush(sp::Color{90, 110, 150, 200}));
    for (int32_t i = 0; i < kLandmarks; ++i) {
      const float fx = std::fmod(static_cast<float>(i) * 173.0F, kWorldSize);
      const float fy = std::fmod(static_cast<float>(i) * 271.0F, kWorldSize);
      painter.FillRect(fx * scale, fy * scale, 3.0F, 2.0F);
    }

    painter.SetBrush(sp::Brush(sp::Color{255, 220, 120, 255}));
    painter.FillCircle(mPlayerX * scale, mPlayerY * scale, 4.0F);

    sp::Pen frame(sp::Color{200, 210, 235, 220}, 2.0F);
    frame.SetDashPattern({6.0F, 4.0F});
    painter.SetPen(frame);
    painter.DrawRect(1.0F, 1.0F, size - 2.0F, size - 2.0F);
  }

  std::shared_ptr<sp::Font> mFont;
  float mPlayerX{kWorldSize * 0.5F};
  float mPlayerY{kWorldSize * 0.5F};
  float mTime{0.0F};
  bool mLeft{false};
  bool mRight{false};
  bool mUp{false};
  bool mDown{false};
  bool mClip{true};
  bool mMinimap{true};
};

int main() {
  sp::AppConfig config;
  config.title = "SDLPainter — viewports: WASD gez, C kirpma, M mini harita";
  config.width = 1000;
  config.height = 720;
  config.stats_overlay = sp::StatsOverlayMode::kDetailed;

  ViewportsDemo app(config);
  return app.Run();
}
