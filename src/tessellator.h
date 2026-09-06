#pragma once

#include "sdl_painter/geometry.h"
#include "sdl_painter/pen.h"
#include "sdl_painter/vertex.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sdl_painter {

/// @brief Şekilleri backend-agnostic vertex listelerine dönüştürür.
///
/// Tessellator, geometrik şekilleri üçgenlere ve çizgilere böler.
/// Renderer veya OpenGL/Vulkan detayları hakkında hiçbir şey bilmez.
/// @note Çizgi stili parametreleri (kalınlık, birleşim, kesik, uç) imzaların
///       sonuna eklenerek taşınıyor. Beşinci bir stil özelliği gerekirse bu
///       liste bir `StrokeStyle` struct'ına dönüşmelidir; şu anda çağıranların
///       ve testlerin tamamını değiştirmemek için böyle bırakıldı.
class Tessellator {
 public:
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

  /// @brief Yuvarlatılmış dikdörtgenin dış hat noktalarını üret.
  ///
  /// Dört köşe yayı ve onları birleştiren dört doğru parçası. Sonuç
  /// konvekstir, bu yüzden hem @ref TessellateFilledPolygon hem de
  /// @ref TessellateStrokedPolygon ek bir mantık gerektirmeden çalışır.
  ///
  /// Köşe çözünürlüğü @ref AdaptiveSegments ile belirlenir; yarıçap büyüdükçe
  /// daire ile aynı oranda artar.
  ///
  /// @param radius Köşe yarıçapı. `<= 0` ise düz dikdörtgenin dört köşesi
  ///        döner; `min(w, h) / 2`'yi aşarsa oraya kırpılır (stadyum şekli).
  /// @return En az dört nokta; `w` veya `h` pozitif değilse boş.
  static std::vector<Point> BuildRoundedRectPoints(float x, float y, float w,
                                                   float h, float radius);

  // Açı birimi derece. 0° = +x ekseni; açı, @ref Painter::Rotate ile aynı
  // yönde artar (dolayısıyla dolu daire tessellation'ıyla da aynı yönde).
  // Bilinçli olarak Qt'nin 1/16 derece + ters yön sözleşmesi izlenmez;
  // kütüphane içi tutarlılık tercih edildi.
  //
  // Segment sayısı hem yarıçapa hem de taranan açıya göre uyarlanır: 10°'lik
  // bir yay, tam çemberle aynı sayıda segment harcamamalı.

  /// @brief Yay üzerindeki noktaları üret (çizgi/dolgu için ortak kaynak).
  ///
  /// @param sweep_degrees Taranan açı. Negatif olabilir (ters yön). Mutlak
  ///        değeri 360°'yi aşarsa 360°'ye kırpılır.
  /// @return En az iki nokta; dejenere girdide boş.
  static std::vector<Point> BuildArcPoints(float cx, float cy, float rx,
                                           float ry, float start_degrees,
                                           float sweep_degrees);

  /// @brief Dolu dilim (pie): merkez + yay. Üçgen fanı.
  static std::vector<Vertex> TessellateFilledPie(float cx, float cy, float rx,
                                                 float ry, float start_degrees,
                                                 float sweep_degrees);

  /// @brief Dolu kiriş (chord): yay uçları düz bir kirişle kapatılır.
  static std::vector<Vertex> TessellateFilledChord(float cx, float cy, float rx,
                                                   float ry,
                                                   float start_degrees,
                                                   float sweep_degrees);

  /// @brief Dikdörtgen çerçevesi için vertex üret (4 kenar quad).
  ///
  /// @param dash Kesik deseni (bkz. @ref TessellateDashedPolyline). `nullptr`
  ///        veya `dash_count == 0` ise kesintisiz.
  /// @param cap Yalnızca kesikliyken anlamlı: her kesik parçasının uçlarına
  ///        uygulanır. Kesintisiz kapalı şeklin ucu yoktur.
  static std::vector<Vertex> TessellateStrokedRect(
      float x, float y, float w, float h, float line_width,
      LineJoin join = LineJoin::kRound, const float* dash = nullptr,
      std::size_t dash_count = 0, LineCap cap = LineCap::kButt);

  /// @brief Daire çerçevesi için vertex üret (çevre boyunca quad'lar).
  ///
  /// @note Segment sayısı yüksek olduğundan köşeler neredeyse düzdür; birleşim
  ///       stilinin görünür etkisi yok denecek kadar azdır. Parametre yalnızca
  ///       kalemin tek bir stil kaynağı olması için taşınır.
  static std::vector<Vertex> TessellateStrokedCircle(
      float cx, float cy, float radius, float line_width,
      LineJoin join = LineJoin::kRound, const float* dash = nullptr,
      std::size_t dash_count = 0, LineCap cap = LineCap::kButt);

  /// @brief Elips çerçevesi için vertex üret.
  static std::vector<Vertex> TessellateStrokedEllipse(
      float cx, float cy, float rx, float ry, float line_width,
      LineJoin join = LineJoin::kRound, const float* dash = nullptr,
      std::size_t dash_count = 0, LineCap cap = LineCap::kButt);

  /// @brief Çizgi segmenti için kalın quad vertex üret.
  /// @param cap Her iki uca uygulanacak uç stili.
  static std::vector<Vertex> TessellateThickLine(float x1, float y1, float x2,
                                                 float y2, float line_width,
                                                 LineCap cap = LineCap::kButt);

  /// @brief Çok noktalı polyline için kalın quad vertex üret.
  ///
  /// Segmentler bağımsız quad'lar olarak üretilir; iç köşelerde kalan kama
  /// biçimli boşluklar `join` stiline göre doldurulur. Bu nedenle vertex
  /// sayısı segment sayısının katı değildir. Kalınlık 1.5 pikselin altındaysa
  /// birleşim eklenmez (görünmez, israf).
  ///
  /// @param cap Yalnızca ilk ve son noktaya uygulanır; ara noktalarda uç
  ///        yoktur, orada birleşim vardır.
  static std::vector<Vertex> TessellateThickPolyline(
      const std::vector<Point>& points, float line_width,
      LineCap cap = LineCap::kButt, LineJoin join = LineJoin::kRound);

  /// @brief Kesikli (dashed) polyline için kalın quad vertex üret.
  ///
  /// Desen yol boyunca sürekli ilerler: bir köşede yarım kalan çizili
  /// parça sonraki segmentte kaldığı yerden devam eder, yani bir kesik
  /// köşenin üzerinden geçebilir. Böyle bir parça kendi içinde bir polyline
  /// olduğundan köşesine birleşim de uygulanır.
  ///
  /// Her çizili parça açık bir polyline'dır; uç stili her parçanın iki
  /// ucuna ayrı ayrı uygulanır (kesikli bir çizginin yuvarlak uçlu olması
  /// budur).
  ///
  /// @param dash Uzunluklar: çizili / boş sırayla. Tek sayıda verilirse desen
  ///        iki tur boyunca kendini tersine çevirerek tamamlanır (SVG davranışı).
  /// @param dash_count `dash` içindeki geçerli uzunluk sayısı. 0 ise veya
  ///        toplam uzunluk sıfırsa kesintisiz çizim yapılır.
  /// @param closed `true` ise son nokta ilkine bağlanır.
  static std::vector<Vertex> TessellateDashedPolyline(
      const std::vector<Point>& points, float line_width, const float* dash,
      std::size_t dash_count, bool closed, LineCap cap = LineCap::kButt,
      LineJoin join = LineJoin::kRound);

  /// @brief Poligon çerçevesi için kalın quad vertex üret (kapalı polyline).
  ///
  /// @ref TessellateThickPolyline ile aynı birleşim davranışı; kapalı olduğu
  /// için tüm köşelere birleşim uygulanır ve uç stili anlamsızdır.
  static std::vector<Vertex> TessellateStrokedPolygon(
      const std::vector<Point>& points, float line_width,
      LineJoin join = LineJoin::kRound);

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
                                                float line_width, bool closed,
                                                LineCap cap, LineJoin join);

  /// @brief Köşeye yuvarlak birleşim diski ekle (merkez + çevre üçgen fanı).
  static void AppendRoundJoin(std::vector<Vertex>& out, const Point& center,
                              float radius);

  /// @brief Köşeye miter veya bevel birleşim ekle.
  ///
  /// Boşluk yalnızca dönüşün dış tarafındadır; iç taraf zaten iki quad'ın
  /// üst üste binmesiyle doludur. Bu yüzden dış taraf çapraz çarpımın
  /// işaretinden bulunur ve tek taraf doldurulur.
  ///
  /// @param prev Köşeden önceki nokta.
  /// @param corner Birleşimin uygulanacağı köşe.
  /// @param next Köşeden sonraki nokta.
  /// @param miter `true` ise miter denenir (sınır aşılırsa bevel'a düşer).
  static void AppendMiterOrBevelJoin(std::vector<Vertex>& out,
                                     const Point& prev, const Point& corner,
                                     const Point& next, float half_width,
                                     bool miter);

  /// @brief Kesik desenini yol boyunca yürütüp "çizili" parçaları döndür.
  ///
  /// Her parça en az iki noktalı bir açık polyline'dır.
  static std::vector<std::vector<Point>> BuildDashRuns(
      const std::vector<Point>& points, const float* dash,
      std::size_t dash_count, bool closed);

  /// @brief Açık uca `cap` stiline göre geometri ekle.
  ///
  /// @param tip Uç noktası.
  /// @param outward_x `tip`'ten dışarı bakan yön (normalize edilmesi gerekmez).
  /// @param outward_y Aynı yönün y bileşeni.
  static void AppendCap(std::vector<Vertex>& out, const Point& tip,
                        float outward_x, float outward_y, float half_width,
                        LineCap cap);

  /// @brief Ear clipping iç implementasyonu.
  static std::vector<Vertex> EarClipping(const std::vector<Point>& raw);

  /// @brief Üçgenin saat yönünde mi olduğunu kontrol et.
  static bool IsClockwise(const Point& a, const Point& b, const Point& c);

  /// @brief Nokta üçgenin içinde mi?
  static bool PointInTriangle(const Point& p, const Point& a, const Point& b,
                              const Point& c);
};

}  // namespace sdl_painter
