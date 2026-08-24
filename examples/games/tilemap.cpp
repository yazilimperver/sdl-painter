/// @brief tilemap — tek atlastan karo izgarasi çizimi.
///
/// İki şeyi birlikte kanıtlar:
///
///   1. **Draw call sayısı harita boyutundan bağımsızdır.** Karolar tek bir
///      dokudan geldiği için hepsi aynı batch'e girer; 512x512'lik bir harita
///      da 64x64'lük bir harita da aynı sayıda draw call üretir.
///
///   2. **Vertex sayısı ekran boyutundan bağımsızdır.** Görünür alan elemesi
///      (culling) olmadan her karede tüm harita tessellate edilirdi. C tuşuyla
///      elemeyi kapat ve F1 katmanındaki vertex/CPU sayaçlarına bak: fark
///      1000 katın üzerinde.
///
/// Eleme, kamera dikdörtgeninin karo indekslerine çevrilmesinden ibaret —
/// bir uzamsal veri yapısına gerek yoktur, çünkü ızgara zaten indekslidir.
///
/// Kontroller:
///   WASD / oklar — gez
///   Tekerlek     — zoom
///   C            — görünür alan filtresini aç/kapat
///   G            — karo ızgara çizgileri
///   1/2/3        — harita 64² / 256² / 512²
///   ESC          — çıkış

#include "sdl_painter/app/application.h"
#include "sdl_painter/image.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sp = sdl_painter;

namespace {

constexpr float kTileSize = 48.0F;
constexpr int32_t kAtlasTile = 32;  ///< Atlastaki bir karonun piksel boyu.
constexpr int32_t kAtlasCols = 4;   ///< Atlas 4x1 karo içerir.
constexpr float kCameraSpeed = 700.0F;

/// @brief 4 karoluk atlas üretir (çim, kum, taş, su).
sp::Image MakeTileAtlas() {
  const int32_t w = kAtlasTile * kAtlasCols;
  const int32_t h = kAtlasTile;
  std::vector<uint8_t> px(static_cast<std::size_t>(w) * h * 4, 255);

  const uint8_t base[kAtlasCols][3] = {
      {70, 130, 70}, {200, 180, 120}, {120, 120, 130}, {60, 110, 180}};

  for (int32_t t = 0; t < kAtlasCols; ++t) {
    for (int32_t y = 0; y < h; ++y) {
      for (int32_t x = 0; x < kAtlasTile; ++x) {
        // Deterministik doku gürültüsü — karolar düz renk görünmesin.
        const int32_t n = ((x * 7 + y * 13 + t * 31) % 17) - 8;
        const std::size_t idx =
            (static_cast<std::size_t>(y) * w + (t * kAtlasTile + x)) * 4;
        for (int32_t c = 0; c < 3; ++c) {
          const int32_t v = static_cast<int32_t>(base[t][c]) + n * 2;
          px[idx + static_cast<std::size_t>(c)] =
              static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
        }
        px[idx + 3] = 255;
      }
    }
  }
  return sp::Image::CreateFromData(px.data(), w, h, 4);
}

}  // namespace

class TilemapDemo : public sp::Application {
 public:
  using Application::Application;

 protected:
  bool OnInit() override {
    mAtlas = MakeTileAtlas();
    Generate(256);
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
    mCamX += dx * kCameraSpeed * dt / mZoom;
    mCamY += dy * kCameraSpeed * dt / mZoom;
  }

  void OnRender(sp::Painter& painter) override {
    painter.Clear(sp::Color{14, 16, 22, 255});

    painter.Save();
    painter.Translate(static_cast<float>(Width()) * 0.5F,
                      static_cast<float>(Height()) * 0.5F);
    painter.Scale(mZoom, mZoom);
    painter.Translate(-mCamX, -mCamY);

    int32_t x0 = 0;
    int32_t y0 = 0;
    int32_t x1 = mSize - 1;
    int32_t y1 = mSize - 1;

    if (mCulling) {
      // Kamera dikdörtgenini karo indekslerine çevir. Tek iş bu.
      const float half_w = static_cast<float>(Width()) * 0.5F / mZoom;
      const float half_h = static_cast<float>(Height()) * 0.5F / mZoom;
      x0 = Clamp(
          static_cast<int32_t>(std::floor((mCamX - half_w) / kTileSize)) - 1);
      y0 = Clamp(
          static_cast<int32_t>(std::floor((mCamY - half_h) / kTileSize)) - 1);
      x1 = Clamp(static_cast<int32_t>(std::ceil((mCamX + half_w) / kTileSize)) +
                 1);
      y1 = Clamp(static_cast<int32_t>(std::ceil((mCamY + half_h) / kTileSize)) +
                 1);
    }

    for (int32_t ty = y0; ty <= y1; ++ty) {
      for (int32_t tx = x0; tx <= x1; ++tx) {
        const uint8_t tile = mTiles[Index(tx, ty)];
        const sp::Rect src{static_cast<float>(tile) * kAtlasTile, 0.0F,
                           static_cast<float>(kAtlasTile),
                           static_cast<float>(kAtlasTile)};
        const sp::Rect dst{static_cast<float>(tx) * kTileSize,
                           static_cast<float>(ty) * kTileSize, kTileSize,
                           kTileSize};
        painter.DrawImage(mAtlas, src, dst);
      }
    }

    if (mGrid) {
      // Izgara çizgileri: karolarla AYNI batch'e giremez (dokusuz geometri),
      // bu yüzden ayrı bir draw call daha üretir — sayaçta görülür.
      sp::Pen grid(sp::Color{255, 255, 255, 40}, 1.0F / mZoom);
      painter.SetPen(grid);
      for (int32_t tx = x0; tx <= x1 + 1; ++tx) {
        const float x = static_cast<float>(tx) * kTileSize;
        painter.DrawLine(x, static_cast<float>(y0) * kTileSize, x,
                         static_cast<float>(y1 + 1) * kTileSize);
      }
      for (int32_t ty = y0; ty <= y1 + 1; ++ty) {
        const float y = static_cast<float>(ty) * kTileSize;
        painter.DrawLine(static_cast<float>(x0) * kTileSize, y,
                         static_cast<float>(x1 + 1) * kTileSize, y);
      }
    }

    painter.Restore();

    // Eleme kapalıyken uyarı şeridi.
    if (!mCulling) {
      painter.SetPen(sp::Pen::NoPen());
      painter.SetBrush(sp::Brush(sp::Color{200, 60, 60, 200}));
      painter.FillRect(0.0F, 0.0F, static_cast<float>(Width()), 6.0F);
    }
  }

  void OnMouseWheel(const sp::MouseWheelEvent& event) override {
    mZoom *= (event.dy > 0.0F) ? 1.15F : (1.0F / 1.15F);
    mZoom = std::fmax(0.15F, std::fmin(4.0F, mZoom));
  }

  void OnKeyDown(const sp::KeyEvent& event) override {
    SetKey(event.key, true);
    if (event.repeat) {
      return;
    }
    switch (event.key) {
      case sp::Key::kC:
        mCulling = !mCulling;
        break;
      case sp::Key::kG:
        mGrid = !mGrid;
        break;
      case sp::Key::k1:
        Generate(64);
        break;
      case sp::Key::k2:
        Generate(256);
        break;
      case sp::Key::k3:
        Generate(512);
        break;
      case sp::Key::kEscape:
        Quit();
        break;
      default:
        break;
    }
  }

  void OnKeyUp(const sp::KeyEvent& event) override { SetKey(event.key, false); }

 private:
  [[nodiscard]] int32_t Clamp(int32_t v) const {
    return v < 0 ? 0 : (v >= mSize ? mSize - 1 : v);
  }

  [[nodiscard]] std::size_t Index(int32_t x, int32_t y) const {
    return static_cast<std::size_t>(y) * mSize + static_cast<std::size_t>(x);
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

  /// @brief Deterministik "arazi": birkaç sinüsün toplamı eşiklenir.
  void Generate(int32_t size) {
    mSize = size;
    mTiles.assign(static_cast<std::size_t>(size) * size, 0);
    for (int32_t y = 0; y < size; ++y) {
      for (int32_t x = 0; x < size; ++x) {
        const float fx = static_cast<float>(x) * 0.08F;
        const float fy = static_cast<float>(y) * 0.08F;
        const float v =
            std::sin(fx) + std::sin(fy * 1.3F) + std::sin((fx + fy) * 0.6F);
        uint8_t tile = 0;
        if (v < -1.2F) {
          tile = 3;  // su
        } else if (v < -0.4F) {
          tile = 1;  // kum
        } else if (v > 1.4F) {
          tile = 2;  // taş
        }
        mTiles[Index(x, y)] = tile;
      }
    }
    // Haritanın ortasına git.
    mCamX = static_cast<float>(size) * kTileSize * 0.5F;
    mCamY = static_cast<float>(size) * kTileSize * 0.5F;
  }

  sp::Image mAtlas;
  std::vector<uint8_t> mTiles;
  int32_t mSize{256};
  float mCamX{0.0F};
  float mCamY{0.0F};
  float mZoom{1.0F};
  bool mCulling{true};
  bool mGrid{false};
  bool mLeft{false};
  bool mRight{false};
  bool mUp{false};
  bool mDown{false};
};

int main() {
  sp::AppConfig config;
  config.title = "SDLPainter — tilemap: C ile elemeyi kapat, sayaca bak";
  config.width = 1000;
  config.height = 700;
  config.vsync = false;
  config.stats_overlay = sp::StatsOverlayMode::kDetailed;

  TilemapDemo app(config);
  return app.Run();
}
