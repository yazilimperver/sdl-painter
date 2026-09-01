/// @brief breakout — gerçek zamanlı tam oyun: döngü, girdi, çarpışma, skor.
///
/// `tictactoe` sıra tabanlı ve statik: hiçbir şey kendiliğinden hareket
/// etmiyordu. Bu örnek eksik kalan tarafı kapatıyor — sürekli hareket, kare
/// hızından bağımsız fizik, her karede çarpışma testi ve bir durum makinesi
/// (menü → oyun → kazandın/kaybettin) bir arada.
///
/// Mimari ayrımı: çarpışma matematiği çizimden tamamen ayrık, saf
/// fonksiyonlar olarak [`collision_logic.h`](collision_logic.h) içinde. Bu
/// sayede pencere açmadan birim testi yazılabiliyor
/// (`tests/test_collision_logic.cpp`) — `tictactoe_logic.h` ile aynı kalıp.
///
/// Kontroller:
///   ← / → veya A/D — raket
///   Fare           — raket (fareyi izler)
///   SPACE          — başlat / topu fırlat
///   R              — yeniden başlat
///   ESC            — çıkış
///
/// Metin sistemde bulunan bir TTF fontuyla çizilir
/// ([`example_font.h`](../example_font.h)); font bulunamazsa oyun metinsiz
/// oynanmaya devam eder — örneğin varlık dosyası bağımlılığı yoktur.

#include "sdl_painter/app/application.h"
#include "sdl_painter/font.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "collision_logic.h"
#include "example_font.h"

namespace sp = sdl_painter;
namespace bo = breakout;

namespace {

constexpr int32_t kBrickCols = 11;
constexpr int32_t kBrickRows = 6;
constexpr float kBrickGap = 6.0F;
constexpr float kFieldTop = 90.0F;
constexpr float kPaddleSpeed = 780.0F;
constexpr float kBallSpeed = 460.0F;
constexpr float kBallRadius = 8.0F;
constexpr float kMaxBounceAngle = 1.05F;  ///< ~60°, radyan.

enum class State { kMenu, kPlaying, kWon, kLost };

struct Brick {
  bo::Aabb box;
  sp::Color color;
  bool alive{true};
};

}  // namespace

class BreakoutDemo : public sp::Application {
 public:
  using Application::Application;

 protected:
  bool OnInit() override {
    // Font bulunamazsa oyun metinsiz oynanmaya devam eder — örnek hiçbir
    // varlık dosyasına bağımlı olmamalı.
    const std::string path = example::FindSystemFont();
    if (!path.empty()) {
      mHudFont = std::make_shared<sp::Font>(path, 22);
      mTitleFont = std::make_shared<sp::Font>(path, 56);
      if (!mHudFont->IsValid()) {
        mHudFont.reset();
      }
      if (!mTitleFont->IsValid()) {
        mTitleFont.reset();
      }
    }
    Restart();
    return true;
  }

  void OnUpdate(float dt) override {
    if (mState != State::kPlaying) {
      return;
    }

    // --- Raket: durum tabanlı sürekli hareket ---
    float dir = 0.0F;
    if (mLeft) {
      dir -= 1.0F;
    }
    if (mRight) {
      dir += 1.0F;
    }
    mPaddle.x += dir * kPaddleSpeed * dt;
    mPaddle.x =
        bo::ClampTo(mPaddle.x, 0.0F, static_cast<float>(Width()) - mPaddle.w);

    if (!mLaunched) {
      // Top rakete yapışık bekler.
      mBall.x = mPaddle.CenterX();
      mBall.y = mPaddle.Top() - kBallRadius - 1.0F;
      return;
    }

    // --- Top: küçük adımlara bölünerek ilerletilir ---
    // Tek adımda ilerletmek, yüksek hızda tuğlanın içinden geçmeye
    // (tünelleme) yol açar. Adım sayısını yarıçapa göre seçmek bunu ucuza
    // çözer; sürekli çarpışma testine (CCD) gerek kalmaz.
    const float travel = std::sqrt(mVx * mVx + mVy * mVy) * dt;
    const auto steps =
        std::max(1, static_cast<int32_t>(travel / (kBallRadius * 0.5F)) + 1);
    const float sub_dt = dt / static_cast<float>(steps);

    for (int32_t s = 0; s < steps; ++s) {
      StepBall(sub_dt);
      if (mState != State::kPlaying) {
        return;
      }
    }
  }

  void OnRender(sp::Painter& painter) override {
    painter.Clear(sp::Color{16, 18, 28, 255});

    DrawBricks(painter);
    DrawPaddleAndBall(painter);
    DrawHud(painter);

    if (mState != State::kPlaying) {
      DrawOverlay(painter);
    }
  }

  void OnKeyDown(const sp::KeyEvent& event) override {
    SetKey(event.key, true);
    if (event.repeat) {
      return;
    }
    switch (event.key) {
      case sp::Key::kSpace:
        if (mState == State::kMenu || mState == State::kWon ||
            mState == State::kLost) {
          Restart();
          mState = State::kPlaying;
        } else if (!mLaunched) {
          Launch();
        }
        break;
      case sp::Key::kR:
        Restart();
        break;
      case sp::Key::kEscape:
        Quit();
        break;
      default:
        break;
    }
  }

  void OnKeyUp(const sp::KeyEvent& event) override { SetKey(event.key, false); }

  void OnMouseMove(const sp::MouseMoveEvent& event) override {
    if (mState != State::kPlaying) {
      return;
    }
    mPaddle.x = bo::ClampTo(event.x - mPaddle.w * 0.5F, 0.0F,
                            static_cast<float>(Width()) - mPaddle.w);
  }

  void OnMouseButtonDown(const sp::MouseButtonEvent&) override {
    if (mState == State::kPlaying && !mLaunched) {
      Launch();
    } else if (mState != State::kPlaying) {
      Restart();
      mState = State::kPlaying;
    }
  }

  void OnResize(const sp::ResizeEvent&) override { Restart(); }

 private:
  void SetKey(sp::Key key, bool down) {
    if (key == sp::Key::kA || key == sp::Key::kLeft) {
      mLeft = down;
    } else if (key == sp::Key::kD || key == sp::Key::kRight) {
      mRight = down;
    }
  }

  void Launch() {
    mLaunched = true;
    mVx = kBallSpeed * 0.35F;
    mVy = -kBallSpeed;
  }

  void Restart() {
    const auto w = static_cast<float>(Width());
    const auto h = static_cast<float>(Height());

    mPaddle = bo::Aabb{w * 0.5F - 70.0F, h - 60.0F, 140.0F, 16.0F};
    mBall =
        bo::Circle{w * 0.5F, mPaddle.Top() - kBallRadius - 1.0F, kBallRadius};
    mLaunched = false;
    mScore = 0;
    mLives = 3;

    mBricks.clear();
    const float margin = 40.0F;
    const float total_w = w - margin * 2.0F;
    const float bw = (total_w - kBrickGap * (kBrickCols - 1)) / kBrickCols;
    const float bh = 26.0F;

    for (int32_t r = 0; r < kBrickRows; ++r) {
      for (int32_t c = 0; c < kBrickCols; ++c) {
        Brick b;
        b.box = bo::Aabb{margin + static_cast<float>(c) * (bw + kBrickGap),
                         kFieldTop + static_cast<float>(r) * (bh + kBrickGap),
                         bw, bh};
        // Üst sıralar daha değerli — renk sırayı belli etsin.
        const float t = static_cast<float>(r) / (kBrickRows - 1);
        b.color = sp::Color{static_cast<uint8_t>(240 - 100 * t),
                            static_cast<uint8_t>(90 + 130 * t),
                            static_cast<uint8_t>(90 + 120 * (1.0F - t)), 235};
        mBricks.push_back(b);
      }
    }
  }

  void StepBall(float dt) {
    mBall.x += mVx * dt;
    mBall.y += mVy * dt;

    const auto w = static_cast<float>(Width());

    // Duvarlar.
    if (mBall.x - mBall.r < 0.0F) {
      mBall.x = mBall.r;
      mVx = std::fabs(mVx);
    } else if (mBall.x + mBall.r > w) {
      mBall.x = w - mBall.r;
      mVx = -std::fabs(mVx);
    }
    if (mBall.y - mBall.r < 0.0F) {
      mBall.y = mBall.r;
      mVy = std::fabs(mVy);
    }

    // Raket — yalnızca aşağı inerken; yoksa top rakete "yapışır".
    if (mVy > 0.0F && bo::Intersects(mBall, mPaddle)) {
      const float t = bo::PaddleBounce(mBall.x, mPaddle);
      const float angle = t * kMaxBounceAngle;
      mVx = kBallSpeed * std::sin(angle);
      mVy = -kBallSpeed * std::cos(angle);
      mBall.y = mPaddle.Top() - mBall.r - 0.5F;
    }

    // Tuğlalar — kare başına en fazla bir kırılma; aynı adımda iki tuğla
    // birden kırmak sekme yönünü belirsizleştirir.
    for (auto& brick : mBricks) {
      if (!brick.alive) {
        continue;
      }
      const bo::Axis axis = bo::ResolveAxis(mBall, brick.box);
      if (axis == bo::Axis::kNone) {
        continue;
      }
      brick.alive = false;
      mScore += 10;
      if (axis == bo::Axis::kHorizontal) {
        mVx = -mVx;
      } else {
        mVy = -mVy;
      }
      break;
    }

    if (AllBricksCleared()) {
      mState = State::kWon;
      return;
    }

    // Aşağı düştü.
    if (mBall.y - mBall.r > static_cast<float>(Height())) {
      --mLives;
      mLaunched = false;
      if (mLives <= 0) {
        mState = State::kLost;
      }
    }
  }

  [[nodiscard]] bool AllBricksCleared() const {
    for (const auto& b : mBricks) {
      if (b.alive) {
        return false;
      }
    }
    return true;
  }

  void DrawBricks(sp::Painter& painter) const {
    painter.SetPen(sp::Pen::NoPen());
    for (const auto& b : mBricks) {
      if (!b.alive) {
        continue;
      }
      painter.SetBrush(sp::Brush(b.color));
      painter.FillRect(b.box.x, b.box.y, b.box.w, b.box.h);
    }
  }

  void DrawPaddleAndBall(sp::Painter& painter) const {
    painter.SetPen(sp::Pen::NoPen());
    painter.SetBrush(sp::Brush(sp::Color{230, 235, 245, 245}));
    painter.FillRect(mPaddle.x, mPaddle.y, mPaddle.w, mPaddle.h);

    painter.SetBrush(sp::Brush(sp::Color{255, 220, 120, 250}));
    painter.FillCircle(mBall.x, mBall.y, mBall.r);

    if (!mLaunched && mState == State::kPlaying) {
      // Fırlatma yönü ipucu — kesikli çizgi.
      sp::Pen hint(sp::Color{255, 255, 255, 110}, 2.0F);
      hint.SetDashPattern({8.0F, 6.0F});
      painter.SetPen(hint);
      painter.DrawLine(mBall.x, mBall.y, mBall.x + 60.0F, mBall.y - 110.0F);
    }
  }

  void DrawHud(sp::Painter& painter) const {
    painter.SetPen(sp::Pen::NoPen());

    if (mHudFont) {
      painter.SetFont(mHudFont);
      // DrawText kalemin rengini kullanir.
      painter.SetPen(sp::Pen(sp::Color{225, 232, 245, 245}, 1.0F));
      painter.DrawText(24.0F, 44.0F, "SKOR  " + std::to_string(mScore));

      const std::string lives = "CAN  " + std::to_string(mLives);
      int32_t tw = 0;
      int32_t th = 0;
      if (mHudFont->MeasureText(lives, tw, th)) {
        // Sag kenara hizala: metnin genisligi olculmeden bu yapilamaz.
        painter.DrawText(static_cast<float>(Width()) - tw - 24.0F, 44.0F,
                         lives);
      } else {
        painter.DrawText(static_cast<float>(Width()) - 120.0F, 44.0F, lives);
      }
      painter.SetPen(sp::Pen::NoPen());
      return;
    }

    // Font yoksa sekil tabanli yedek gosterim.
    painter.SetBrush(sp::Brush(sp::Color{120, 200, 255, 230}));
    const int32_t bars = mScore / 10;
    for (int32_t i = 0; i < bars && i < 40; ++i) {
      painter.FillRect(24.0F + static_cast<float>(i) * 8.0F, 30.0F, 5.0F,
                       18.0F);
    }
    painter.SetBrush(sp::Brush(sp::Color{255, 110, 110, 235}));
    for (int32_t i = 0; i < mLives; ++i) {
      painter.FillCircle(
          static_cast<float>(Width()) - 34.0F - static_cast<float>(i) * 30.0F,
          39.0F, 9.0F);
    }
  }

  void DrawOverlay(sp::Painter& painter) const {
    const auto w = static_cast<float>(Width());
    const auto h = static_cast<float>(Height());

    painter.SetPen(sp::Pen::NoPen());
    painter.SetBrush(sp::Brush(sp::Color{0, 0, 0, 175}));
    painter.FillRect(0.0F, 0.0F, w, h);

    const sp::Color accent =
        mState == State::kWon    ? sp::Color{110, 230, 140, 245}
        : mState == State::kLost ? sp::Color{240, 100, 100, 245}
                                 : sp::Color{200, 210, 240, 245};

    const char* title = mState == State::kWon    ? "KAZANDIN!"
                        : mState == State::kLost ? "OYUN BITTI"
                                                 : "BREAKOUT";
    const char* hint = mState == State::kMenu ? "Baslamak icin SPACE"
                                              : "Tekrar oynamak icin SPACE";

    if (mTitleFont && mHudFont) {
      // Rect asiri yuklemesi metni yatayda kendisi ortalar; dikey konumu
      // dikdortgenin yerini secerek ayarliyoruz.
      painter.SetFont(mTitleFont);
      painter.SetPen(sp::Pen(accent, 1.0F));
      painter.DrawText(sp::Rect{0.0F, h * 0.34F, w, 70.0F}, title,
                       sp::Alignment::kCenter);

      painter.SetFont(mHudFont);
      painter.SetPen(sp::Pen(sp::Color{215, 222, 235, 235}, 1.0F));
      painter.DrawText(sp::Rect{0.0F, h * 0.34F + 90.0F, w, 30.0F},
                       "SKOR  " + std::to_string(mScore),
                       sp::Alignment::kCenter);
      painter.DrawText(sp::Rect{0.0F, h * 0.34F + 130.0F, w, 30.0F}, hint,
                       sp::Alignment::kCenter);
      painter.SetPen(sp::Pen::NoPen());
      return;
    }

    // Font yoksa: durum simgesi (kazandin → yukari ucgen, kaybettin → asagi).
    painter.SetBrush(sp::Brush(accent));
    const float cx = w * 0.5F;
    const float cy = h * 0.45F;
    if (mState == State::kWon) {
      painter.FillPolygon({{cx, cy - 60.0F},
                           {cx + 60.0F, cy + 40.0F},
                           {cx - 60.0F, cy + 40.0F}});
    } else if (mState == State::kLost) {
      painter.FillPolygon({{cx, cy + 60.0F},
                           {cx + 60.0F, cy - 40.0F},
                           {cx - 60.0F, cy - 40.0F}});
    } else {
      painter.FillRect(cx - 50.0F, cy - 50.0F, 100.0F, 100.0F);
    }
    sp::Pen mark(accent, 3.0F);
    mark.SetDashPattern({14.0F, 9.0F});
    painter.SetPen(mark);
    painter.DrawRect(cx - 140.0F, cy + 90.0F, 280.0F, 46.0F);
  }

  std::vector<Brick> mBricks;
  bo::Aabb mPaddle;
  bo::Circle mBall;
  float mVx{0.0F};
  float mVy{0.0F};
  int32_t mScore{0};
  int32_t mLives{3};
  State mState{State::kMenu};
  bool mLaunched{false};
  bool mLeft{false};
  bool mRight{false};
  std::shared_ptr<sp::Font> mHudFont;
  std::shared_ptr<sp::Font> mTitleFont;
};

int main() {
  sp::AppConfig config;
  config.title = "SDLPainter — breakout: SPACE ile basla";
  config.width = 1000;
  config.height = 720;
  config.resizable = false;  // Yeniden boyutlandırma sahayı sıfırlar.
  config.stats_overlay = sp::StatsOverlayMode::kDetailed;

  BreakoutDemo app(config);
  return app.Run();
}
