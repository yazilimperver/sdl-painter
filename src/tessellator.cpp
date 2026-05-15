#include "tessellator.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace sdl_painter {

namespace {
constexpr float kTwoPi = 2.0f * 3.14159265358979323846f;
}  // namespace

// --- Dolu şekiller ---

std::vector<Vertex> Tessellator::TessellateFilledRect(
    float x, float y, float w, float h) {
  // İki üçgen, 6 vertex
  return {
      {x,     y    },
      {x + w, y    },
      {x + w, y + h},
      {x,     y    },
      {x + w, y + h},
      {x,     y + h},
  };
}

std::vector<Vertex> Tessellator::TessellateFilledCircle(
    float cx, float cy, float radius) {
  int32_t segments = AdaptiveSegments(radius);
  std::vector<Vertex> result;
  result.reserve(static_cast<std::size_t>(segments) * 3);

  for (int32_t i = 0; i < segments; ++i) {
    float a0 = kTwoPi * static_cast<float>(i)     / static_cast<float>(segments);
    float a1 = kTwoPi * static_cast<float>(i + 1) / static_cast<float>(segments);
    result.push_back({cx, cy});
    result.push_back({cx + std::cos(a0) * radius, cy + std::sin(a0) * radius});
    result.push_back({cx + std::cos(a1) * radius, cy + std::sin(a1) * radius});
  }
  return result;
}

std::vector<Vertex> Tessellator::TessellateFilledEllipse(
    float cx, float cy, float rx, float ry) {
  int32_t segments = AdaptiveSegments(std::max(rx, ry));
  std::vector<Vertex> result;
  result.reserve(static_cast<std::size_t>(segments) * 3);

  for (int32_t i = 0; i < segments; ++i) {
    float a0 = kTwoPi * static_cast<float>(i)     / static_cast<float>(segments);
    float a1 = kTwoPi * static_cast<float>(i + 1) / static_cast<float>(segments);
    result.push_back({cx, cy});
    result.push_back({cx + std::cos(a0) * rx, cy + std::sin(a0) * ry});
    result.push_back({cx + std::cos(a1) * rx, cy + std::sin(a1) * ry});
  }
  return result;
}

std::vector<Vertex> Tessellator::TessellateFilledPolygon(
    const std::vector<Point>& points) {
  if (points.size() < 3) return {};
  return EarClipping(points);
}

// --- Çerçeve şekiller ---

std::vector<Vertex> Tessellator::TessellateStrokedRect(
    float x, float y, float w, float h, float line_width) {
  return TessellateStrokedPolygon(
      {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}}, line_width);
}

std::vector<Vertex> Tessellator::TessellateStrokedCircle(
    float cx, float cy, float radius, float line_width) {
  int32_t segments = AdaptiveSegments(radius);
  std::vector<Point> pts;
  pts.reserve(static_cast<std::size_t>(segments));

  for (int32_t i = 0; i < segments; ++i) {
    float a = kTwoPi * static_cast<float>(i) / static_cast<float>(segments);
    pts.push_back({cx + std::cos(a) * radius, cy + std::sin(a) * radius});
  }
  return TessellateStrokedPolygon(pts, line_width);
}

std::vector<Vertex> Tessellator::TessellateStrokedEllipse(
    float cx, float cy, float rx, float ry, float line_width) {
  int32_t segments = AdaptiveSegments(std::max(rx, ry));
  std::vector<Point> pts;
  pts.reserve(static_cast<std::size_t>(segments));

  for (int32_t i = 0; i < segments; ++i) {
    float a = kTwoPi * static_cast<float>(i) / static_cast<float>(segments);
    pts.push_back({cx + std::cos(a) * rx, cy + std::sin(a) * ry});
  }
  return TessellateStrokedPolygon(pts, line_width);
}

std::vector<Vertex> Tessellator::TessellateThickLine(
    float x1, float y1, float x2, float y2, float line_width) {
  float dx = x2 - x1;
  float dy = y2 - y1;
  float len = std::sqrt(dx * dx + dy * dy);
  if (len < 1e-6f) return {};

  // Dike normal vektör (normalize edilmiş)
  float nx = -dy / len;
  float ny =  dx / len;
  float hw = line_width * 0.5f;

  // Quad'ın 4 köşesi
  float p0x = x1 + hw * nx,  p0y = y1 + hw * ny;  // A üst
  float p1x = x1 - hw * nx,  p1y = y1 - hw * ny;  // A alt
  float p2x = x2 + hw * nx,  p2y = y2 + hw * ny;  // B üst
  float p3x = x2 - hw * nx,  p3y = y2 - hw * ny;  // B alt

  return {
      {p0x, p0y}, {p1x, p1y}, {p2x, p2y},
      {p1x, p1y}, {p3x, p3y}, {p2x, p2y},
  };
}

std::vector<Vertex> Tessellator::TessellateThickPolyline(
    const std::vector<Point>& points, float line_width) {
  std::vector<Vertex> result;
  if (points.size() < 2) return result;

  for (std::size_t i = 0; i + 1 < points.size(); ++i) {
    auto seg = TessellateThickLine(points[i].x, points[i].y,
                                   points[i + 1].x, points[i + 1].y,
                                   line_width);
    result.insert(result.end(), seg.begin(), seg.end());
  }
  return result;
}

std::vector<Vertex> Tessellator::TessellateStrokedPolygon(
    const std::vector<Point>& points, float line_width) {
  if (points.size() < 2) return {};

  // Kapalı polyline — son noktayı başa bağla
  std::vector<Point> closed = points;
  closed.push_back(points[0]);
  return TessellateThickPolyline(closed, line_width);
}

std::vector<TexturedVertex> Tessellator::TessellateTexturedRect(
    float x, float y, float w, float h,
    float u0, float v0, float u1, float v1) {
  return {
      {x,     y,     u0, v0},
      {x + w, y,     u1, v0},
      {x,     y + h, u0, v1},
      {x,     y + h, u0, v1},
      {x + w, y,     u1, v0},
      {x + w, y + h, u1, v1},
  };
}

// --- Yardımcı iç fonksiyonlar ---

bool Tessellator::IsClockwise(const Point& a, const Point& b, const Point& c) {
  float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
  return cross < 0.0f;
}

bool Tessellator::PointInTriangle(const Point& p,
                                   const Point& a, const Point& b,
                                   const Point& c) {
  auto Sign = [](const Point& p1, const Point& p2, const Point& p3) {
    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
  };
  float d1 = Sign(p, a, b);
  float d2 = Sign(p, b, c);
  float d3 = Sign(p, c, a);
  bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
  bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
  return !(has_neg && has_pos);
}

std::vector<Vertex> Tessellator::EarClipping(const std::vector<Point>& pts) {
  if (pts.size() < 3) return {};

  // Basit üçgen durumu
  if (pts.size() == 3) {
    return {{pts[0].x, pts[0].y}, {pts[1].x, pts[1].y}, {pts[2].x, pts[2].y}};
  }

  // İmzalı alan ile sarma yönünü belirle (shoelace formülü)
  float area = 0.0f;
  for (std::size_t i = 0; i < pts.size(); ++i) {
    std::size_t j = (i + 1) % pts.size();
    area += pts[i].x * pts[j].y - pts[j].x * pts[i].y;
  }
  // area > 0 → CCW; area < 0 → CW

  // CW ise indeks sırasını ters çevir → her zaman CCW olarak işle
  std::vector<int32_t> indices(pts.size());
  std::iota(indices.begin(), indices.end(), 0);
  if (area < 0.0f) {
    std::reverse(indices.begin(), indices.end());
  }

  std::vector<Vertex> result;
  result.reserve((pts.size() - 2) * 3);

  // O(n²) ear clipping
  while (indices.size() > 3) {
    bool found_ear = false;
    std::size_t n = indices.size();

    for (std::size_t i = 0; i < n; ++i) {
      std::size_t iprev = (i + n - 1) % n;
      std::size_t inext = (i + 1) % n;

      const Point& a = pts[static_cast<std::size_t>(indices[iprev])];
      const Point& b = pts[static_cast<std::size_t>(indices[i])];
      const Point& c = pts[static_cast<std::size_t>(indices[inext])];

      // CCW poligonda dışbükey köşe: cross(a→b, b→c) > 0
      float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
      if (cross <= 0.0f) continue;  // içbükey (reflex) köşe, kulak değil

      // Diğer hiçbir nokta bu üçgenin içinde olmamalı
      bool is_ear = true;
      for (std::size_t j = 0; j < n; ++j) {
        if (j == iprev || j == i || j == inext) continue;
        if (PointInTriangle(pts[static_cast<std::size_t>(indices[j])],
                            a, b, c)) {
          is_ear = false;
          break;
        }
      }

      if (is_ear) {
        result.push_back({a.x, a.y});
        result.push_back({b.x, b.y});
        result.push_back({c.x, c.y});
        indices.erase(indices.begin() + static_cast<std::ptrdiff_t>(i));
        found_ear = true;
        break;
      }
    }

    // Dejenere poligon koruması
    if (!found_ear) break;
  }

  // Son üçgeni ekle
  if (indices.size() == 3) {
    result.push_back({pts[static_cast<std::size_t>(indices[0])].x,
                      pts[static_cast<std::size_t>(indices[0])].y});
    result.push_back({pts[static_cast<std::size_t>(indices[1])].x,
                      pts[static_cast<std::size_t>(indices[1])].y});
    result.push_back({pts[static_cast<std::size_t>(indices[2])].x,
                      pts[static_cast<std::size_t>(indices[2])].y});
  }

  return result;
}

}  // namespace sdl_painter
