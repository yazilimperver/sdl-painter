/// @brief tictactoe — Application catisiyla eksiksiz bir uygulama.
///
/// Gelistirme Fazi 8 demosu (eski ad: phase8_tictactoe).
///
/// app_basics catinin temelini, game_loop sabit adimli oyun dongusunu gosterir.
/// Bu demo ise catinin girdi tarafini ve gercek bir uygulamanin akisini
/// gosterir — diger orneklerin hicbirinde bulunmayan yetenekler:
///
///   - OnMouseButtonDown  → tiklamayi tahta hucresine cevirme (hit testing)
///   - OnMouseMove        → imlecin uzerinde oldugu hucreyi vurgulama
///   - OnResize           → tahta geometrisini pencere boyutuna gore yeniden
///                          hesaplama (duyarli yerlesim)
///   - Uygulama durum makinesi (oynaniyor → kazanan/berabere → yeniden basla)
///   - Metin + sekil birlikte: durum cubugu, Rect + Alignment ile hizalama
///
/// Sira tabanli bir oyun oldugu icin TimingMode::kVariable yeterlidir; sabit
/// adima (game_loop) ihtiyac yoktur — iki zamanlama modunun ne zaman secilecegini
/// yan yana gosterir.
///
/// Oyun mantigi (kazanan tespiti, hit testing) tictactoe_logic.h icinde saf
/// fonksiyonlar olarak durur ve birim testlerle dogrulanir; bu dosya yalnizca
/// girdi ve cizimden sorumludur.
///
/// Fare sol tik: hamle.  R: yeniden basla.  ESC: cikis.

#include "sdl_painter/app/application.h"

#include <fstream>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>

#include "tictactoe_logic.h"

// windows.h (spdlog uzerinden dolayli gelebilir) DrawText'i DrawTextA/W
// makrosuna cevirir ve Painter::DrawText cagrisini bozar. painter.h bunu
// kendi icinde temizler, ancak windows.h ondan SONRA dahil edilirse makro
// geri doner — bu yuzden include sirasindan bagimsiz olarak burada da temizle.
#ifdef DrawText
#undef DrawText
#endif

namespace sp = sdl_painter;
namespace ttt = tictactoe;

namespace {

constexpr float kBoardMargin = 40.0F;   // tahta cevresi bosluk
constexpr float kStatusHeight = 90.0F;  // alt durum cubugu yuksekligi
constexpr float kGridWidth = 6.0F;      // izgara cizgi kalinligi
constexpr float kMarkWidth = 12.0F;     // X / O cizgi kalinligi
constexpr float kMarkInset = 0.24F;  // hucre kenarindan isaret bosluğu (oran)

const sp::Color kBackground{18, 22, 30, 255};
const sp::Color kGridColor{90, 105, 130, 255};
const sp::Color kMarkX{255, 120, 100, 255};
const sp::Color kMarkO{90, 190, 255, 255};
const sp::Color kHoverColor{255, 255, 255, 26};
const sp::Color kWinColor{120, 230, 150, 255};
const sp::Color kTextColor{225, 232, 240, 255};

/// @brief Sistemde mevcut bir TTF fontunu bulmaya calis; yoksa bos string.
std::string FindSystemFont() {
  const char* candidates[] = {
#ifdef _WIN32
      "C:/Windows/Fonts/arial.ttf",
      "C:/Windows/Fonts/calibri.ttf",
      "C:/Windows/Fonts/segoeui.ttf",
      "C:/Windows/Fonts/tahoma.ttf",
#else
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
      "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
      "/usr/share/fonts/TTF/DejaVuSans.ttf",
#endif
  };
  for (const char* path : candidates) {
    if (std::ifstream(path).good()) {
      return path;
    }
  }
  return {};
}

}  // namespace

/// @brief Fare girdisi, durum makinesi ve duyarli yerlesim ornegi.
class TicTacToeApp : public sp::Application {
 public:
  using Application::Application;

 protected:
  bool OnInit() override {
    const std::string font_path = FindSystemFont();
    if (!font_path.empty()) {
      mFont = std::make_shared<sp::Font>(font_path, 28);
      if (!mFont->IsValid()) {
        mFont.reset();
      }
    }
    // Font bulunamazsa demo metinsiz calismaya devam eder.
    if (!mFont) {
      spdlog::warn("Sistem fontu bulunamadi; durum metni cizilmeyecek.");
    }

    RecomputeLayout(Width(), Height());
    Reset();
    return true;
  }

  void OnResize(const sp::ResizeEvent& event) override {
    RecomputeLayout(event.width, event.height);
  }

  void OnMouseMove(const sp::MouseMoveEvent& event) override {
    mHoverCell =
        ttt::CellFromPoint(event.x, event.y, mBoardX, mBoardY, mBoardSize);
  }

  void OnMouseButtonDown(const sp::MouseButtonEvent& event) override {
    if (event.button != sp::MouseButton::kLeft) {
      return;
    }
    // Oyun bittiyse herhangi bir tik yeni oyun baslatir.
    if (mState != ttt::GameState::kPlaying) {
      Reset();
      return;
    }
    const int32_t cell =
        ttt::CellFromPoint(event.x, event.y, mBoardX, mBoardY, mBoardSize);
    if (!ttt::TryPlaceMark(mBoard, cell, mCurrent)) {
      return;
    }
    mState = ttt::EvaluateState(mBoard);
    mWinLine = ttt::FindWinLine(mBoard);
    if (mState == ttt::GameState::kPlaying) {
      mCurrent = ttt::NextPlayer(mCurrent);
    }
    UpdateTitle();
  }

  void OnKeyDown(const sp::KeyEvent& event) override {
    if (event.key == sp::Key::kEscape) {
      Quit();
    } else if (event.key == sp::Key::kR) {
      Reset();
    }
  }

  void OnRender(sp::Painter& painter) override {
    painter.Clear(kBackground);

    DrawHover(painter);
    DrawGrid(painter);
    DrawMarks(painter);
    DrawWinLine(painter);
    DrawStatus(painter);
  }

 private:
  /// @brief Tahtayi pencereye ortala; kenar uzunlugunu bosluklara gore sec.
  void RecomputeLayout(int32_t width, int32_t height) {
    const auto w = static_cast<float>(width);
    const auto h = static_cast<float>(height);
    const float usable_h = h - kStatusHeight - 2.0F * kBoardMargin;
    const float usable_w = w - 2.0F * kBoardMargin;
    mBoardSize = usable_w < usable_h ? usable_w : usable_h;
    if (mBoardSize < 0.0F) {
      mBoardSize = 0.0F;
    }
    mBoardX = (w - mBoardSize) * 0.5F;
    mBoardY = kBoardMargin;
  }

  void Reset() {
    mBoard = ttt::EmptyBoard();
    mCurrent = ttt::Cell::kX;
    mState = ttt::GameState::kPlaying;
    mWinLine = ttt::WinLine{};
    UpdateTitle();
  }

  void UpdateTitle() {
    std::string suffix;
    switch (mState) {
      case ttt::GameState::kWinX:
        suffix = "X kazandi";
        break;
      case ttt::GameState::kWinO:
        suffix = "O kazandi";
        break;
      case ttt::GameState::kDraw:
        suffix = "Berabere";
        break;
      case ttt::GameState::kPlaying:
        suffix = mCurrent == ttt::Cell::kX ? "sira: X" : "sira: O";
        break;
    }
    SetTitle("SDLPainter — tictactoe (Phase 8) (" + suffix + ")");
  }

  /// @brief Hucrenin sol ust kosesi.
  void CellOrigin(int32_t index, float& out_x, float& out_y) const {
    const float cell = mBoardSize / 3.0F;
    out_x = mBoardX + static_cast<float>(index % 3) * cell;
    out_y = mBoardY + static_cast<float>(index / 3) * cell;
  }

  /// @brief Hucrenin merkezi.
  void CellCenter(int32_t index, float& out_x, float& out_y) const {
    const float cell = mBoardSize / 3.0F;
    CellOrigin(index, out_x, out_y);
    out_x += cell * 0.5F;
    out_y += cell * 0.5F;
  }

  void DrawHover(sp::Painter& painter) const {
    if (mState != ttt::GameState::kPlaying || !ttt::IsValidIndex(mHoverCell)) {
      return;
    }
    if (mBoard[static_cast<std::size_t>(mHoverCell)] != ttt::Cell::kEmpty) {
      return;
    }
    const float cell = mBoardSize / 3.0F;
    float x = 0.0F;
    float y = 0.0F;
    CellOrigin(mHoverCell, x, y);
    painter.SetBrush(sp::Brush(kHoverColor));
    painter.FillRect(x, y, cell, cell);
  }

  void DrawGrid(sp::Painter& painter) const {
    const float cell = mBoardSize / 3.0F;
    painter.SetPen(sp::Pen(kGridColor, kGridWidth));
    for (int32_t i = 1; i < 3; ++i) {
      const float offset = static_cast<float>(i) * cell;
      // Dikey
      painter.DrawLine(mBoardX + offset, mBoardY, mBoardX + offset,
                       mBoardY + mBoardSize);
      // Yatay
      painter.DrawLine(mBoardX, mBoardY + offset, mBoardX + mBoardSize,
                       mBoardY + offset);
    }
  }

  void DrawMarks(sp::Painter& painter) const {
    const float cell = mBoardSize / 3.0F;
    const float inset = cell * kMarkInset;
    for (int32_t i = 0; i < 9; ++i) {
      const ttt::Cell mark = mBoard[static_cast<std::size_t>(i)];
      if (mark == ttt::Cell::kEmpty) {
        continue;
      }
      float x = 0.0F;
      float y = 0.0F;
      CellOrigin(i, x, y);
      if (mark == ttt::Cell::kX) {
        painter.SetPen(sp::Pen(kMarkX, kMarkWidth));
        painter.DrawLine(x + inset, y + inset, x + cell - inset,
                         y + cell - inset);
        painter.DrawLine(x + cell - inset, y + inset, x + inset,
                         y + cell - inset);
      } else {
        painter.SetPen(sp::Pen(kMarkO, kMarkWidth));
        painter.DrawCircle(x + cell * 0.5F, y + cell * 0.5F,
                           cell * 0.5F - inset);
      }
    }
  }

  void DrawWinLine(sp::Painter& painter) const {
    if (!mWinLine.valid) {
      return;
    }
    float ax = 0.0F;
    float ay = 0.0F;
    float cx = 0.0F;
    float cy = 0.0F;
    CellCenter(mWinLine.a, ax, ay);
    CellCenter(mWinLine.c, cx, cy);
    painter.SetPen(sp::Pen(kWinColor, kMarkWidth + 4.0F));
    painter.DrawLine(ax, ay, cx, cy);
  }

  void DrawStatus(sp::Painter& painter) {
    if (!mFont) {
      return;
    }
    std::string message;
    switch (mState) {
      case ttt::GameState::kWinX:
        message = "X kazandi! Yeni oyun icin tikla veya R";
        break;
      case ttt::GameState::kWinO:
        message = "O kazandi! Yeni oyun icin tikla veya R";
        break;
      case ttt::GameState::kDraw:
        message = "Berabere. Yeni oyun icin tikla veya R";
        break;
      case ttt::GameState::kPlaying:
        message = mCurrent == ttt::Cell::kX ? "Sira: X" : "Sira: O";
        break;
    }

    painter.SetFont(mFont);
    painter.SetBrush(sp::Brush(kTextColor));
    const sp::Rect box{0.0F, static_cast<float>(Height()) - kStatusHeight,
                       static_cast<float>(Width()), kStatusHeight};
    painter.DrawText(box, message, sp::Alignment::kCenter);
  }

  ttt::Board mBoard{ttt::EmptyBoard()};
  ttt::Cell mCurrent{ttt::Cell::kX};
  ttt::GameState mState{ttt::GameState::kPlaying};
  ttt::WinLine mWinLine{};
  int32_t mHoverCell{-1};

  float mBoardX{0.0F};
  float mBoardY{0.0F};
  float mBoardSize{0.0F};

  std::shared_ptr<sp::Font> mFont;
};

int main() {
  sp::AppConfig config;
  config.title = "SDLPainter — tictactoe: Tic-Tac-Toe (Phase 8)";
  config.width = 700;
  config.height = 780;
  config.resizable = true;  // OnResize ile duyarli yerlesim gosterilir
  config.timing =
      sp::TimingMode::kVariable;  // sira tabanli; sabit adim gereksiz

  TicTacToeApp app(config);
  return app.Run();
}
