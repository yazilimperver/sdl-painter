/// @brief paint — kütüphanenin adını hak eden demo: gerçek bir çizim programı.
///
/// Bu örnek kütüphaneyi temel işlevler için **kullanmaktadır**.
/// Fare ile serbest çizim, fırça boyu, renk paleti, geri al, temizle.
///
/// Çizim modeli — dikkate değer olan kısım:
///
///   Her fırça darbesi bir **nokta listesi** olarak saklanır ve her karede
///   `DrawPolyline` ile yeniden çizilir. Ekranın kendisi bir tampon değildir;
///   sahne her karede sıfırdan üretilir. Bunun bedeli, her karede tüm
///   darbelerin yeniden tessellate edilmesidir — F1 katmanındaki CPU süresi
///   bu maliyeti gösterir. Kazancı ise geri almanın (undo) tek satır olması:
///   listenin sonundaki darbeyi at, gerisi kendiliğinden doğru çizilir.
///
///   Dokuya çizim (render-to-texture) olsaydı biriken bir tampona çizip
///   maliyeti sabitleyebilirdik — ama o zaman geri almak için ayrı bir
///   mekanizma gerekirdi. Bu, kütüphanenin bugünkü kapsamıyla uyumlu ve
///   dürüst olan yaklaşım.
///
/// Fırça darbeleri yuvarlak uç ve yuvarlak birleşim kullanır: köşeli uçlarla
/// bir fırça darbesi doğal görünmez.
///
/// Kontroller:
///   Sol tık + sürükle — çiz
///   1..8              — renk seç
///   ↑ / ↓             — fırça boyu
///   Ctrl+Z            — geri al
///   C                 — tümünü temizle
///   E                 — silgi (arka plan rengiyle çizer)
///   ESC               — çıkış

#include "sdl_painter/app/application.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sp = sdl_painter;

namespace {

constexpr float kMinBrush = 2.0F;
constexpr float kMaxBrush = 64.0F;
constexpr float kMinPointDistance = 2.0F;  ///< Aşırı nokta birikmesini önler.
constexpr sp::Color kBackground{28, 30, 38, 255};

const std::array<sp::Color, 8> kPalette = {
    sp::Color{240, 240, 245, 255}, sp::Color{240, 90, 80, 255},
    sp::Color{250, 170, 60, 255},  sp::Color{245, 225, 80, 255},
    sp::Color{110, 220, 120, 255}, sp::Color{80, 175, 250, 255},
    sp::Color{160, 120, 245, 255}, sp::Color{245, 120, 200, 255},
};

/// @brief Tek bir fırça darbesi.
struct Stroke {
  std::vector<sp::Point> points;
  sp::Color color;
  float width{6.0F};
};

}  // namespace

class PaintDemo : public sp::Application {
 public:
  using Application::Application;

 protected:
  void OnRender(sp::Painter& painter) override {
    painter.Clear(kBackground);

    for (const auto& stroke : mStrokes) {
      DrawStroke(painter, stroke);
    }
    if (mDrawing) {
      DrawStroke(painter, mCurrent);
    }

    DrawPalette(painter);
  }

  void OnMouseButtonDown(const sp::MouseButtonEvent& event) override {
    if (event.button != sp::MouseButton::kLeft) {
      return;
    }
    // Palet üzerine tıklandıysa çizim başlatma.
    if (const int32_t swatch = SwatchAt(event.x, event.y); swatch >= 0) {
      mColorIndex = static_cast<std::size_t>(swatch);
      mEraser = false;
      return;
    }

    mDrawing = true;
    mCurrent.points.clear();
    mCurrent.points.push_back({event.x, event.y});
    mCurrent.color = mEraser ? kBackground : kPalette[mColorIndex];
    mCurrent.width = mBrush;
  }

  void OnMouseMove(const sp::MouseMoveEvent& event) override {
    if (!mDrawing) {
      return;
    }
    // Çok yakın noktaları ele: hem tessellation maliyetini düşürür hem de
    // sıfır uzunluklu segmentlerin birleşim hesabını bozmasını önler.
    const sp::Point& last = mCurrent.points.back();
    const float dx = event.x - last.x;
    const float dy = event.y - last.y;
    if (dx * dx + dy * dy < kMinPointDistance * kMinPointDistance) {
      return;
    }
    mCurrent.points.push_back({event.x, event.y});
  }

  void OnMouseButtonUp(const sp::MouseButtonEvent& event) override {
    if (event.button != sp::MouseButton::kLeft || !mDrawing) {
      return;
    }
    mDrawing = false;
    if (mCurrent.points.size() >= 2) {
      mStrokes.push_back(mCurrent);
    } else if (mCurrent.points.size() == 1) {
      // Tek tıklama da bir iz bırakmalı: aynı noktayı iki kez koyup nokta
      // biçiminde kısa bir darbe üret.
      mCurrent.points.push_back(
          {mCurrent.points[0].x + 0.01F, mCurrent.points[0].y});
      mStrokes.push_back(mCurrent);
    }
    mCurrent.points.clear();
  }

  void OnKeyDown(const sp::KeyEvent& event) override {
    if (event.repeat && event.key != sp::Key::kDown &&
        event.key != sp::Key::kUp) {
      return;
    }

    const bool ctrl = (static_cast<uint8_t>(event.modifiers) &
                       static_cast<uint8_t>(sp::KeyModifier::kCtrl)) != 0;
    if (ctrl && event.key == sp::Key::kZ) {
      Undo();
      return;
    }

    switch (event.key) {
      case sp::Key::kC:
        mStrokes.clear();
        break;
      case sp::Key::kE:
        mEraser = !mEraser;
        break;
      case sp::Key::kDown:
        mBrush = std::fmax(kMinBrush, mBrush - 2.0F);
        break;
      case sp::Key::kUp:
        mBrush = std::fmin(kMaxBrush, mBrush + 2.0F);
        break;
      case sp::Key::kEscape:
        Quit();
        break;
      default:
        break;
    }

    // 1..8 → palet.
    const auto key_index =
        static_cast<int32_t>(event.key) - static_cast<int32_t>(sp::Key::k1);
    if (key_index >= 0 && key_index < static_cast<int32_t>(kPalette.size())) {
      mColorIndex = static_cast<std::size_t>(key_index);
      mEraser = false;
    }
  }

 private:
  static void DrawStroke(sp::Painter& painter, const Stroke& stroke) {
    if (stroke.points.size() < 2) {
      return;
    }
    sp::Pen pen(stroke.color, stroke.width);
    // Fırça darbesi: yuvarlak uç ve birleşim olmadan doğal görünmez.
    pen.SetCapStyle(sp::LineCap::kRound);
    pen.SetJoinStyle(sp::LineJoin::kRound);
    painter.SetPen(pen);
    painter.DrawPolyline(stroke.points);
  }

  void DrawPalette(sp::Painter& painter) const {
    const float y = static_cast<float>(Height()) - kSwatch - 12.0F;

    for (std::size_t i = 0; i < kPalette.size(); ++i) {
      const float x = 12.0F + static_cast<float>(i) * (kSwatch + 8.0F);
      painter.SetPen(sp::Pen::NoPen());
      painter.SetBrush(sp::Brush(kPalette[i]));
      painter.FillRect(x, y, kSwatch, kSwatch);

      if (i == mColorIndex && !mEraser) {
        // Seçili renk: kesikli çerçeve — "seçim" için klasik gösterim.
        sp::Pen sel(sp::Color{255, 255, 255, 240}, 2.0F);
        sel.SetDashPattern({5.0F, 4.0F});
        painter.SetPen(sel);
        painter.DrawRect(x - 3.0F, y - 3.0F, kSwatch + 6.0F, kSwatch + 6.0F);
      }
    }

    // Fırça boyu göstergesi.
    const float preview_x =
        12.0F + static_cast<float>(kPalette.size()) * (kSwatch + 8.0F) + 24.0F;
    painter.SetPen(sp::Pen::NoPen());
    painter.SetBrush(sp::Brush(mEraser ? sp::Color{200, 200, 210, 200}
                                       : kPalette[mColorIndex]));
    painter.FillCircle(preview_x + kSwatch * 0.5F, y + kSwatch * 0.5F,
                       mBrush * 0.5F);

    if (mEraser) {
      sp::Pen mark(sp::Color{255, 120, 120, 240}, 2.0F);
      mark.SetDashPattern({4.0F, 3.0F});
      painter.SetPen(mark);
      painter.DrawCircle(preview_x + kSwatch * 0.5F, y + kSwatch * 0.5F,
                         mBrush * 0.5F + 6.0F);
    }
  }

  /// @brief Verilen noktada bir palet kutusu var mı? Yoksa -1.
  [[nodiscard]] int32_t SwatchAt(float x, float y) const {
    const float row_y = static_cast<float>(Height()) - kSwatch - 12.0F;
    if (y < row_y || y > row_y + kSwatch) {
      return -1;
    }
    for (std::size_t i = 0; i < kPalette.size(); ++i) {
      const float sx = 12.0F + static_cast<float>(i) * (kSwatch + 8.0F);
      if (x >= sx && x <= sx + kSwatch) {
        return static_cast<int32_t>(i);
      }
    }
    return -1;
  }

  void Undo() {
    if (!mStrokes.empty()) {
      mStrokes.pop_back();
    }
  }

  static constexpr float kSwatch = 34.0F;

  std::vector<Stroke> mStrokes;
  Stroke mCurrent;
  std::size_t mColorIndex{0};
  float mBrush{8.0F};
  bool mDrawing{false};
  bool mEraser{false};
};

int main() {
  sp::AppConfig config;
  config.title =
      "SDLPainter — paint: 1-8 renk, yukari/asagi firca, Ctrl+Z geri al";
  config.width = 1100;
  config.height = 750;
  config.stats_overlay = sp::StatsOverlayMode::kDetailed;

  PaintDemo app(config);
  return app.Run();
}
