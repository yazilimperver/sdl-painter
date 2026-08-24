/// @brief sprite_animation — gerçek bir sprite sheet: atlas dilimleme ve aynalama.
///
/// `DrawImage(image, src_rect, dest_rect)` aşırı yüklemesinin gerçek hayattaki
/// tek kullanımı budur: tek bir dokuda ızgara hâlinde duran kareleri sırayla
/// göstermek. Tüm kareler **aynı dokuda** olduğu için sahnedeki bütün
/// karakterler tek draw call'a girer — kaç tane oldukları fark etmez.
///
/// Kullanılan sheet 8 × 4'lük bir ızgara (24×32 kare):
///
///   satır 0 → izleyiciye bakan   satır 2 → sola bakan
///   satır 1 → sırtı dönük        satır 3 → sağa bakan
///
/// **Satır 2 ile satır 3 birbirinin tam aynası** (kare kare doğrulandı). Bu
/// örneğin asıl iddiası da burada: SPACE ile sağa bakan karakterleri ya
/// sheet'in kendi 3. satırından ya da 2. satırı `ImageFlip::kHorizontal` ile
/// çevirerek çizersin — sonuç **piksel piksel aynıdır**. Yani aynalama, bir
/// sprite sheet'in yarısını taşımamanı sağlar.
///
/// Aynalamanın `Save`/`Scale(-1,1)`/`Restore` ile yapılan hâli de M tuşunda:
/// aynı görüntü, ama konumu elle telafi etmek gerekiyor ve transform yığınına
/// dokunuluyor.
///
/// Varlık: `assets/rpg_character_walk.png` — CC0, arikel
/// (bkz. [`assets/README.md`](../assets/README.md)). CMake dosyayı derleme
/// sırasında çalıştırılabilirin yanına kopyalar; örnek onu
/// `SDL_GetBasePath()` üzerinden bulur.
///
/// @note Bu örnek varsayılan (doğrusal) doku filtresini kullanır, yani piksel
///       kenarları 3× büyütmede yumuşar. Keskin piksel için
///       `Image::SetFilter(TextureFilter::kNearest)` — farkı yan yana gösteren
///       örnek: `pixel_art`.
///
/// Kontroller:
///   SPACE — sağ yönü: sheet'in kendi satırı / sola bakan satırın aynası
///   M     — aynalamayı ImageFlip yerine Scale(-1,1) ile yap
///   G     — atlasın tamamını ekranda göster (ızgara yerleşimi görünür)
///   J / K — animasyon hızı
///   ESC   — çıkış

#include "sdl_painter/app/application.h"
#include "sdl_painter/image.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>
#include <vector>

namespace sp = sdl_painter;

namespace {

constexpr int32_t kFrameW = 24;
constexpr int32_t kFrameH = 32;
constexpr int32_t kFrameCount = 8;  ///< Sütun sayısı = yürüyüş karesi.
constexpr int32_t kRowLeft = 2;     ///< Sola bakan satır.
constexpr int32_t kRowRight = 3;    ///< Sağa bakan satır (satır 2'nin aynası).
constexpr float kScale = 3.0F;      ///< Piksel sanatı büyütme katsayısı.
constexpr int32_t kWalkerCount = 18;
constexpr float kFrameDuration = 0.10F;

/// @brief Varlık dosyasının çalıştırılabilirin yanındaki yolu.
///
/// Kütüphane hiçbir dosyayı çalışma zamanında aramaz (ADR-009); bu yalnızca
/// örneğe ait bir varlık ve CMake onu buraya kopyalıyor.
std::string AssetPath(const char* name) {
  const char* base = SDL_GetBasePath();
  return (base != nullptr ? std::string(base) : std::string()) + "assets/" +
         name;
}

struct Walker {
  float x{0.0F};
  float y{0.0F};
  float speed{0.0F};
  float frame_time{0.0F};
  int32_t frame{0};
  bool facing_left{false};
};

}  // namespace

class SpriteAnimationDemo : public sp::Application {
 public:
  using Application::Application;

 protected:
  bool OnInit() override {
    const std::string path = AssetPath("rpg_character_walk.png");
    mSheet = sp::Image(path);
    if (!mSheet.IsValid()) {
      // Sessizce boş bir pencere göstermektense açık bir hata ver: bu örneğin
      // konusu zaten sprite sheet, onsuz gösterecek bir şeyi yok.
      SDL_Log("sprite_animation: sprite sheet yuklenemedi: %s", path.c_str());
      return false;
    }

    mWalkers.resize(kWalkerCount);
    for (int32_t i = 0; i < kWalkerCount; ++i) {
      Walker& wk = mWalkers[static_cast<std::size_t>(i)];
      wk.x = static_cast<float>(i % 6) * 155.0F + 70.0F;
      wk.y = static_cast<float>(i / 6) * 150.0F + 120.0F;
      wk.speed = 35.0F + static_cast<float>(i % 5) * 22.0F;
      wk.facing_left = (i % 2) == 1;
      wk.frame = i % kFrameCount;
    }
    return true;
  }

  void OnUpdate(float dt) override {
    const auto w = static_cast<float>(Width());
    for (auto& wk : mWalkers) {
      // Kare ilerletme SÜREYE bağlı: kare hızı değişse de yürüyüş aynı
      // hızda görünür.
      wk.frame_time += dt * mSpeedScale;
      while (wk.frame_time >= kFrameDuration) {
        wk.frame_time -= kFrameDuration;
        wk.frame = (wk.frame + 1) % kFrameCount;
      }

      const float dir = wk.facing_left ? -1.0F : 1.0F;
      wk.x += wk.speed * dir * dt * mSpeedScale;
      if (wk.x < 50.0F) {
        wk.x = 50.0F;
        wk.facing_left = false;
      } else if (wk.x > w - 50.0F) {
        wk.x = w - 50.0F;
        wk.facing_left = true;
      }
    }
  }

  void OnRender(sp::Painter& painter) override {
    painter.Clear(sp::Color{26, 28, 40, 255});

    if (mShowAtlas) {
      DrawWholeAtlas(painter);
      return;
    }

    for (const auto& wk : mWalkers) {
      DrawWalker(painter, wk);
    }
  }

  void OnKeyDown(const sp::KeyEvent& event) override {
    if (event.repeat) {
      return;
    }
    switch (event.key) {
      case sp::Key::kSpace:
        mMirrorRightFromLeftRow = !mMirrorRightFromLeftRow;
        UpdateTitle();
        break;
      case sp::Key::kM:
        mMirrorWithTransform = !mMirrorWithTransform;
        UpdateTitle();
        break;
      case sp::Key::kG:
        mShowAtlas = !mShowAtlas;
        break;
      case sp::Key::kK:
        mSpeedScale *= 1.25F;
        break;
      case sp::Key::kJ:
        mSpeedScale *= 0.8F;
        break;
      case sp::Key::kEscape:
        Quit();
        break;
      default:
        break;
    }
  }

 private:
  /// @brief Bir karakteri çiz — bu örneğin özü.
  void DrawWalker(sp::Painter& painter, const Walker& wk) const {
    const float dw = kFrameW * kScale;
    const float dh = kFrameH * kScale;
    const sp::Rect dst{wk.x - dw * 0.5F, wk.y, dw, dh};

    // Sola bakış her zaman sheet'in kendi satırından gelir.
    if (wk.facing_left) {
      painter.DrawImage(mSheet, FrameRect(kRowLeft, wk.frame), dst);
      return;
    }

    // Sağa bakış iki yoldan biriyle:
    if (!mMirrorRightFromLeftRow) {
      // (a) Sheet'in kendi sağa bakan satırı.
      painter.DrawImage(mSheet, FrameRect(kRowRight, wk.frame), dst);
      return;
    }

    // (b) Sola bakan satır, yatayda çevrilerek. Sonuç (a) ile aynı piksel.
    const sp::Rect src = FrameRect(kRowLeft, wk.frame);
    if (!mMirrorWithTransform) {
      // Tercih edilen yol: hedef dikdörtgen yerinde kalır, yalnızca UV
      // çevrilir. Ek vertex, ek draw call, transform yığınına dokunma yok.
      painter.DrawImage(mSheet, src, dst, sp::Color::White(),
                        sp::ImageFlip::kHorizontal);
    } else {
      // Aynı sonucun transform ile hâli: negatif ölçek kendi orijini
      // etrafında çevirdiği için konumu elle telafi etmek gerekir.
      painter.Save();
      painter.Translate(wk.x, wk.y);
      painter.Scale(-1.0F, 1.0F);
      painter.DrawImage(mSheet, src, sp::Rect{-dw * 0.5F, 0.0F, dw, dh});
      painter.Restore();
    }
  }

  /// @brief Atlasın tamamı, ızgara çizgileriyle — yerleşim görünür olsun.
  void DrawWholeAtlas(sp::Painter& painter) const {
    const float s = 6.0F;
    const float w = static_cast<float>(mSheet.Width()) * s;
    const float h = static_cast<float>(mSheet.Height()) * s;
    const float x = (static_cast<float>(Width()) - w) * 0.5F;
    const float y = (static_cast<float>(Height()) - h) * 0.5F;

    painter.DrawImage(mSheet, sp::Rect{x, y, w, h});

    sp::Pen grid(sp::Color{255, 255, 255, 90}, 1.0F);
    grid.SetDashPattern({6.0F, 5.0F});
    painter.SetPen(grid);
    for (int32_t c = 0; c <= kFrameCount; ++c) {
      const float gx = x + static_cast<float>(c * kFrameW) * s;
      painter.DrawLine(gx, y, gx, y + h);
    }
    for (int32_t r = 0; r <= 4; ++r) {
      const float gy = y + static_cast<float>(r * kFrameH) * s;
      painter.DrawLine(x, gy, x + w, gy);
    }

    // Aynalanan satır çiftini vurgula: 2 ve 3.
    painter.SetPen(sp::Pen(sp::Color{120, 255, 170, 230}, 3.0F));
    for (int32_t r : {kRowLeft, kRowRight}) {
      painter.DrawRect(x, y + static_cast<float>(r * kFrameH) * s, w,
                       static_cast<float>(kFrameH) * s);
    }
  }

  /// @brief Izgaradaki (satır, sütun) karesinin kaynak dikdörtgeni.
  static sp::Rect FrameRect(int32_t row, int32_t col) {
    return sp::Rect{static_cast<float>(col * kFrameW),
                    static_cast<float>(row * kFrameH),
                    static_cast<float>(kFrameW), static_cast<float>(kFrameH)};
  }

  void UpdateTitle() {
    std::string t = "SDLPainter — sprite_animation · saga bakis: ";
    if (!mMirrorRightFromLeftRow) {
      t += "sheet'in kendi satiri";
    } else {
      t += mMirrorWithTransform ? "Scale(-1,1) ile aynalama"
                                : "ImageFlip ile aynalama";
    }
    SetTitle(t);
  }

  sp::Image mSheet;
  std::vector<Walker> mWalkers;
  float mSpeedScale{1.0F};
  bool mMirrorRightFromLeftRow{false};
  bool mMirrorWithTransform{false};
  bool mShowAtlas{false};
};

int main() {
  sp::AppConfig config;
  config.title =
      "SDLPainter — sprite_animation · saga bakis: sheet'in kendi satiri";
  config.width = 1000;
  config.height = 620;
  config.stats_overlay = sp::StatsOverlayMode::kDetailed;

  SpriteAnimationDemo app(config);
  return app.Run();
}
