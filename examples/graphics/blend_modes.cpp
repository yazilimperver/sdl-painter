/// @brief blend_modes — karıştırma modları ve bedelleri.
///
/// Dört mod, aynı sahne üzerinde yan yana. Üstteki üç panel her modun ne
/// yaptığını gösterir; alttaki parçacık alanı neden işe yaradığını:
/// toplamalı karıştırmayla üst üste binen kıvılcımlar birbirini söndürmek
/// yerine parlatır ve alev/ışık hissi ancak böyle doğar.
///
/// Modun bedeli de saklanmıyor. Renk ve tint vertex'te taşındığı için
/// batch'i kırmaz; karıştırma modu bir GPU durumudur ve taşınamaz. Her
/// değişim bir flush demek. SPACE ile "mod başına grupla" ve "şekil başına
/// değiştir" arasında geçiş yap, F1 katmanındaki draw call sayacına bak:
/// aynı görüntü, çok farklı maliyet. Doğru kullanım, aynı modu kullanan
/// çizimleri bir arada tutmaktır.
///
/// Vulkan'da karıştırma pipeline'ın sabit durumudur; mod başına ayrı bir
/// pipeline varyantı başlangıçta üretilir, çizim sırasında derleme olmaz.
/// İki backend aynı faktörleri kullanır, yani aynı görüntüyü verir.
///
/// Kontroller:
///   SPACE — gruplu / şekil başına mod değişimi (maliyet farkı)
///   1..4  — parçacık alanının modu: alpha / additive / multiply / none
///   F1    — kare istatistiği
///   ESC   — çıkış

#include "sdl_painter/app/application.h"
#include "sdl_painter/font.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "example_font.h"

namespace sp = sdl_painter;

namespace {

constexpr std::size_t kSparkCount = 900;

const std::array<sp::BlendMode, 4> kModes = {
    sp::BlendMode::kAlpha, sp::BlendMode::kAdditive, sp::BlendMode::kMultiply,
    sp::BlendMode::kNone};
const std::array<const char*, 4> kModeNames = {"kAlpha", "kAdditive",
                                               "kMultiply", "kNone"};

struct Spark {
  float x{0.0F};
  float y{0.0F};
  float vx{0.0F};
  float vy{0.0F};
  float life{1.0F};
  sp::Color color;
};

}  // namespace

class BlendModesDemo : public sp::Application {
 public:
  using Application::Application;

 protected:
  bool OnInit() override {
    const std::string path = example::FindSystemFont();
    if (!path.empty()) {
      mFont = std::make_shared<sp::Font>(path, 15);
      if (!mFont->IsValid()) {
        mFont.reset();
      }
    }
    mSparks.resize(kSparkCount);
    for (auto& s : mSparks) {
      Respawn(s);
      s.life = mUnit(mRng);
    }
    return true;
  }

  void OnUpdate(float dt) override {
    for (auto& s : mSparks) {
      s.x += s.vx * dt;
      s.y += s.vy * dt;
      s.vy += 120.0F * dt;
      s.life -= dt * 0.55F;
      if (s.life <= 0.0F) {
        Respawn(s);
      }
    }
  }

  void OnRender(sp::Painter& painter) override {
    painter.Clear(sp::Color{16, 18, 26, 255});

    DrawModeStrip(painter, 70.0F);
    DrawSparkField(painter, 330.0F);
  }

  void OnKeyDown(const sp::KeyEvent& event) override {
    if (event.repeat) {
      return;
    }
    switch (event.key) {
      case sp::Key::kSpace:
        mGrouped = !mGrouped;
        break;
      case sp::Key::k1:
        mFieldMode = 0;
        break;
      case sp::Key::k2:
        mFieldMode = 1;
        break;
      case sp::Key::k3:
        mFieldMode = 2;
        break;
      case sp::Key::k4:
        mFieldMode = 3;
        break;
      case sp::Key::kEscape:
        Quit();
        break;
      default:
        break;
    }
  }

 private:
  void Label(sp::Painter& painter, float x, float y, float w,
             const std::string& text, const sp::Color& color) {
    if (!mFont) {
      return;
    }
    painter.SetFont(mFont);
    painter.SetPen(sp::Pen(color, 1.0F));
    painter.DrawText(sp::Rect{x, y, w, 20.0F}, text, sp::Alignment::kCenter);
    painter.SetPen(sp::Pen::NoPen());
  }

  /// @brief Her mod için aynı üç üst üste binen daire.
  void DrawModeStrip(sp::Painter& painter, float y) {
    const float panel_w = 230.0F;
    for (std::size_t i = 0; i < kModes.size(); ++i) {
      const float x = 20.0F + static_cast<float>(i) * (panel_w + 10.0F);

      // Zemin: karistirmanin uzerine uygulanacagi hedef.
      painter.SetBlendMode(sp::BlendMode::kAlpha);
      painter.SetPen(sp::Pen::NoPen());
      painter.SetBrush(sp::Brush::LinearGradient(
          {x, y}, {x + panel_w, y + 180.0F}, sp::Color{70, 60, 110, 255},
          sp::Color{25, 30, 55, 255}));
      painter.FillRect(x, y, panel_w, 180.0F);

      // Uc daire, secilen modla.
      painter.SetBlendMode(kModes[i]);
      const std::array<sp::Color, 3> kCircles = {sp::Color{255, 90, 90, 190},
                                                 sp::Color{90, 255, 120, 190},
                                                 sp::Color{110, 160, 255, 190}};
      for (std::size_t k = 0; k < 3; ++k) {
        const float a = 6.28318F * static_cast<float>(k) / 3.0F;
        painter.SetBrush(sp::Brush(kCircles[k]));
        painter.FillCircle(x + panel_w * 0.5F + std::cos(a) * 38.0F,
                           y + 90.0F + std::sin(a) * 38.0F, 55.0F);
      }

      painter.SetBlendMode(sp::BlendMode::kAlpha);
      Label(painter, x, y + 186.0F, panel_w, kModeNames[i],
            sp::Color{215, 222, 240, 235});
    }
  }

  /// @brief Kıvılcım alanı — modun *ne işe yaradığı* ve maliyeti.
  void DrawSparkField(sp::Painter& painter, float y) {
    const auto w = static_cast<float>(Width());
    const float h = 300.0F;

    painter.SetBlendMode(sp::BlendMode::kAlpha);
    painter.SetPen(sp::Pen::NoPen());
    painter.SetBrush(sp::Brush(sp::Color{10, 10, 16, 255}));
    painter.FillRect(20.0F, y, w - 40.0F, h);

    if (mGrouped) {
      // DOGRU KULLANIM: ayni modu kullanan cizimler bir arada → tek flush.
      painter.SetBlendMode(kModes[static_cast<std::size_t>(mFieldMode)]);
      for (const auto& s : mSparks) {
        painter.SetBrush(sp::Brush(Faded(s)));
        painter.FillCircle(s.x, s.y, 4.0F);
      }
    } else {
      // KOTU KULLANIM: her sekilden once mod yaziliyor. Ayni goruntu, ama
      // her degisim bir flush → parcacik basina draw call.
      for (std::size_t i = 0; i < mSparks.size(); ++i) {
        painter.SetBlendMode(
            kModes[i % 2 == 0 ? static_cast<std::size_t>(mFieldMode) : 0]);
        painter.SetBrush(sp::Brush(Faded(mSparks[i])));
        painter.FillCircle(mSparks[i].x, mSparks[i].y, 4.0F);
      }
    }

    painter.SetBlendMode(sp::BlendMode::kAlpha);
    Label(painter, 20.0F, y + h + 6.0F, w - 40.0F,
          mGrouped ? "mod basina grupli (SPACE ile degistir)"
                   : "sekil basina mod degisimi — draw call sayacina bak",
          mGrouped ? sp::Color{140, 230, 160, 235}
                   : sp::Color{255, 140, 120, 235});
  }

  [[nodiscard]] static sp::Color Faded(const Spark& s) {
    sp::Color c = s.color;
    const float t = s.life < 0.0F ? 0.0F : (s.life > 1.0F ? 1.0F : s.life);
    c.a = static_cast<uint8_t>(255.0F * t);
    return c;
  }

  void Respawn(Spark& s) {
    const auto w = static_cast<float>(Width());
    s.x = w * 0.5F + (mUnit(mRng) - 0.5F) * 120.0F;
    s.y = 330.0F + 250.0F;
    const float angle = -3.14159265F * (0.25F + 0.5F * mUnit(mRng));
    const float speed = 90.0F + 200.0F * mUnit(mRng);
    s.vx = std::cos(angle) * speed;
    s.vy = std::sin(angle) * speed;
    s.life = 1.0F;
    const float t = mUnit(mRng);
    s.color = sp::Color{255, static_cast<uint8_t>(120.0F + 120.0F * t),
                        static_cast<uint8_t>(40.0F + 80.0F * t), 255};
  }

  std::shared_ptr<sp::Font> mFont;
  std::vector<Spark> mSparks;
  std::mt19937 mRng{7};
  std::uniform_real_distribution<float> mUnit{0.0F, 1.0F};
  int32_t mFieldMode{1};  // varsayilan: additive (etkisi en gorunur)
  bool mGrouped{true};
};

int main() {
  sp::AppConfig config;
  config.title = "SDLPainter — blend_modes: SPACE maliyet, 1-4 mod";
  config.width = 990;
  config.height = 700;
  config.vsync = false;
  config.stats_overlay = sp::StatsOverlayMode::kDetailed;

  BlendModesDemo app(config);
  return app.Run();
}
