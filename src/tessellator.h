#pragma once

#include "sdl_painter/geometry.h"
#include "sdl_painter/vertex.h"

#include <algorithm>
#include <cstdint>
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
  ///
  /// Segmentler bağımsız quad'lar olarak üretilir; iç köşelerde kalan kama
  /// biçimli boşluklar **yuvarlak birleşim** (round join) diskleriyle
  /// doldurulur. Bu nedenle vertex sayısı segment sayısının katı değildir.
  /// Kalınlık 1.5 pikselin altındaysa birleşim eklenmez (görünmez, israf).
  static std::vector<Vertex> TessellateThickPolyline(
      const std::vector<Point>& points, float line_width);

  /// @brief Poligon çerçevesi için kalın quad vertex üret (kapalı polyline).
  ///
  /// @ref TessellateThickPolyline ile aynı birleşim davranışı; kapalı olduğu
  /// için **tüm** köşelere birleşim uygulanır.
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
  ///
  /// Alt sınır görsel kalite, üst sınır ise bellek/CPU koruması içindir:
  /// 512 segment zaten piksel altı hassasiyet demektir; sınırsız bırakmak
  /// büyük yarıçaplarda tek çağrıda yüz binlerce vertex üretiyordu.
  static int32_t AdaptiveSegments(float radius) {
    constexpr int32_t kMinSegments = 16;
    constexpr int32_t kMaxSegments = 512;
    if (!(radius > 0.0F)) {  // NaN ve negatif yarıçap koruması
      return kMinSegments;
    }
    const float kScaled = radius * 0.5F;
    if (kScaled >= static_cast<float>(kMaxSegments)) {
      return kMaxSegments;
    }
    return std::max(kMinSegments, static_cast<int32_t>(kScaled));
  }

  /// @brief Ardışık (ve kapanıştaki) çakışan noktaları eleyerek kopya döndür.
  static std::vector<Point> RemoveDuplicatePoints(
      const std::vector<Point>& points);

  /// @brief Açık/kapalı polyline için ortak kalın çizgi + birleşim üretimi.
  /// @param closed `true` ise son nokta ilkine bağlanır ve tüm köşelere
  ///        birleşim uygulanır.
  static std::vector<Vertex> TessellatePolyline(const std::vector<Point>& raw,
                                                float line_width, bool closed);

  /// @brief Köşeye yuvarlak birleşim diski ekle (merkez + çevre üçgen fanı).
  static void AppendRoundJoin(std::vector<Vertex>& out, const Point& center,
                              float radius);

  /// @brief Ear clipping iç implementasyonu.
  static std::vector<Vertex> EarClipping(const std::vector<Point>& raw);

  /// @brief Üçgenin saat yönünde mi olduğunu kontrol et.
  static bool IsClockwise(const Point& a, const Point& b, const Point& c);

  /// @brief Nokta üçgenin içinde mi?
  static bool PointInTriangle(const Point& p, const Point& a, const Point& b,
                              const Point& c);
};

}  // namespace sdl_painter
