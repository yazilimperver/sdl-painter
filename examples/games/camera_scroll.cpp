/// @brief camera_scroll — transform yığınının kamera olarak kullanımı.
///
/// SDLPainter'da ayrı bir "kamera" sınıfı yok, bununla birlikte mevcut kabiliyetler ile bunu elde edebiliriz:
/// kamera, temelde çizimden önce uygulanan tek bir ters ötelemedir.
///
/// @code
/// painter.Save();
/// painter.Translate(-camera_x, -camera_y);   // dünya → ekran
/// ...dünya koordinatlarıyla çiz...
/// painter.Restore();                          // arayüz ekran koordinatında
/// @endcode
///
/// Gösterilen:
///   - Dünya ↔ ekran koordinat dönüşümü (fare konumu dünyaya çevrilir)
///   - Paralaks: farklı katmanlar kamerayı farklı oranlarda takip eder
///   - Zoom: `Scale` ile birlikte kullanım ve doğru merkez seçimi
///   - Arayüzün (HUD) kameradan etkilenmemesi — `Restore` sonrası çizilir
///
/// Karo ızgarası ve görünür alan eleme (culling) ayrı örnekte: `tilemap`.
///
/// Kontroller:
///   WASD / oklar — kamerayı hareket ettir
///   Tekerlek     — yakınlaş / uzaklaş
///   Fare         — dünya koordinatı HUD'da gösterilir, hedef işaretlenir
///   R            — kamerayı sıfırla
///   ESC          — çıkış

#include "sdl_painter/app/application.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sp = sdl_painter;

namespace {

constexpr float kCameraSpeed = 520.0F;
constexpr float kMinZoom = 0.35F;
constexpr float kMaxZoom = 3.0F;

/// @brief Paralaks katmanı: kamerayı `factor` oranında takip eder.
///
/// factor = 0 → hiç hareket etmez (sonsuz uzak gökyüzü)
/// factor = 1 → dünyayla birlikte hareket eder (oyun düzlemi)
struct Layer {
  float factor{1.0F};
  sp::Color color;
  float radius{20.0F};
  float spacing{240.0F};
  float y_offset{0.0F};
};

const std::array<Layer, 3> kLayers = {
    Layer{0.25F, sp::Color{46, 52, 78, 255}, 90.0F, 520.0F, -60.0F},
    Layer{0.55F, sp::Color{62, 78, 112, 255}, 55.0F, 330.0F, 40.0F},
    Layer{1.00F, sp::Color{96, 132, 178, 255}, 28.0F, 210.0F, 140.0F},
};

}  // namespace

class CameraScrollDemo : public sp::Application {
 public:
  using Application::Application;

 protected:
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
    // Yakınlaşmışken aynı ekran hızını korumak için kamera hızı zoom'a
    // bölünür; aksi halde zoom arttıkça dünya uçarak geçer.
    mCamX += dx * kCameraSpeed * dt / mZoom;
    mCamY += dy * kCameraSpeed * dt / mZoom;
  }

  void OnRender(sp::Painter& painter) override {
    painter.Clear(sp::Color{18, 20, 30, 255});
    painter.SetPen(sp::Pen::NoPen());

    for (const auto& layer : kLayers) {
      DrawLayer(painter, layer);
    }

    // Dünya orijinini işaretle: kameranın gerçekten hareket ettiği görünsün.
    painter.Save();
    ApplyCamera(painter, 1.0F);
    sp::Pen axis(sp::Color{255, 255, 255, 90}, 2.0F / mZoom);
    axis.SetDashPattern({14.0F / mZoom, 10.0F / mZoom});
    painter.SetPen(axis);
    painter.DrawLine(-4000.0F, 0.0F, 4000.0F, 0.0F);
    painter.DrawLine(0.0F, -4000.0F, 0.0F, 4000.0F);

    // Fare hedefi — dünya koordinatında.
    const sp::Point world = ScreenToWorld(mMouseX, mMouseY);
    painter.SetPen(sp::Pen(sp::Color{255, 210, 90, 230}, 2.0F / mZoom));
    painter.DrawCircle(world.x, world.y, 26.0F / mZoom);
    painter.DrawLine(world.x - 40.0F / mZoom, world.y, world.x + 40.0F / mZoom,
                     world.y);
    painter.DrawLine(world.x, world.y - 40.0F / mZoom, world.x,
                     world.y + 40.0F / mZoom);
    painter.Restore();

    DrawHud(painter, world);
  }

  void OnMouseMove(const sp::MouseMoveEvent& event) override {
    mMouseX = event.x;
    mMouseY = event.y;
  }

  void OnMouseWheel(const sp::MouseWheelEvent& event) override {
    // Fare imlecinin altındaki dünya noktası sabit kalmalı: önce o noktayı
    // bul, zoom'u değiştir, sonra kamerayı aynı noktayı gösterecek şekilde
    // düzelt. Bu yapılmazsa zoom ekran merkezine doğru "kayar".
    const sp::Point before = ScreenToWorld(mMouseX, mMouseY);

    mZoom *= (event.dy > 0.0F) ? 1.12F : (1.0F / 1.12F);
    mZoom = std::fmax(kMinZoom, std::fmin(kMaxZoom, mZoom));

    const sp::Point after = ScreenToWorld(mMouseX, mMouseY);
    mCamX += before.x - after.x;
    mCamY += before.y - after.y;
  }

  void OnKeyDown(const sp::KeyEvent& event) override {
    SetKey(event.key, true);
    if (!event.repeat && event.key == sp::Key::kR) {
      mCamX = 0.0F;
      mCamY = 0.0F;
      mZoom = 1.0F;
    }
    if (event.key == sp::Key::kEscape) {
      Quit();
    }
  }

  void OnKeyUp(const sp::KeyEvent& event) override { SetKey(event.key, false); }

 private:
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

  /// @brief Kamerayı uygula. `factor` paralaks oranı.
  void ApplyCamera(sp::Painter& painter, float factor) const {
    // Sıra önemli: önce ekran merkezine taşı, sonra ölçekle, sonra kamerayı
    // ters öteleyerek dünyayı kaydır. Ters sırada zoom, ekranın sol üst
    // köşesine göre çalışırdı.
    painter.Translate(static_cast<float>(Width()) * 0.5F,
                      static_cast<float>(Height()) * 0.5F);
    painter.Scale(mZoom, mZoom);
    painter.Translate(-mCamX * factor, -mCamY * factor);
  }

  /// @brief Ekran pikselini dünya koordinatına çevir (ApplyCamera'nın tersi).
  [[nodiscard]] sp::Point ScreenToWorld(float sx, float sy) const {
    const float cx = static_cast<float>(Width()) * 0.5F;
    const float cy = static_cast<float>(Height()) * 0.5F;
    return {(sx - cx) / mZoom + mCamX, (sy - cy) / mZoom + mCamY};
  }

  void DrawLayer(sp::Painter& painter, const Layer& layer) const {
    painter.Save();
    ApplyCamera(painter, layer.factor);
    painter.SetBrush(sp::Brush(layer.color));

    // Yalnızca görünür aralıktaki sütunları çiz. Kamera ne kadar uzağa
    // giderse gitsin çizilen nesne sayısı sabit kalır.
    const float half_w = static_cast<float>(Width()) * 0.5F / mZoom;
    const float world_left = mCamX * layer.factor - half_w - layer.radius;
    const float world_right = mCamX * layer.factor + half_w + layer.radius;

    const auto first =
        static_cast<int32_t>(std::floor(world_left / layer.spacing));
    const auto last =
        static_cast<int32_t>(std::ceil(world_right / layer.spacing));

    for (int32_t i = first; i <= last; ++i) {
      const float x = static_cast<float>(i) * layer.spacing;
      // Tepe yüksekliği deterministik olarak konumdan türetilir.
      const float h =
          layer.radius * (0.7F + 0.5F * std::sin(static_cast<float>(i) * 1.7F));
      painter.FillCircle(x, layer.y_offset + h, h);
    }
    painter.Restore();
  }

  /// @brief HUD kameradan ETKİLENMEZ: Restore'dan sonra, ekran
  ///        koordinatlarında çizilir.
  void DrawHud(sp::Painter& painter, const sp::Point& world) const {
    painter.SetPen(sp::Pen::NoPen());
    painter.SetBrush(sp::Brush(sp::Color{0, 0, 0, 150}));
    painter.FillRect(10.0F, 10.0F, 330.0F, 74.0F);

    // Metin yerine çubuk göstergeler: örnek font dosyasına bağlı olmasın.
    painter.SetBrush(sp::Brush(sp::Color{120, 200, 255, 230}));
    const float zoom_t = (mZoom - kMinZoom) / (kMaxZoom - kMinZoom);
    painter.FillRect(20.0F, 22.0F, 300.0F * zoom_t, 12.0F);

    painter.SetBrush(sp::Brush(sp::Color{255, 200, 100, 230}));
    // Dünya konumunu -2000..2000 aralığında çubuklaştır.
    const float wx =
        std::fmax(0.0F, std::fmin(1.0F, (world.x + 2000.0F) / 4000.0F));
    const float wy =
        std::fmax(0.0F, std::fmin(1.0F, (world.y + 2000.0F) / 4000.0F));
    painter.FillRect(20.0F, 42.0F, 300.0F * wx, 8.0F);
    painter.FillRect(20.0F, 56.0F, 300.0F * wy, 8.0F);

    painter.SetPen(sp::Pen(sp::Color{255, 255, 255, 60}, 1.0F));
    painter.DrawRect(10.0F, 10.0F, 330.0F, 74.0F);
  }

  float mCamX{0.0F};
  float mCamY{0.0F};
  float mZoom{1.0F};
  float mMouseX{0.0F};
  float mMouseY{0.0F};
  bool mLeft{false};
  bool mRight{false};
  bool mUp{false};
  bool mDown{false};
};

int main() {
  sp::AppConfig config;
  config.title = "SDLPainter — camera_scroll: WASD gez, tekerlek zoom";
  config.width = 1000;
  config.height = 700;
  config.stats_overlay = sp::StatsOverlayMode::kDetailed;

  CameraScrollDemo app(config);
  return app.Run();
}
