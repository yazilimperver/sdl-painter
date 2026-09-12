#pragma once

/// @file geometry_oracle.h
/// @brief Tessellator çıktısını tessellator kodunu kullanmadan doğrulayan
///        ölçütler.
///
/// Vertex sayısı bir mesh'in doğru olduğunu göstermez; bozuk bir
/// triangulation da doğru sayıda üçgen üretebilir. Buradaki ölçütler şeklin
/// kapladığı alanı ve her noktanın kaç üçgen tarafından kapsandığını ölçer.
/// Tessellator'daki geometri fonksiyonları bilerek kullanılmaz: aynı hatayı
/// paylaşan bir ölçüt o hatayı göremez.

#include "sdl_painter/geometry.h"
#include "sdl_painter/vertex.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sdl_painter::testing {

/// @brief `(b - a) x (p - a)`; p, a'dan b'ye giden doğrunun solundaysa pozitif.
inline double Orientation(const Vertex& a, const Vertex& b, double px,
                          double py) {
  return ((static_cast<double>(b.x) - a.x) * (py - a.y)) -
         ((static_cast<double>(b.y) - a.y) * (px - a.x));
}

/// @brief Üçgen listesindeki üçgenlerin mutlak alanları toplamı.
///
/// Basit bir poligonun doğru triangulation'ında poligon alanına eşittir.
/// Dışarı taşan veya üst üste binen her üçgen toplamı büyütür.
inline double TriangleListArea(const std::vector<Vertex>& vertices) {
  double sum = 0.0;
  for (std::size_t i = 0; i + 2 < vertices.size(); i += 3) {
    sum += std::fabs(Orientation(vertices[i], vertices[i + 1],
                                 vertices[i + 2].x, vertices[i + 2].y)) /
           2.0;
  }
  return sum;
}

/// @brief Poligonun alanı (shoelace formülü, sarım yönünden bağımsız).
inline double ShoelaceArea(const std::vector<Point>& polygon) {
  double twice = 0.0;
  for (std::size_t i = 0; i < polygon.size(); ++i) {
    const Point& p = polygon[i];
    const Point& q = polygon[(i + 1) % polygon.size()];
    twice +=
        (static_cast<double>(p.x) * q.y) - (static_cast<double>(q.x) * p.y);
  }
  return std::fabs(twice) / 2.0;
}

/// @brief (px, py) noktasını içeren üçgen sayısı.
///
/// Doğru bir dolgu veya çizgi mesh'inde şeklin içindeki nokta tam bir, dışındaki
/// nokta hiçbir üçgende bulunmaz. İki ve üstü, yarı saydam renkte o noktanın
/// birden fazla blend edileceği anlamına gelir.
///
/// Kenar üzerindeki nokta içeride sayılır; iki üçgenin ortak kenarındaki nokta
/// iki kez sayılacağı için örnek noktaları kenarlardan uzak seçin. Alanı sıfır
/// olan üçgenler atlanır.
inline int32_t CoverageCount(const std::vector<Vertex>& vertices, float px,
                             float py) {
  int32_t count = 0;
  for (std::size_t i = 0; i + 2 < vertices.size(); i += 3) {
    const Vertex& a = vertices[i];
    const Vertex& b = vertices[i + 1];
    const Vertex& c = vertices[i + 2];
    if (Orientation(a, b, c.x, c.y) == 0.0) {
      continue;
    }
    const double d1 = Orientation(a, b, px, py);
    const double d2 = Orientation(b, c, px, py);
    const double d3 = Orientation(c, a, px, py);
    const bool has_neg = d1 < 0.0 || d2 < 0.0 || d3 < 0.0;
    const bool has_pos = d1 > 0.0 || d2 > 0.0 || d3 > 0.0;
    if (!(has_neg && has_pos)) {
      ++count;
    }
  }
  return count;
}

}  // namespace sdl_painter::testing
