#pragma once

#include "sdl_painter/geometry.h"
#include "sdl_painter/vertex.h"

#include <vector>

namespace sdl_painter {

/// @brief Şekilleri backend-agnostic vertex listelerine dönüştürür.
///
/// Tessellator, geometrik şekilleri üçgenlere ve çizgilere böler.
/// Renderer veya OpenGL/Vulkan detayları hakkında hiçbir şey bilmez.
class Tessellator {
 public:
  // --- Dolu şekiller (üçgen listeleri) ---

  /// @brief Dolu dikdörtgen için vertex üret (2 üçgen, 6 vertex).
  static std::vector<Vertex> TessellateFilledRect(float x, float y, float w,
                                                  float h);

  /// @brief Dolu daire için vertex üret (triangle fan).
  /// Segment sayısı yarıçapa göre adaptif: max(16, radius * 0.5)
  static std::vector<Vertex> TessellateFilledCircle(float cx, float cy,
                                                    float radius);

  /// @brief Dolu elips için vertex üret (triangle fan).
  static std::vector<Vertex> TessellateFilledEllipse(float cx, float cy,
                                                     float rx, float ry);

  /// @brief Dolu poligon için vertex üret (ear clipping triangulation).
  /// Konkav poligonları destekler.
  static std::vector<Vertex> TessellateFilledPolygon(
      const std::vector<Point>& points);

  // --- Çerçeve şekiller (quad tabanlı kalın çizgi) ---

  /// @brief Dikdörtgen çerçevesi için vertex üret (4 kenar quad).
  static std::vector<Vertex> TessellateStrokedRect(float x, float y, float w,
                                                   float h, float line_width);

  /// @brief Daire çerçevesi için vertex üret (çevre boyunca quad'lar).
  static std::vector<Vertex> TessellateStrokedCircle(float cx, float cy,
                                                     float radius,
                                                     float line_width);

  /// @brief Elips çerçevesi için vertex üret.
  static std::vector<Vertex> TessellateStrokedEllipse(float cx, float cy,
                                                      float rx, float ry,
                                                      float line_width);

  /// @brief Çizgi segmenti için kalın quad vertex üret.
  static std::vector<Vertex> TessellateThickLine(float x1, float y1, float x2,
                                                 float y2, float line_width);

  /// @brief Çok noktalı polyline için kalın quad vertex üret.
  static std::vector<Vertex> TessellateThickPolyline(
      const std::vector<Point>& points, float line_width);

  /// @brief Poligon çerçevesi için kalın quad vertex üret (kapalı polyline).
  static std::vector<Vertex> TessellateStrokedPolygon(
      const std::vector<Point>& points, float line_width);

  // --- Texture koordinatlı vertex'ler ---

  /// @brief Texture'lı dikdörtgen için TexturedVertex üret.
  static std::vector<TexturedVertex> TessellateTexturedRect(float x, float y,
                                                            float w, float h,
                                                            float u0, float v0,
                                                            float u1, float v1);

 private:
  /// @brief Adaptif segment sayısı hesapla.
  static int32_t AdaptiveSegments(float radius) {
    constexpr int32_t kMinSegments = 16;
    return std::max(kMinSegments, static_cast<int32_t>(radius * 0.5f));
  }

  /// @brief Ear clipping iç implementasyonu.
  static std::vector<Vertex> EarClipping(const std::vector<Point>& points);

  /// @brief Üçgenin saat yönünde mi olduğunu kontrol et.
  static bool IsClockwise(const Point& a, const Point& b, const Point& c);

  /// @brief Nokta üçgenin içinde mi?
  static bool PointInTriangle(const Point& p, const Point& a, const Point& b,
                              const Point& c);
};

}  // namespace sdl_painter
