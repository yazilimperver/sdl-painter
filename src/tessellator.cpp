#include "tessellator.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <spdlog/spdlog.h>

namespace sdl_painter {

namespace {
constexpr float kTwoPi = 2.0F * 3.14159265358979323846F;
}  // namespace

// --- Dolu şekiller ---

std::vector<Vertex> Tessellator::TessellateFilledRect(float x, float y, float w,
                                                      float h) {
  // İki üçgen, 6 vertex
  // clang-format off
  return {
      {x,     y    },
      {x + w, y    },
      {x + w, y + h},
      {x,     y    },
      {x + w, y + h},
      {x,     y + h},
  };
  // clang-format on
}

std::vector<Vertex> Tessellator::TessellateFilledCircle(float cx, float cy,
                                                        float radius) {
  int32_t segments = AdaptiveSegments(radius);
  std::vector<Vertex> result;
  result.reserve(static_cast<std::size_t>(segments) * 3);

  for (int32_t i = 0; i < segments; ++i) {
    float a0 = kTwoPi * static_cast<float>(i) / static_cast<float>(segments);
    float a1 =
        kTwoPi * static_cast<float>(i + 1) / static_cast<float>(segments);
    result.emplace_back(cx, cy);
    result.emplace_back(cx + std::cos(a0) * radius, cy + std::sin(a0) * radius);
    result.emplace_back(cx + std::cos(a1) * radius, cy + std::sin(a1) * radius);
  }
  return result;
}

std::vector<Vertex> Tessellator::TessellateFilledEllipse(float cx, float cy,
                                                         float rx, float ry) {
  int32_t segments = AdaptiveSegments(std::max(rx, ry));
  std::vector<Vertex> result;
  result.reserve(static_cast<std::size_t>(segments) * 3);

  for (int32_t i = 0; i < segments; ++i) {
    float a0 = kTwoPi * static_cast<float>(i) / static_cast<float>(segments);
    float a1 =
        kTwoPi * static_cast<float>(i + 1) / static_cast<float>(segments);
    result.emplace_back(cx, cy);
    result.emplace_back(cx + std::cos(a0) * rx, cy + std::sin(a0) * ry);
    result.emplace_back(cx + std::cos(a1) * rx, cy + std::sin(a1) * ry);
  }
  return result;
}

std::vector<Vertex> Tessellator::TessellateFilledPolygon(
    const std::vector<Point>& points) {
  if (points.size() < 3) {
    return {};
  }
  return EarClipping(points);
}

// --- Çerçeve şekiller ---

std::vector<Vertex> Tessellator::TessellateStrokedRect(float x, float y,
                                                       float w, float h,
                                                       float line_width) {
  return TessellateStrokedPolygon(
      {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}}, line_width);
}

std::vector<Vertex> Tessellator::TessellateStrokedCircle(float cx, float cy,
                                                         float radius,
                                                         float line_width) {
  int32_t segments = AdaptiveSegments(radius);
  std::vector<Point> pts;
  pts.reserve(static_cast<std::size_t>(segments));

  for (int32_t i = 0; i < segments; ++i) {
    float a = kTwoPi * static_cast<float>(i) / static_cast<float>(segments);
    pts.emplace_back(cx + std::cos(a) * radius, cy + std::sin(a) * radius);
  }
  return TessellateStrokedPolygon(pts, line_width);
}

std::vector<Vertex> Tessellator::TessellateStrokedEllipse(float cx, float cy,
                                                          float rx, float ry,
                                                          float line_width) {
  int32_t segments = AdaptiveSegments(std::max(rx, ry));
  std::vector<Point> pts;
  pts.reserve(static_cast<std::size_t>(segments));

  for (int32_t i = 0; i < segments; ++i) {
    float a = kTwoPi * static_cast<float>(i) / static_cast<float>(segments);
    pts.emplace_back(cx + std::cos(a) * rx, cy + std::sin(a) * ry);
  }
  return TessellateStrokedPolygon(pts, line_width);
}

std::vector<Vertex> Tessellator::TessellateThickLine(float x1, float y1,
                                                     float x2, float y2,
                                                     float line_width) {
  float dx = x2 - x1;
  float dy = y2 - y1;
  float len = std::sqrt(dx * dx + dy * dy);
  if (len < 1e-6F) {
    return {};
  }

  // Dike normal vektör (normalize edilmiş)
  float nx = -dy / len;
  float ny = dx / len;
  float hw = line_width * 0.5F;

  // Quad'ın 4 köşesi
  // clang-format off
  float p0x = x1 + hw * nx;  float p0y = y1 + hw * ny;  // A üst
  float p1x = x1 - hw * nx;  float p1y = y1 - hw * ny;  // A alt
  float p2x = x2 + hw * nx;  float p2y = y2 + hw * ny;  // B üst
  float p3x = x2 - hw * nx;  float p3y = y2 - hw * ny;  // B alt

  return {
      {p0x, p0y}, {p1x, p1y}, {p2x, p2y},
      {p1x, p1y}, {p3x, p3y}, {p2x, p2y},
  };
  // clang-format on
}

void Tessellator::AppendRoundJoin(std::vector<Vertex>& out, const Point& center,
                                  float radius) {
  // Yuvarlak birleşim (round join): köşeye kalınlığın yarısı yarıçapında bir
  // disk yerleştirilir. Miter'a göre avantajı, keskin açılarda sivri uç
  // (spike) üretmemesi ve miter limit ayarı gerektirmemesi.
  //
  // Segment sayısı yarıçapla ölçeklenir; ince çizgilerde 6 segment yeterli.
  constexpr int32_t kMinJoinSegments = 6;
  constexpr int32_t kMaxJoinSegments = 24;
  const int32_t kSegments = std::clamp(static_cast<int32_t>(radius * 2.0F),
                                       kMinJoinSegments, kMaxJoinSegments);

  for (int32_t i = 0; i < kSegments; ++i) {
    const float a0 =
        kTwoPi * static_cast<float>(i) / static_cast<float>(kSegments);
    const float a1 =
        kTwoPi * static_cast<float>(i + 1) / static_cast<float>(kSegments);
    out.emplace_back(center.x, center.y);
    out.emplace_back(center.x + std::cos(a0) * radius,
                     center.y + std::sin(a0) * radius);
    out.emplace_back(center.x + std::cos(a1) * radius,
                     center.y + std::sin(a1) * radius);
  }
}

std::vector<Vertex> Tessellator::TessellateThickPolyline(
    const std::vector<Point>& points, float line_width) {
  return TessellatePolyline(points, line_width, /*closed=*/false);
}

std::vector<Vertex> Tessellator::TessellateStrokedPolygon(
    const std::vector<Point>& points, float line_width) {
  return TessellatePolyline(points, line_width, /*closed=*/true);
}

std::vector<Vertex> Tessellator::TessellatePolyline(
    const std::vector<Point>& raw, float line_width, bool closed) {
  // Çakışan ardışık noktalar sıfır uzunluklu segment üretir; hem quad hem de
  // birleşim hesabını bozar.
  const std::vector<Point> points = RemoveDuplicatePoints(raw);
  if (points.size() < 2) {
    return {};
  }
  if (!(line_width > 0.0F)) {
    return {};
  }

  std::vector<Vertex> result;
  const std::size_t kSegmentCount = closed ? points.size() : points.size() - 1;
  result.reserve(kSegmentCount * 6);

  for (std::size_t i = 0; i < kSegmentCount; ++i) {
    const Point& a = points[i];
    const Point& b = points[(i + 1) % points.size()];
    auto seg = TessellateThickLine(a.x, a.y, b.x, b.y, line_width);
    result.insert(result.end(), seg.begin(), seg.end());
  }

  // Birleşimler: segmentler bağımsız quad'lar olduğundan köşelerde kama
  // biçiminde boşluk kalır. Kalınlık 1 pikselin altındayken boşluk görünmez,
  // disk eklemek yalnızca vertex israfı olur.
  constexpr float kMinJoinWidth = 1.5F;
  if (line_width >= kMinJoinWidth) {
    const float kRadius = line_width * 0.5F;
    // Açık polyline'da yalnızca iç köşeler; kapalı poligonda tüm köşeler
    // (ilk köşe de son segmentin bitişidir).
    const std::size_t kFirst = closed ? 0 : 1;
    const std::size_t kLast = closed ? points.size() : points.size() - 1;
    for (std::size_t i = kFirst; i < kLast; ++i) {
      AppendRoundJoin(result, points[i], kRadius);
    }
  }

  return result;
}

std::vector<TexturedVertex> Tessellator::TessellateTexturedRect(
    float x, float y, float w, float h, float u0, float v0, float u1,
    float v1) {
  // clang-format off
  return {
      {x,     y,     u0, v0},
      {x + w, y,     u1, v0},
      {x,     y + h, u0, v1},
      {x,     y + h, u0, v1},
      {x + w, y,     u1, v0},
      {x + w, y + h, u1, v1},
  };
  // clang-format on
}

// --- Yardımcı iç fonksiyonlar ---

bool Tessellator::IsClockwise(const Point& a, const Point& b, const Point& c) {
  float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
  return cross < 0.0F;
}

bool Tessellator::PointInTriangle(const Point& p, const Point& a,
                                  const Point& b, const Point& c) {
  auto sign = [](const Point& p1, const Point& p2, const Point& p3) {
    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
  };
  // Kesin (strict) iç test: kenar üzerindeki noktalar "dışarıda" sayılır.
  // Kulak testinde sınır noktalarını içeride saymak, geçerli kulakları
  // reddedip triangulation'ı erken durduruyordu (bkz. K4).
  const float d1 = sign(p, a, b);
  const float d2 = sign(p, b, c);
  const float d3 = sign(p, c, a);
  constexpr float kEps = 1e-6F;
  const bool has_neg = (d1 < -kEps) || (d2 < -kEps) || (d3 < -kEps);
  const bool has_pos = (d1 > kEps) || (d2 > kEps) || (d3 > kEps);
  return !(has_neg && has_pos) &&
         !(std::fabs(d1) <= kEps || std::fabs(d2) <= kEps ||
           std::fabs(d3) <= kEps);
}

std::vector<Point> Tessellator::RemoveDuplicatePoints(
    const std::vector<Point>& points) {
  auto same = [](const Point& a, const Point& b) {
    constexpr float kEpsSq = 1e-12F;
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return (dx * dx + dy * dy) <= kEpsSq;
  };

  std::vector<Point> result;
  result.reserve(points.size());
  for (const Point& p : points) {
    if (result.empty() || !same(result.back(), p)) {
      result.push_back(p);
    }
  }
  // Kapalı poligonda son nokta ilkiyle çakışıyorsa o da tekrardır.
  while (result.size() > 1 && same(result.front(), result.back())) {
    result.pop_back();
  }
  return result;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
std::vector<Vertex> Tessellator::EarClipping(const std::vector<Point>& raw) {
  // Tekrarlı (çakışan) köşeleri ele: sıfır uzunluklu kenarlar kulak testini
  // bozup triangulation'ın sessizce yarıda kesilmesine yol açıyordu (K4).
  const std::vector<Point> points = RemoveDuplicatePoints(raw);

  if (points.size() < 3) {
    return {};
  }

  // Basit üçgen durumu
  if (points.size() == 3) {
    return {{points[0].x, points[0].y},
            {points[1].x, points[1].y},
            {points[2].x, points[2].y}};
  }

  // İmzalı alan ile sarma yönünü belirle (shoelace formülü)
  float area = 0.0F;
  for (std::size_t i = 0; i < points.size(); ++i) {
    std::size_t j = (i + 1) % points.size();
    area += points[i].x * points[j].y - points[j].x * points[i].y;
  }
  // area > 0 → CCW; area < 0 → CW

  // CW ise indeks sırasını ters çevir → her zaman CCW olarak işle
  std::vector<int32_t> indices(points.size());
  std::iota(indices.begin(), indices.end(), 0);
  if (area < 0.0F) {
    std::reverse(indices.begin(), indices.end());
  }

  std::vector<Vertex> result;
  result.reserve((points.size() - 2) * 3);

  // O(n²) ear clipping
  while (indices.size() > 3) {
    bool found_ear = false;
    std::size_t n = indices.size();

    for (std::size_t i = 0; i < n; ++i) {
      std::size_t iprev = (i + n - 1) % n;
      std::size_t inext = (i + 1) % n;

      const Point& a = points[static_cast<std::size_t>(indices[iprev])];
      const Point& b = points[static_cast<std::size_t>(indices[i])];
      const Point& c = points[static_cast<std::size_t>(indices[inext])];

      // CCW poligonda dışbükey köşe: cross(a→b, b→c) > 0
      float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
      if (cross <= 0.0F) {
        continue;  // içbükey (reflex) köşe, kulak değil
      }

      // Diğer hiçbir nokta bu üçgenin içinde olmamalı
      bool is_ear = true;
      for (std::size_t j = 0; j < n; ++j) {
        if (j == iprev || j == i || j == inext) {
          continue;
        }
        if (PointInTriangle(points[static_cast<std::size_t>(indices[j])], a, b,
                            c)) {
          is_ear = false;
          break;
        }
      }

      if (is_ear) {
        result.emplace_back(a.x, a.y);
        result.emplace_back(b.x, b.y);
        result.emplace_back(c.x, c.y);
        indices.erase(indices.begin() + static_cast<std::ptrdiff_t>(i));
        found_ear = true;
        break;
      }
    }

    // Dejenere poligon koruması. Buraya düşmek, girdinin basit bir poligon
    // olmadığı (kendini kesen kenarlar, sıfır alanlı bölgeler) anlamına
    // gelir. Sessizce yarıda kesmek yerine kullanıcıyı uyar: aksi halde
    // şeklin büyük kısmı hiçbir iz bırakmadan kaybolur.
    if (!found_ear) {
      spdlog::warn(
          "Tessellator: poligon tam üçgenlenemedi — {} köşe atlandı "
          "(kendini kesen veya dejenere poligon?).",
          indices.size() - 3);
      break;
    }
  }

  // Son üçgeni ekle
  if (indices.size() == 3) {
    result.emplace_back(points[static_cast<std::size_t>(indices[0])].x,
                        points[static_cast<std::size_t>(indices[0])].y);
    result.emplace_back(points[static_cast<std::size_t>(indices[1])].x,
                        points[static_cast<std::size_t>(indices[1])].y);
    result.emplace_back(points[static_cast<std::size_t>(indices[2])].x,
                        points[static_cast<std::size_t>(indices[2])].y);
  }

  return result;
}

}  // namespace sdl_painter
