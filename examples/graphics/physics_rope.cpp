/// @brief physics_rope — Verlet zinciri ve kumaş; kalın çizginin dürüst sınavı.
///
/// Fizik tarafı kasıtlı olarak basit (yaklaşık 40 satır Verlet entegrasyonu);
/// örneğin asıl konusu çizim. Sürekli şekil değiştiren, keskin açılar
/// üreten bir polyline, kalın çizgi geometrisinin en zorlandığı durumdur:
///
///   * Birleşim (join) stili yanlışsa köşelerde boşluk ya da sivri çıkıntı
///     görünür — J tuşuyla üç stili canlı karşılaştır.
///   * Uç (cap) stili yoksa ipin uçları düz kesik kalır.
///
/// Halat serbestçe savrulurken bu kusurlar durağan bir ekran görüntüsünde
/// gözden kaçar; hareket hâlinde kaçmaz. Örneğin var olma sebebi budur.
///
/// Kontroller:
///   Fare      — en yakın düğümü yakala ve sürükle
///   J         — birleşim stili: round / miter / bevel
///   C         — uç stili: butt / square / round
///   T         — kumaş (cloth) / halat (rope) arasında geçiş
///   R         — sıfırla
///   ESC       — çıkış

#include "sdl_painter/app/application.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sp = sdl_painter;

namespace {

constexpr float kGravity = 900.0F;
constexpr int32_t kSolverIterations = 8;  ///< Kısıt çözücü sıkılığı.
constexpr std::size_t kRopeNodes = 28;
constexpr int32_t kClothCols = 16;
constexpr int32_t kClothRows = 11;

/// @brief Verlet noktası — hız yerine önceki konum saklanır.
struct Node {
  float x{0.0F};
  float y{0.0F};
  float prev_x{0.0F};
  float prev_y{0.0F};
  bool pinned{false};
};

/// @brief İki düğüm arasındaki sabit uzunluk kısıtı.
struct Link {
  std::size_t a{0};
  std::size_t b{0};
  float rest{0.0F};
};

}  // namespace

class PhysicsRopeDemo : public sp::Application {
 public:
  using Application::Application;

 protected:
  bool OnInit() override {
    Build();
    return true;
  }

  void OnUpdate(float dt) override {
    // Sabit adım: değişken dt Verlet'i kararsız yapar.
    const float step = 1.0F / 120.0F;
    mAccumulator += dt;
    int32_t guard = 0;
    while (mAccumulator >= step && guard < 8) {
      Integrate(step);
      for (int32_t i = 0; i < kSolverIterations; ++i) {
        SolveLinks();
      }
      mAccumulator -= step;
      ++guard;
    }
    if (guard == 8) {
      mAccumulator = 0.0F;  // Geride kalındıysa biriktirme.
    }
  }

  void OnRender(sp::Painter& painter) override {
    painter.Clear(sp::Color{16, 18, 26, 255});

    sp::Pen pen(sp::Color{240, 190, 100, 235}, mIsCloth ? 3.0F : 9.0F);
    pen.SetJoinStyle(mJoin);
    pen.SetCapStyle(mCap);
    painter.SetPen(pen);

    if (mIsCloth) {
      // Kumaş: her satır ve her sütun ayrı bir polyline.
      for (int32_t r = 0; r < kClothRows; ++r) {
        std::vector<sp::Point> row;
        row.reserve(static_cast<std::size_t>(kClothCols));
        for (int32_t c = 0; c < kClothCols; ++c) {
          const Node& n = mNodes[Index(r, c)];
          row.push_back({n.x, n.y});
        }
        painter.DrawPolyline(row);
      }
      for (int32_t c = 0; c < kClothCols; ++c) {
        std::vector<sp::Point> col;
        col.reserve(static_cast<std::size_t>(kClothRows));
        for (int32_t r = 0; r < kClothRows; ++r) {
          const Node& n = mNodes[Index(r, c)];
          col.push_back({n.x, n.y});
        }
        painter.DrawPolyline(col);
      }
    } else {
      // Halat: tek bir kalın polyline. Köşe stilinin etkisi burada en
      // belirgin — kalınlık 9 piksel.
      std::vector<sp::Point> pts;
      pts.reserve(mNodes.size());
      for (const auto& n : mNodes) {
        pts.push_back({n.x, n.y});
      }
      painter.DrawPolyline(pts);
    }

    // Sabitlenmiş düğümler ve yakalanan düğüm.
    painter.SetPen(sp::Pen::NoPen());
    for (std::size_t i = 0; i < mNodes.size(); ++i) {
      const Node& n = mNodes[i];
      if (n.pinned) {
        painter.SetBrush(sp::Brush(sp::Color{255, 90, 90, 240}));
        painter.FillCircle(n.x, n.y, 5.0F);
      }
    }
    if (mGrabbed < mNodes.size()) {
      painter.SetBrush(sp::Brush(sp::Color{120, 255, 160, 240}));
      painter.FillCircle(mNodes[mGrabbed].x, mNodes[mGrabbed].y, 7.0F);
    }
  }

  void OnMouseButtonDown(const sp::MouseButtonEvent& event) override {
    mGrabbed = NearestNode(event.x, event.y);
  }

  void OnMouseButtonUp(const sp::MouseButtonEvent&) override {
    mGrabbed = mNodes.size();
  }

  void OnMouseMove(const sp::MouseMoveEvent& event) override {
    if (mGrabbed < mNodes.size()) {
      Node& n = mNodes[mGrabbed];
      n.x = event.x;
      n.y = event.y;
      n.prev_x = event.x;  // Hız sıfırlanır: sürükleme fırlatmaya dönüşmesin.
      n.prev_y = event.y;
    }
  }

  void OnKeyDown(const sp::KeyEvent& event) override {
    if (event.repeat) {
      return;
    }
    switch (event.key) {
      case sp::Key::kJ:
        mJoin = NextJoin(mJoin);
        UpdateTitle();
        break;
      case sp::Key::kC:
        mCap = NextCap(mCap);
        UpdateTitle();
        break;
      case sp::Key::kT:
        mIsCloth = !mIsCloth;
        Build();
        break;
      case sp::Key::kR:
        Build();
        break;
      case sp::Key::kEscape:
        Quit();
        break;
      default:
        break;
    }
  }

 private:
  [[nodiscard]] std::size_t Index(int32_t r, int32_t c) const {
    return static_cast<std::size_t>(r) * kClothCols +
           static_cast<std::size_t>(c);
  }

  static sp::LineJoin NextJoin(sp::LineJoin j) {
    switch (j) {
      case sp::LineJoin::kRound:
        return sp::LineJoin::kMiter;
      case sp::LineJoin::kMiter:
        return sp::LineJoin::kBevel;
      default:
        return sp::LineJoin::kRound;
    }
  }

  static sp::LineCap NextCap(sp::LineCap c) {
    switch (c) {
      case sp::LineCap::kButt:
        return sp::LineCap::kSquare;
      case sp::LineCap::kSquare:
        return sp::LineCap::kRound;
      default:
        return sp::LineCap::kButt;
    }
  }

  void UpdateTitle() {
    const char* join = mJoin == sp::LineJoin::kRound   ? "round"
                       : mJoin == sp::LineJoin::kMiter ? "miter"
                                                       : "bevel";
    const char* cap = mCap == sp::LineCap::kButt     ? "butt"
                      : mCap == sp::LineCap::kSquare ? "square"
                                                     : "round";
    SetTitle(std::string("SDLPainter — physics_rope · join=") + join +
             " cap=" + cap);
  }

  void Build() {
    mNodes.clear();
    mLinks.clear();
    mGrabbed = 0;

    const auto w = static_cast<float>(Width());

    if (mIsCloth) {
      const float spacing = 34.0F;
      const float x0 = w * 0.5F - spacing * (kClothCols - 1) * 0.5F;
      const float y0 = 70.0F;
      for (int32_t r = 0; r < kClothRows; ++r) {
        for (int32_t c = 0; c < kClothCols; ++c) {
          Node n;
          n.x = x0 + static_cast<float>(c) * spacing;
          n.y = y0 + static_cast<float>(r) * spacing;
          n.prev_x = n.x;
          n.prev_y = n.y;
          // Üst satırın iki ucu ve ortası askıda.
          n.pinned = (r == 0) &&
                     (c == 0 || c == kClothCols - 1 || c == kClothCols / 2);
          mNodes.push_back(n);
        }
      }
      for (int32_t r = 0; r < kClothRows; ++r) {
        for (int32_t c = 0; c < kClothCols; ++c) {
          if (c + 1 < kClothCols) {
            AddLink(Index(r, c), Index(r, c + 1));
          }
          if (r + 1 < kClothRows) {
            AddLink(Index(r, c), Index(r + 1, c));
          }
        }
      }
    } else {
      const float spacing = 22.0F;
      for (std::size_t i = 0; i < kRopeNodes; ++i) {
        Node n;
        n.x = w * 0.5F - kRopeNodes * spacing * 0.5F +
              static_cast<float>(i) * spacing;
        n.y = 90.0F;
        n.prev_x = n.x;
        n.prev_y = n.y;
        n.pinned = (i == 0) || (i == kRopeNodes - 1);
        mNodes.push_back(n);
      }
      for (std::size_t i = 0; i + 1 < mNodes.size(); ++i) {
        AddLink(i, i + 1);
      }
    }

    mGrabbed = mNodes.size();  // "yakalanan yok"
    UpdateTitle();
  }

  void AddLink(std::size_t a, std::size_t b) {
    const float dx = mNodes[b].x - mNodes[a].x;
    const float dy = mNodes[b].y - mNodes[a].y;
    mLinks.push_back({a, b, std::sqrt(dx * dx + dy * dy)});
  }

  void Integrate(float dt) {
    const float damping = 0.992F;
    for (auto& n : mNodes) {
      if (n.pinned) {
        continue;
      }
      const float vx = (n.x - n.prev_x) * damping;
      const float vy = (n.y - n.prev_y) * damping;
      n.prev_x = n.x;
      n.prev_y = n.y;
      n.x += vx;
      n.y += vy + kGravity * dt * dt;
    }
  }

  void SolveLinks() {
    for (const auto& l : mLinks) {
      Node& a = mNodes[l.a];
      Node& b = mNodes[l.b];
      const float dx = b.x - a.x;
      const float dy = b.y - a.y;
      const float dist = std::sqrt(dx * dx + dy * dy);
      if (dist < 1e-5F) {
        continue;
      }
      const float diff = (dist - l.rest) / dist * 0.5F;
      const float ox = dx * diff;
      const float oy = dy * diff;
      if (!a.pinned) {
        a.x += ox;
        a.y += oy;
      }
      if (!b.pinned) {
        b.x -= ox;
        b.y -= oy;
      }
    }
  }

  [[nodiscard]] std::size_t NearestNode(float x, float y) const {
    std::size_t best = mNodes.size();
    float best_dist = 40.0F * 40.0F;  // yakalama yarıçapı
    for (std::size_t i = 0; i < mNodes.size(); ++i) {
      const float dx = mNodes[i].x - x;
      const float dy = mNodes[i].y - y;
      const float d = dx * dx + dy * dy;
      if (d < best_dist) {
        best_dist = d;
        best = i;
      }
    }
    return best;
  }

  std::vector<Node> mNodes;
  std::vector<Link> mLinks;
  std::size_t mGrabbed{0};
  float mAccumulator{0.0F};
  sp::LineJoin mJoin{sp::LineJoin::kRound};
  sp::LineCap mCap{sp::LineCap::kRound};
  bool mIsCloth{false};
};

int main() {
  sp::AppConfig config;
  config.title = "SDLPainter — physics_rope";
  config.width = 1000;
  config.height = 700;
  config.stats_overlay = sp::StatsOverlayMode::kDetailed;

  PhysicsRopeDemo app(config);
  return app.Run();
}
