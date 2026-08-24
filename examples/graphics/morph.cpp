/// @brief morph — iki poligon arasında yumuşak geçiş; tessellator'ın sınavı.
///
/// Şekil her karede yeniden tessellate edilir ve ara kareler bilerek zor
/// seçilmiştir: yıldızdan haça geçerken poligon **konkav** olur, kendi
/// üzerine yaklaşır, bazı köşeler neredeyse doğrusallaşır. Ear clipping bu
/// durumların hepsinde ayakta kalmalı — kalmazsa ekranda üçgen kaybı veya
/// yanıp sönme olarak görünür.
///
/// `v1.2.0`'da düzeltilen bir hata tam olarak buydu: tekrarlı köşe içeren
/// poligonda triangulation erken duruyordu (bkz. CHANGELOG, K4).
///
/// Gösterilen:
///   - Her karede yeniden tessellation'ın maliyeti (F1 → CPU süresi)
///   - Konkav poligon dolgusu ve aynı poligonun çerçevesi birlikte
///   - Köşe sayısı eşitlenmiş şekil arası enterpolasyon
///
/// Kontroller:
///   SPACE — otomatik geçişi duraklat, elle sürüklemek için ok tuşları
///   ←/→   — geçiş oranını elle değiştir
///   W     — çerçeveyi (wireframe) aç/kapat
///   ESC   — çıkış

#include "sdl_painter/app/application.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sp = sdl_painter;

namespace {

/// @brief Tüm şekiller AYNI köşe sayısına sahip olmalı — enterpolasyon
///        köşe-köşe yapılıyor.
constexpr std::size_t kVertexCount = 24;
constexpr float kPi = 3.14159265358979323846F;

/// @brief n köşeli yıldız; iç ve dış yarıçap dönüşümlü.
std::vector<sp::Point> MakeStar(float radius, float inner_ratio) {
  std::vector<sp::Point> pts;
  pts.reserve(kVertexCount);
  for (std::size_t i = 0; i < kVertexCount; ++i) {
    const float a =
        2.0F * kPi * static_cast<float>(i) / static_cast<float>(kVertexCount);
    const float r = (i % 2 == 0) ? radius : radius * inner_ratio;
    pts.push_back({std::cos(a) * r, std::sin(a) * r});
  }
  return pts;
}

/// @brief Çember üzerine eşit aralıklı noktalar.
std::vector<sp::Point> MakeCircle(float radius) {
  std::vector<sp::Point> pts;
  pts.reserve(kVertexCount);
  for (std::size_t i = 0; i < kVertexCount; ++i) {
    const float a =
        2.0F * kPi * static_cast<float>(i) / static_cast<float>(kVertexCount);
    pts.push_back({std::cos(a) * radius, std::sin(a) * radius});
  }
  return pts;
}

/// @brief Haç (artı) şekli — belirgin biçimde KONKAV.
///
/// Kenarları köşe sayısı kVertexCount olacak şekilde örneklenir; böylece
/// diğer şekillerle birebir enterpole edilebilir.
std::vector<sp::Point> MakeCross(float size) {
  // Haçın 12 köşeli ana hattı.
  const float a = size;
  const float b = size * 0.36F;
  const std::vector<sp::Point> outline = {
      {-b, -a}, {b, -a}, {b, -b}, {a, -b}, {a, b},   {b, b},
      {b, a},   {-b, a}, {-b, b}, {-a, b}, {-a, -b}, {-b, -b},
  };

  // Ana hattı, çevre boyunca eşit aralıklı kVertexCount noktaya yeniden
  // örnekle. Şekiller arası enterpolasyonun ön koşulu köşe sayısının eşit
  // olmasıdır.
  float perimeter = 0.0F;
  std::vector<float> seg_len(outline.size(), 0.0F);
  for (std::size_t i = 0; i < outline.size(); ++i) {
    const sp::Point& p0 = outline[i];
    const sp::Point& p1 = outline[(i + 1) % outline.size()];
    seg_len[i] = std::sqrt((p1.x - p0.x) * (p1.x - p0.x) +
                           (p1.y - p0.y) * (p1.y - p0.y));
    perimeter += seg_len[i];
  }

  std::vector<sp::Point> pts;
  pts.reserve(kVertexCount);
  const float step = perimeter / static_cast<float>(kVertexCount);
  float target = 0.0F;
  float walked = 0.0F;
  std::size_t seg = 0;
  for (std::size_t i = 0; i < kVertexCount; ++i) {
    while (seg + 1 < outline.size() && walked + seg_len[seg] < target) {
      walked += seg_len[seg];
      ++seg;
    }
    const sp::Point& p0 = outline[seg];
    const sp::Point& p1 = outline[(seg + 1) % outline.size()];
    const float t =
        seg_len[seg] > 0.0F ? (target - walked) / seg_len[seg] : 0.0F;
    pts.push_back({p0.x + (p1.x - p0.x) * t, p0.y + (p1.y - p0.y) * t});
    target += step;
  }
  return pts;
}

/// @brief Yumuşak geçiş eğrisi (smoothstep) — doğrusal geçiş mekanik durur.
float SmoothStep(float t) {
  return t * t * (3.0F - 2.0F * t);
}

}  // namespace

class MorphDemo : public sp::Application {
 public:
  using Application::Application;

 protected:
  bool OnInit() override {
    mShapes.push_back(MakeCircle(150.0F));
    mShapes.push_back(MakeStar(160.0F, 0.45F));
    mShapes.push_back(MakeCross(150.0F));
    mShapes.push_back(MakeStar(160.0F, 0.18F));  // çok sivri — en zor hâli
    return true;
  }

  void OnUpdate(float dt) override {
    if (mPaused) {
      return;
    }
    mProgress += dt * 0.45F;
    while (mProgress >= 1.0F) {
      mProgress -= 1.0F;
      mFrom = (mFrom + 1) % mShapes.size();
    }
  }

  void OnRender(sp::Painter& painter) override {
    painter.Clear(sp::Color{18, 20, 28, 255});

    const std::vector<sp::Point> shape = Interpolate();

    painter.Save();
    painter.Translate(static_cast<float>(Width()) * 0.5F,
                      static_cast<float>(Height()) * 0.5F);

    painter.SetBrush(sp::Brush(sp::Color{90, 160, 255, 190}));
    painter.FillPolygon(shape);

    if (mWireframe) {
      sp::Pen pen(sp::Color{230, 240, 255, 230}, 2.0F);
      pen.SetJoinStyle(sp::LineJoin::kRound);
      painter.SetPen(pen);
      painter.DrawPolygon(shape);

      // Köşeleri işaretle: enterpolasyonun köşe-köşe olduğu görünsün.
      painter.SetPen(sp::Pen::NoPen());
      painter.SetBrush(sp::Brush(sp::Color{255, 200, 80, 230}));
      for (const auto& p : shape) {
        painter.FillCircle(p.x, p.y, 3.0F);
      }
    }
    painter.Restore();
  }

  void OnKeyDown(const sp::KeyEvent& event) override {
    if (event.repeat && event.key != sp::Key::kLeft &&
        event.key != sp::Key::kRight) {
      return;
    }
    switch (event.key) {
      case sp::Key::kSpace:
        mPaused = !mPaused;
        break;
      case sp::Key::kW:
        mWireframe = !mWireframe;
        break;
      case sp::Key::kLeft:
        mProgress = std::fmax(0.0F, mProgress - 0.05F);
        break;
      case sp::Key::kRight:
        mProgress = std::fmin(1.0F, mProgress + 0.05F);
        break;
      case sp::Key::kEscape:
        Quit();
        break;
      default:
        break;
    }
  }

 private:
  [[nodiscard]] std::vector<sp::Point> Interpolate() const {
    const std::vector<sp::Point>& a = mShapes[mFrom];
    const std::vector<sp::Point>& b = mShapes[(mFrom + 1) % mShapes.size()];
    const float t = SmoothStep(mProgress);

    std::vector<sp::Point> out;
    out.reserve(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
      out.push_back(
          {a[i].x + (b[i].x - a[i].x) * t, a[i].y + (b[i].y - a[i].y) * t});
    }
    return out;
  }

  std::vector<std::vector<sp::Point>> mShapes;
  std::size_t mFrom{0};
  float mProgress{0.0F};
  bool mPaused{false};
  bool mWireframe{true};
};

int main() {
  sp::AppConfig config;
  config.title = "SDLPainter — morph: her karede yeniden tessellation";
  config.width = 800;
  config.height = 700;
  config.stats_overlay = sp::StatsOverlayMode::kDetailed;

  MorphDemo app(config);
  return app.Run();
}
