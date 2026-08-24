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

// --- Yay tabanlı şekiller ---

std::vector<Point> Tessellator::BuildArcPoints(float cx, float cy, float rx,
                                               float ry, float start_degrees,
                                               float sweep_degrees) {
  if (!(rx > 0.0F) || !(ry > 0.0F)) {
    return {};
  }
  // Tam turdan fazlası görsel olarak tam turdan farksızdır; kırpmak, çok
  // büyük açı değerlerinde segment sayısının patlamasını da engeller.
  float sweep = std::clamp(sweep_degrees, -360.0F, 360.0F);
  if (std::fabs(sweep) < 1e-4F) {
    return {};
  }

  constexpr float kDegToRad = 3.14159265358979323846F / 180.0F;

  // Segment sayısı yarıçapa göre uyarlanır, sonra taranan açı oranına
  // düşürülür. Alt sınır 2: bir yayı en az bir doğru parçası temsil etmeli.
  const int32_t kFullSegments = AdaptiveSegments(std::max(rx, ry));
  const float kFraction = std::fabs(sweep) / 360.0F;
  const auto kSegments = std::max(
      2, static_cast<int32_t>(static_cast<float>(kFullSegments) * kFraction));

  std::vector<Point> pts;
  pts.reserve(static_cast<std::size_t>(kSegments) + 1);
  for (int32_t i = 0; i <= kSegments; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(kSegments);
    const float a = (start_degrees + sweep * t) * kDegToRad;
    pts.emplace_back(cx + std::cos(a) * rx, cy + std::sin(a) * ry);
  }
  return pts;
}

std::vector<Vertex> Tessellator::TessellateFilledPie(float cx, float cy,
                                                     float rx, float ry,
                                                     float start_degrees,
                                                     float sweep_degrees) {
  const std::vector<Point> arc =
      BuildArcPoints(cx, cy, rx, ry, start_degrees, sweep_degrees);
  if (arc.size() < 2) {
    return {};
  }
  // Merkezden üçgen fanı: dilim daima yıldız-konveks olduğu için ear
  // clipping'e gerek yok.
  std::vector<Vertex> result;
  result.reserve((arc.size() - 1) * 3);
  for (std::size_t i = 0; i + 1 < arc.size(); ++i) {
    result.emplace_back(cx, cy);
    result.emplace_back(arc[i].x, arc[i].y);
    result.emplace_back(arc[i + 1].x, arc[i + 1].y);
  }
  return result;
}

std::vector<Vertex> Tessellator::TessellateFilledChord(float cx, float cy,
                                                       float rx, float ry,
                                                       float start_degrees,
                                                       float sweep_degrees) {
  const std::vector<Point> arc =
      BuildArcPoints(cx, cy, rx, ry, start_degrees, sweep_degrees);
  if (arc.size() < 3) {
    return {};
  }
  // Kiriş bölgesi konvekstir (yay + onu kapatan doğru parçası), bu yüzden
  // ilk noktadan üçgen fanı yeterli; ear clipping'in O(n²) maliyeti gereksiz.
  std::vector<Vertex> result;
  result.reserve((arc.size() - 2) * 3);
  for (std::size_t i = 1; i + 1 < arc.size(); ++i) {
    result.emplace_back(arc[0].x, arc[0].y);
    result.emplace_back(arc[i].x, arc[i].y);
    result.emplace_back(arc[i + 1].x, arc[i + 1].y);
  }
  return result;
}

// --- Çerçeve şekiller ---

std::vector<Vertex> Tessellator::TessellateStrokedRect(
    float x, float y, float w, float h, float line_width, LineJoin join,
    const float* dash, std::size_t dash_count, LineCap cap) {
  const std::vector<Point> pts = {
      {x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
  if (dash != nullptr && dash_count > 0) {
    return TessellateDashedPolyline(pts, line_width, dash, dash_count,
                                    /*closed=*/true, cap, join);
  }
  return TessellateStrokedPolygon(pts, line_width, join);
}

std::vector<Vertex> Tessellator::TessellateStrokedCircle(
    float cx, float cy, float radius, float line_width, LineJoin join,
    const float* dash, std::size_t dash_count, LineCap cap) {
  int32_t segments = AdaptiveSegments(radius);
  std::vector<Point> pts;
  pts.reserve(static_cast<std::size_t>(segments));

  for (int32_t i = 0; i < segments; ++i) {
    float a = kTwoPi * static_cast<float>(i) / static_cast<float>(segments);
    pts.emplace_back(cx + std::cos(a) * radius, cy + std::sin(a) * radius);
  }
  if (dash != nullptr && dash_count > 0) {
    return TessellateDashedPolyline(pts, line_width, dash, dash_count,
                                    /*closed=*/true, cap, join);
  }
  return TessellateStrokedPolygon(pts, line_width, join);
}

std::vector<Vertex> Tessellator::TessellateStrokedEllipse(
    float cx, float cy, float rx, float ry, float line_width, LineJoin join,
    const float* dash, std::size_t dash_count, LineCap cap) {
  int32_t segments = AdaptiveSegments(std::max(rx, ry));
  std::vector<Point> pts;
  pts.reserve(static_cast<std::size_t>(segments));

  for (int32_t i = 0; i < segments; ++i) {
    float a = kTwoPi * static_cast<float>(i) / static_cast<float>(segments);
    pts.emplace_back(cx + std::cos(a) * rx, cy + std::sin(a) * ry);
  }
  if (dash != nullptr && dash_count > 0) {
    return TessellateDashedPolyline(pts, line_width, dash, dash_count,
                                    /*closed=*/true, cap, join);
  }
  return TessellateStrokedPolygon(pts, line_width, join);
}

std::vector<Vertex> Tessellator::TessellateThickLine(float x1, float y1,
                                                     float x2, float y2,
                                                     float line_width,
                                                     LineCap cap) {
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

  std::vector<Vertex> result = {
      {p0x, p0y}, {p1x, p1y}, {p2x, p2y},
      {p1x, p1y}, {p3x, p3y}, {p2x, p2y},
  };
  // clang-format on

  if (cap != LineCap::kButt) {
    const float ux = dx / len;
    const float uy = dy / len;
    AppendCap(result, Point{x1, y1}, -ux, -uy, hw, cap);
    AppendCap(result, Point{x2, y2}, ux, uy, hw, cap);
  }
  return result;
}

void Tessellator::AppendCap(std::vector<Vertex>& out, const Point& tip,
                            float outward_x, float outward_y, float half_width,
                            LineCap cap) {
  if (cap == LineCap::kButt) {
    return;
  }
  if (cap == LineCap::kRound) {
    // Tam disk: yarısı zaten quad'ın altında kalır. Yarım disk üretmek
    // vertex sayısını yarıya indirirdi ama açı aralığını yön vektöründen
    // hesaplamayı gerektirir; kazanç, birkaç üçgen için karmaşıklığa değmez.
    AppendRoundJoin(out, tip, half_width);
    return;
  }
  // Kare uç: çizgiyi yarım kalınlık kadar uzatan ek bir quad.
  // Yön vektörü burada normalize edilir; çağıranların hazır birim vektör
  // tutmak zorunda kalmaması için (polyline uçlarında elde ham fark var).
  const float len = std::sqrt(outward_x * outward_x + outward_y * outward_y);
  if (len < 1e-6F) {
    return;
  }
  outward_x /= len;
  outward_y /= len;

  const float nx = -outward_y;
  const float ny = outward_x;
  const float ex = tip.x + outward_x * half_width;
  const float ey = tip.y + outward_y * half_width;

  const float ax = tip.x + nx * half_width;
  const float ay = tip.y + ny * half_width;
  const float bx = tip.x - nx * half_width;
  const float by = tip.y - ny * half_width;
  const float cx = ex + nx * half_width;
  const float cy = ey + ny * half_width;
  const float dx2 = ex - nx * half_width;
  const float dy2 = ey - ny * half_width;

  out.emplace_back(ax, ay);
  out.emplace_back(bx, by);
  out.emplace_back(cx, cy);
  out.emplace_back(bx, by);
  out.emplace_back(dx2, dy2);
  out.emplace_back(cx, cy);
}

void Tessellator::AppendMiterOrBevelJoin(std::vector<Vertex>& out,
                                         const Point& prev, const Point& corner,
                                         const Point& next, float half_width,
                                         bool miter) {
  const float d1x = corner.x - prev.x;
  const float d1y = corner.y - prev.y;
  const float d2x = next.x - corner.x;
  const float d2y = next.y - corner.y;
  const float len1 = std::sqrt(d1x * d1x + d1y * d1y);
  const float len2 = std::sqrt(d2x * d2x + d2y * d2y);
  if (len1 < 1e-6F || len2 < 1e-6F) {
    return;
  }

  const float u1x = d1x / len1;
  const float u1y = d1y / len1;
  const float u2x = d2x / len2;
  const float u2y = d2y / len2;

  // Neredeyse düz köşede boşluk yok; miter noktası da sonsuza kaçar.
  const float cross = u1x * u2y - u1y * u2x;
  if (std::fabs(cross) < 1e-6F) {
    return;
  }

  // Boşluk dönüşün dış tarafındadır: sola dönüşte sağ taraf, sağa dönüşte sol.
  const float side = (cross > 0.0F) ? -1.0F : 1.0F;
  const float n1x = -u1y * side;
  const float n1y = u1x * side;
  const float n2x = -u2y * side;
  const float n2y = u2x * side;

  const Point a{corner.x + n1x * half_width, corner.y + n1y * half_width};
  const Point b{corner.x + n2x * half_width, corner.y + n2y * half_width};

  if (miter) {
    // Miter noktası, iki dış kenarın kesişimi: köşe açıortayı yönünde
    // half_width / cos(θ/2) uzaklıkta.
    float mx = n1x + n2x;
    float my = n1y + n2y;
    const float mlen = std::sqrt(mx * mx + my * my);
    if (mlen > 1e-6F) {
      mx /= mlen;
      my /= mlen;
      const float cos_half = mx * n1x + my * n1y;
      if (cos_half > 1e-6F) {
        const float ratio = 1.0F / cos_half;
        if (ratio <= kMiterLimit) {
          const Point m{corner.x + mx * half_width * ratio,
                        corner.y + my * half_width * ratio};
          out.emplace_back(corner.x, corner.y);
          out.emplace_back(a.x, a.y);
          out.emplace_back(m.x, m.y);
          out.emplace_back(corner.x, corner.y);
          out.emplace_back(m.x, m.y);
          out.emplace_back(b.x, b.y);
          return;
        }
      }
    }
    // Sınır aşıldı veya dejenere: bevel'a düş.
  }

  out.emplace_back(corner.x, corner.y);
  out.emplace_back(a.x, a.y);
  out.emplace_back(b.x, b.y);
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
    const std::vector<Point>& points, float line_width, LineCap cap,
    LineJoin join) {
  return TessellatePolyline(points, line_width, /*closed=*/false, cap, join);
}

std::vector<std::vector<Point>> Tessellator::BuildDashRuns(
    const std::vector<Point>& points, const float* dash, std::size_t dash_count,
    bool closed) {
  std::vector<std::vector<Point>> runs;
  if (points.size() < 2 || dash == nullptr || dash_count == 0) {
    return runs;
  }

  // Desen indeksi monoton artar; "çizili mi?" sorusu indeksin **çift olup
  // olmadığından** gelir, uzunluk ise indeksin desen boyuna göre modundan.
  // Tek sayıda uzunlukta bu, deseni iki tur boyunca tersine çevirir — SVG'nin
  // stroke-dasharray davranışı.
  std::size_t pattern_index = 0;
  bool drawing = true;
  float remaining = dash[0];

  std::vector<Point> current;
  if (drawing) {
    current.push_back(points.front());
  }

  const std::size_t kSegmentCount = closed ? points.size() : points.size() - 1;
  for (std::size_t i = 0; i < kSegmentCount; ++i) {
    const Point& a = points[i];
    const Point& b = points[(i + 1) % points.size()];
    const float seg_dx = b.x - a.x;
    const float seg_dy = b.y - a.y;
    const float seg_len = std::sqrt(seg_dx * seg_dx + seg_dy * seg_dy);
    if (seg_len < 1e-6F) {
      continue;
    }
    const float ux = seg_dx / seg_len;
    const float uy = seg_dy / seg_len;

    float travelled = 0.0F;
    while (seg_len - travelled > remaining) {
      travelled += remaining;
      const Point split{a.x + ux * travelled, a.y + uy * travelled};

      if (drawing) {
        current.push_back(split);
        if (current.size() >= 2) {
          runs.push_back(current);
        }
        current.clear();
      } else {
        current.clear();
        current.push_back(split);
      }

      drawing = !drawing;
      ++pattern_index;
      remaining = dash[pattern_index % dash_count];
    }

    remaining -= (seg_len - travelled);
    // Köşe noktası: çizili bir parçanın ortasına denk geliyorsa parçaya
    // eklenir; böylece kesik köşenin üzerinden geçtiğinde birleşim alır.
    if (drawing) {
      current.push_back(b);
    }
  }

  if (drawing && current.size() >= 2) {
    runs.push_back(current);
  }
  return runs;
}

std::vector<Vertex> Tessellator::TessellateDashedPolyline(
    const std::vector<Point>& points, float line_width, const float* dash,
    std::size_t dash_count, bool closed, LineCap cap, LineJoin join) {
  const std::vector<Point> cleaned = RemoveDuplicatePoints(points);
  if (cleaned.size() < 2 || !(line_width > 0.0F)) {
    return {};
  }

  // Desen geçersizse kesintisiz çiz. "Geçersiz" yalnızca boş desen değil:
  // uzunluklardan biri bile pozitif değilse yürüme döngüsü o adımda hiç
  // ilerlemez ve **sonsuza kadar döner**. Pen::SetDashPattern pozitif olmayan
  // değerleri zaten eler; bu kontrol, tessellator'ı doğrudan çağıranlar için.
  bool valid = dash != nullptr && dash_count > 0;
  for (std::size_t i = 0; valid && i < dash_count; ++i) {
    valid = dash[i] > 0.0F;
  }
  if (!valid) {
    return TessellatePolyline(cleaned, line_width, closed, cap, join);
  }

  std::vector<Vertex> result;
  for (const auto& run : BuildDashRuns(cleaned, dash, dash_count, closed)) {
    // Her çizili parça kendi içinde AÇIK bir polyline'dır — kapalı şeklin
    // kesiği de açık parçalardan oluşur.
    auto piece =
        TessellatePolyline(run, line_width, /*closed=*/false, cap, join);
    result.insert(result.end(), piece.begin(), piece.end());
  }
  return result;
}

std::vector<Vertex> Tessellator::TessellateStrokedPolygon(
    const std::vector<Point>& points, float line_width, LineJoin join) {
  // Kapalı geometride uç yoktur; cap değeri kullanılmaz.
  return TessellatePolyline(points, line_width, /*closed=*/true, LineCap::kButt,
                            join);
}

std::vector<Vertex> Tessellator::TessellatePolyline(
    const std::vector<Point>& raw, float line_width, bool closed, LineCap cap,
    LineJoin join) {
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
  // geometri eklemek yalnızca vertex israfı olur. Aynı eşik uçlar için de
  // geçerli: yarım pikselden kısa bir uzatma görünmez.
  constexpr float kMinJoinWidth = 1.5F;
  if (line_width >= kMinJoinWidth) {
    const float kRadius = line_width * 0.5F;
    // Açık polyline'da yalnızca iç köşeler; kapalı poligonda tüm köşeler
    // (ilk köşe de son segmentin bitişidir).
    const std::size_t kFirst = closed ? 0 : 1;
    const std::size_t kLast = closed ? points.size() : points.size() - 1;
    for (std::size_t i = kFirst; i < kLast; ++i) {
      if (join == LineJoin::kRound) {
        AppendRoundJoin(result, points[i], kRadius);
      } else {
        const Point& prev = points[(i + points.size() - 1) % points.size()];
        const Point& next = points[(i + 1) % points.size()];
        AppendMiterOrBevelJoin(result, prev, points[i], next, kRadius,
                               join == LineJoin::kMiter);
      }
    }

    // Uçlar yalnızca açık geometride ve yalnızca iki uç noktada.
    if (!closed && cap != LineCap::kButt) {
      const Point& head = points.front();
      const Point& after_head = points[1];
      const Point& tail = points.back();
      const Point& before_tail = points[points.size() - 2];
      AppendCap(result, head, head.x - after_head.x, head.y - after_head.y,
                kRadius, cap);
      AppendCap(result, tail, tail.x - before_tail.x, tail.y - before_tail.y,
                kRadius, cap);
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
