#pragma once

#include "sdl_painter/geometry.h"
#include "sdl_painter/pen.h"
#include "sdl_painter/vertex.h"

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
  /// @brief Kalın çizginin tek segmenti: birim yön ve gövdenin dört köşesi.
  ///
  /// Sol, yönün +90° döndürülmüş hâlidir. Köşeler birleşimde kırpılabilsin diye
  /// gövde yazılmadan önce burada tutulur.
  struct StrokeSegment {
    float ux{0.0F};
    float uy{0.0F};
    float length{0.0F};
    Point start_left;
    Point start_right;
    Point end_left;
    Point end_right;
  };

  /// @brief Çağrılar arasında yeniden kullanılan ara diziler.
  ///
  /// Çağıran (Painter) bir kez oluşturur ve her çizimde aynı nesneyi verir;
  /// dizilerin kapasitesi korunduğu için kararlı durumda heap tahsisi olmaz.
  /// İçerik bir çağrıdan diğerine anlamsızdır.
  struct Scratch {
    std::vector<Point> points;            ///< Tekrarsız nokta kopyası.
    std::vector<StrokeSegment> segments;  ///< Çizginin segmentleri.
    std::vector<Vertex> joins;            ///< Birleşim üçgenleri.
    std::vector<Point> cleaned;          ///< Kesikli yolun tekrarsız kopyası.
    std::vector<Point> dash_points;      ///< Kesik parçaları, art arda.
    std::vector<std::size_t> dash_ends;  ///< Her parçanın bitiş indeksi.
    std::vector<Point> dash_run;         ///< Tessellate edilen tek parça.
    std::vector<std::int32_t> ear_indices;  ///< Ear clipping'te kalan köşeler.

    /// @brief Kenar yumuşatmada bir kenarın kaç üçgende geçtiği ve ilk
    ///        geçtiği üçgendeki yönü.
    struct AntialiasEdge {
      std::uint32_t count{0};
      std::uint32_t from{0};
      std::uint32_t to{0};
      std::uint32_t opposite{0};  ///< Üçgenin kenara karşı köşesi.
    };

    /// @brief @ref AntialiasTriangles'ın ara dizileri.
    struct Antialias {
      std::vector<std::uint64_t> cell_keys;    ///< Izgara tablosu: anahtar.
      std::vector<std::uint32_t> cell_values;  ///< Izgara tablosu: hücre.
      std::vector<Point> points;               ///< Kaynaşmış köşeler.
      std::vector<std::uint32_t> next;       ///< Aynı hücrede sonraki köşe.
      std::vector<std::uint32_t> cell_head;  ///< Hücrenin ilk köşesi.
      std::vector<std::uint32_t> cell_tail;  ///< Hücrenin son köşesi.
      std::vector<std::uint32_t> tris;       ///< Kaynaşmış üçgenler.
      std::vector<std::uint64_t> edge_keys;  ///< Kenar tablosu: anahtar.
      std::vector<std::uint32_t> edge_values;  ///< Kenar tablosu: indeks.
      std::vector<AntialiasEdge> edges;        ///< Kenarlar.
      std::vector<std::uint32_t> boundary;  ///< Sınır kenarlarının indeksi.
      std::vector<Point> normal_sum;  ///< Köşe başına normal toplamı.
      std::vector<std::uint32_t> boundary_degree;
      std::vector<Point> inner;  ///< İçeri çekilmiş köşeler.
      std::vector<Point> outer;  ///< Şeridin dış köşeleri.
    } aa;
  };

  /// @brief Dolu dikdörtgen için vertex üret (2 üçgen, 6 vertex).
  static std::vector<Vertex> TessellateFilledRect(float x, float y, float w,
                                                  float h);

  /// @brief @ref TessellateFilledRect; sonuç `out`'a yazılır (önce
  ///        temizlenir).
  static void TessellateFilledRect(float x, float y, float w, float h,
                                   std::vector<Vertex>& out);

  /// @brief Dolu daire için vertex üret (triangle fan).
  /// Segment sayısı yarıçapa göre adaptif (bkz. @ref AdaptiveSegments).
  /// @param pixel_scale Bkz. @ref AdaptiveSegments.
  static std::vector<Vertex> TessellateFilledCircle(float cx, float cy,
                                                    float radius,
                                                    float pixel_scale = 0.0F);

  /// @brief Dolu elips için vertex üret (triangle fan).
  /// Çevre noktaları @ref BuildEllipsePoints'ten gelir; çizgisiyle aynıdır.
  /// @param pixel_scale Bkz. @ref AdaptiveSegments.
  static std::vector<Vertex> TessellateFilledEllipse(float cx, float cy,
                                                     float rx, float ry,
                                                     float pixel_scale = 0.0F);

  /// @brief @ref TessellateFilledEllipse; çevre noktaları `scratch.points`'te,
  ///        sonuç `out`'ta (önce temizlenir).
  static void TessellateFilledEllipse(float cx, float cy, float rx, float ry,
                                      float pixel_scale, Scratch& scratch,
                                      std::vector<Vertex>& out);

  /// @brief Elipsin çevre noktalarını üret (çizgi/dolgu için ortak kaynak).
  ///
  /// Nokta sayısı büyük yarıçaptan @ref AdaptiveSegments ile belirlenir;
  /// daire, iki yarıçapı eşit elipstir. İlk nokta 0°'dedir (+x ekseni). Yol
  /// kapalıdır: ilk nokta sonda tekrarlanmaz.
  /// @param pixel_scale Bkz. @ref AdaptiveSegments.
  static std::vector<Point> BuildEllipsePoints(float cx, float cy, float rx,
                                               float ry,
                                               float pixel_scale = 0.0F);

  /// @brief @ref BuildEllipsePoints; sonuç `out`'a yazılır (önce temizlenir).
  static void BuildEllipsePoints(float cx, float cy, float rx, float ry,
                                 float pixel_scale, std::vector<Point>& out);

  /// @brief Dolu poligon için vertex üret (ear clipping triangulation).
  /// Konkav poligonları destekler.
  static std::vector<Vertex> TessellateFilledPolygon(
      const std::vector<Point>& points);

  /// @brief @ref TessellateFilledPolygon; ara diziler `scratch`'te, sonuç
  ///        `out`'ta (önce temizlenir). `points`, `scratch`'in bir dizisi
  ///        olmamalı.
  static void TessellateFilledPolygon(const std::vector<Point>& points,
                                      Scratch& scratch,
                                      std::vector<Vertex>& out);

  /// @brief Yuvarlatılmış dikdörtgenin dış hat noktalarını üret.
  ///
  /// Dört köşe yayı ve onları birleştiren dört doğru parçası. Sonuç
  /// konvekstir; dolgusu @ref TessellateFilledRoundedRect ile üretilir,
  /// @ref TessellateStrokedPolygon ise ek bir mantık gerektirmeden çalışır.
  ///
  /// Köşe çözünürlüğü @ref AdaptiveSegments ile belirlenir; yarıçap büyüdükçe
  /// daire ile aynı oranda artar.
  ///
  /// @param radius Köşe yarıçapı. `<= 0` ise düz dikdörtgenin dört köşesi
  ///        döner; `min(w, h) / 2`'yi aşarsa oraya kırpılır (stadyum şekli).
  /// @param pixel_scale Bkz. @ref AdaptiveSegments.
  /// @return En az dört nokta; `w` veya `h` pozitif değilse boş.
  static std::vector<Point> BuildRoundedRectPoints(float x, float y, float w,
                                                   float h, float radius,
                                                   float pixel_scale = 0.0F);

  /// @brief @ref BuildRoundedRectPoints; sonuç `out`'a yazılır (önce
  ///        temizlenir).
  static void BuildRoundedRectPoints(float x, float y, float w, float h,
                                     float radius, float pixel_scale,
                                     std::vector<Point>& out);

  /// @brief Dolu yuvarlatılmış dikdörtgen: merkezden üçgen fanı.
  ///
  /// Dış hat konveks ve merkez daima içeride olduğu için fan her boyutta
  /// şeklin tamamını kaplar. Ear clipping burada güvenilir değildi: yarıçap
  /// kısa kenarın yarısına eşitken (hap şekli) bazı boyutlarda kulak
  /// bulamıyor ve şeklin bir kısmını atlıyordu.
  ///
  /// @param radius @ref BuildRoundedRectPoints ile aynı kurallar.
  /// @param pixel_scale Bkz. @ref AdaptiveSegments.
  /// @return Üçgen listesi; `w` veya `h` pozitif değilse boş.
  static std::vector<Vertex> TessellateFilledRoundedRect(
      float x, float y, float w, float h, float radius,
      float pixel_scale = 0.0F);

  /// @brief @ref TessellateFilledRoundedRect; dış hat `scratch.points`'te,
  ///        sonuç `out`'ta (önce temizlenir).
  static void TessellateFilledRoundedRect(float x, float y, float w, float h,
                                          float radius, float pixel_scale,
                                          Scratch& scratch,
                                          std::vector<Vertex>& out);

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
  /// @param pixel_scale Bkz. @ref AdaptiveSegments.
  /// @return En az iki nokta; dejenere girdide boş.
  static std::vector<Point> BuildArcPoints(float cx, float cy, float rx,
                                           float ry, float start_degrees,
                                           float sweep_degrees,
                                           float pixel_scale = 0.0F);

  /// @brief @ref BuildArcPoints; sonuç `out`'a yazılır (önce temizlenir).
  static void BuildArcPoints(float cx, float cy, float rx, float ry,
                             float start_degrees, float sweep_degrees,
                             float pixel_scale, std::vector<Point>& out);

  /// @brief Dolu dilim (pie): merkez + yay. Üçgen fanı.
  /// @param pixel_scale Bkz. @ref AdaptiveSegments.
  static std::vector<Vertex> TessellateFilledPie(float cx, float cy, float rx,
                                                 float ry, float start_degrees,
                                                 float sweep_degrees,
                                                 float pixel_scale = 0.0F);

  /// @brief @ref TessellateFilledPie; yay `scratch.points`'te, sonuç `out`'ta
  ///        (önce temizlenir).
  static void TessellateFilledPie(float cx, float cy, float rx, float ry,
                                  float start_degrees, float sweep_degrees,
                                  float pixel_scale, Scratch& scratch,
                                  std::vector<Vertex>& out);

  /// @brief Dolu kiriş (chord): yay uçları düz bir kirişle kapatılır.
  /// @param pixel_scale Bkz. @ref AdaptiveSegments.
  static std::vector<Vertex> TessellateFilledChord(float cx, float cy, float rx,
                                                   float ry,
                                                   float start_degrees,
                                                   float sweep_degrees,
                                                   float pixel_scale = 0.0F);

  /// @brief @ref TessellateFilledChord; yay `scratch.points`'te, sonuç
  ///        `out`'ta (önce temizlenir).
  static void TessellateFilledChord(float cx, float cy, float rx, float ry,
                                    float start_degrees, float sweep_degrees,
                                    float pixel_scale, Scratch& scratch,
                                    std::vector<Vertex>& out);

  /// @brief Çizgi segmenti için kalın quad vertex üret.
  /// @param cap Her iki uca uygulanacak uç stili.
  static std::vector<Vertex> TessellateThickLine(float x1, float y1, float x2,
                                                 float y2, float line_width,
                                                 LineCap cap = LineCap::kButt);

  /// @brief Çok noktalı polyline için kalın quad vertex üret.
  ///
  /// Her segment bir quad'dır; iç köşelerde gövdeler iç kenarlarının kesişim
  /// noktasında kırpılır ve dış taraftaki boşluk `join` stiline göre
  /// doldurulur. Böylece yarı saydam çizgide köşe tek kez karışır. Kırpma
  /// payı komşu segmentlerden birinin yarısını aşarsa (çok keskin köşe veya
  /// kısa segment) o köşede gövdeler kırpılmaz ve iç tarafta örtüşür.
  /// Vertex sayısı segment sayısının katı değildir. Kalınlık 1.5 pikselin
  /// altındaysa birleşim ve kırpma yapılmaz (görünmez, israf).
  ///
  /// @param cap Yalnızca ilk ve son noktaya uygulanır; ara noktalarda uç
  ///        yoktur, orada birleşim vardır.
  static std::vector<Vertex> TessellateThickPolyline(
      const std::vector<Point>& points, float line_width,
      LineCap cap = LineCap::kButt, LineJoin join = LineJoin::kRound);

  /// @brief @ref TessellateThickPolyline; ara diziler `scratch`'te, sonuç
  ///        `out`'ta (önce temizlenir). `points`, `scratch`'in bir dizisi
  ///        olmamalı.
  static void TessellateThickPolyline(const std::vector<Point>& points,
                                      float line_width, LineCap cap,
                                      LineJoin join, Scratch& scratch,
                                      std::vector<Vertex>& out);

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

  /// @brief @ref TessellateDashedPolyline; ara diziler `scratch`'te, sonuç
  ///        `out`'ta (önce temizlenir). `points`, `scratch`'in bir dizisi
  ///        olmamalı.
  static void TessellateDashedPolyline(const std::vector<Point>& points,
                                       float line_width, const float* dash,
                                       std::size_t dash_count, bool closed,
                                       LineCap cap, LineJoin join,
                                       Scratch& scratch,
                                       std::vector<Vertex>& out);

  /// @brief Poligon çerçevesi için kalın quad vertex üret (kapalı polyline).
  ///
  /// @ref TessellateThickPolyline ile aynı birleşim davranışı; kapalı olduğu
  /// için tüm köşelere birleşim uygulanır ve uç stili anlamsızdır.
  static std::vector<Vertex> TessellateStrokedPolygon(
      const std::vector<Point>& points, float line_width,
      LineJoin join = LineJoin::kRound);

  /// @brief @ref TessellateStrokedPolygon; ara diziler `scratch`'te, sonuç
  ///        `out`'ta (önce temizlenir). `points`, `scratch`'in bir dizisi
  ///        olmamalı.
  static void TessellateStrokedPolygon(const std::vector<Point>& points,
                                       float line_width, LineJoin join,
                                       Scratch& scratch,
                                       std::vector<Vertex>& out);

  /// @brief Üçgen listesine kenar yumuşatma şeridi (fringe) ekle.
  ///
  /// Dolgu ve çizgi için aynı yöntem: köşeler konumlarına göre birleştirilir,
  /// yalnızca bir üçgene ait kenarlar sınır sayılır. Sınır köşeleri
  /// `fringe_width / 2` içeri çekilir ve her sınır kenarına içte alfa 255,
  /// dışta alfa 0 olan `fringe_width` genişliğinde bir şerit eklenir. Şerit
  /// eski kenarı ortaladığı için şeklin boyutu değişmez; dolgu ile şerit üst
  /// üste binmez, yarı saydam renkte çift karışım olmaz.
  ///
  /// Kapsama (coverage) vertex'in alfa kanalındadır; renk yazılırken fırçanın
  /// alfasıyla çarpılmalıdır. Girdinin alfası yok sayılır.
  ///
  /// Çok küçük şekillerde (sınır kutusunun kısa kenarı şerit genişliğinden
  /// azsa) içeri çekme yapılmaz, şerit yalnızca dışa yarım genişlikte uzanır;
  /// aksi halde şekil kendi üstüne katlanırdı.
  ///
  /// @param triangles Üçgen listesi (vertex sayısı 3'ün katı).
  /// @param fringe_width Şerit genişliği, girdi koordinatlarında (genellikle
  ///        1 ekran pikseli). Pozitif değilse girdi olduğu gibi döner.
  /// @return Yeni üçgen listesi; konum sonlu değilse girdi olduğu gibi döner.
  static std::vector<Vertex> AntialiasTriangles(
      const std::vector<Vertex>& triangles, float fringe_width);

  /// @brief @ref AntialiasTriangles; ara diziler `scratch.aa`'da, sonuç
  ///        `out`'ta (önce temizlenir). `out`, `triangles` olmamalı.
  static void AntialiasTriangles(const std::vector<Vertex>& triangles,
                                 float fringe_width, Scratch& scratch,
                                 std::vector<Vertex>& out);

  /// @brief Texture'lı dikdörtgen için TexturedVertex üret.
  static std::vector<TexturedVertex> TessellateTexturedRect(float x, float y,
                                                            float w, float h,
                                                            float u0, float v0,
                                                            float u1, float v1);

  /// @brief @ref TessellateTexturedRect; sonuç `out`'a yazılır (önce
  ///        temizlenir).
  static void TessellateTexturedRect(float x, float y, float w, float h,
                                     float u0, float v0, float u1, float v1,
                                     std::vector<TexturedVertex>& out);

 private:
  /// @brief Tam çember için adaptif segment sayısı hesapla.
  ///
  /// İki kural var:
  /// - `pixel_scale <= 0` (kenar yumuşatma kapalı): `max(16, radius * 0.5)`.
  ///   Basamaklı kenar küçük sapmaları zaten gizler.
  /// - `pixel_scale > 0` (kenar yumuşatma açık): kirişin yaydan sapması
  ///   ekranda 0,25 pikseli geçmeyecek kadar segment. Yumuşak kenarda köşeli
  ///   görünüm fark ediliyordu (r=30'da 16 segment 0,58 piksel sapıyordu).
  ///   Yarıçap `pixel_scale` ile ekran pikseline çevrilir; ölçek dönüşümün
  ///   geometrik ortalamasıdır, eşit olmayan ölçekte sapma uzun eksende
  ///   toleransı biraz aşabilir.
  ///
  /// Her iki kuralda sınırlar [16, 512]: alt sınır görsel kalite, üst sınır
  /// bellek/CPU koruması içindir. 512 segment zaten piksel altı hassasiyet
  /// demektir; sınırsız bırakmak büyük yarıçaplarda tek çağrıda yüz binlerce
  /// vertex üretiyordu.
  static int32_t AdaptiveSegments(float radius, float pixel_scale = 0.0F);

  /// @brief Ardışık çakışan noktaları eleyerek kopya döndür.
  /// @param closed `true` ise ilk noktayla çakışan son nokta da elenir; açık
  ///        yolda başlangıca dönüş korunur.
  static std::vector<Point> RemoveDuplicatePoints(
      const std::vector<Point>& points, bool closed);

  /// @brief @ref RemoveDuplicatePoints; sonuç `out`'a yazılır (önce
  ///        temizlenir). `out`, `points` olmamalı.
  static void RemoveDuplicatePoints(const std::vector<Point>& points,
                                    bool closed, std::vector<Point>& out);

  /// @brief Açık/kapalı polyline için ortak kalın çizgi + birleşim üretimi.
  ///
  /// Sonuç `out`'un sonuna eklenir (kesikli yol parçaları art arda yazar).
  /// Ara diziler `scratch.points`, `scratch.segments` ve `scratch.joins`;
  /// `raw` bunlardan biri olmamalı.
  /// @param closed `true` ise son nokta ilkine bağlanır ve tüm köşelere
  ///        birleşim uygulanır.
  static void TessellatePolyline(const std::vector<Point>& raw,
                                 float line_width, bool closed, LineCap cap,
                                 LineJoin join, Scratch& scratch,
                                 std::vector<Vertex>& out);

  /// @brief Köşeye yuvarlak birleşim diski ekle (merkez + çevre üçgen fanı).
  ///
  /// Yalnızca iç tarafta kırpılamayan köşelerde kullanılır (bkz.
  /// @ref TessellateThickPolyline).
  static void AppendRoundJoin(std::vector<Vertex>& out, const Point& center,
                              float radius);

  /// @brief Köşeye miter veya bevel birleşim ekle.
  ///
  /// Yalnızca iç tarafta kırpılamayan köşelerde kullanılır. Orada boşluk
  /// yalnızca dönüşün dış tarafındadır; iç taraf iki quad'ın üst üste
  /// binmesiyle doludur. Bu yüzden dış taraf çapraz çarpımın işaretinden
  /// bulunur ve tek taraf doldurulur.
  ///
  /// @param prev Köşeden önceki nokta.
  /// @param corner Birleşimin uygulanacağı köşe.
  /// @param next Köşeden sonraki nokta.
  /// @param miter `true` ise miter denenir (sınır aşılırsa bevel'a düşer).
  static void AppendMiterOrBevelJoin(std::vector<Vertex>& out,
                                     const Point& prev, const Point& corner,
                                     const Point& next, float half_width,
                                     bool miter);

  /// @brief Kesik desenini yol boyunca yürütüp "çizili" parçaları üret.
  ///
  /// Her parça en az iki noktalı bir açık polyline'dır. Parçalar
  /// `run_points`'e art arda yazılır; `run_ends[i]`, i. parçanın bittiği
  /// indekstir (parça bir önceki bitişten başlar). İki dizi de önce
  /// temizlenir.
  static void BuildDashRuns(const std::vector<Point>& points, const float* dash,
                            std::size_t dash_count, bool closed,
                            std::vector<Point>& run_points,
                            std::vector<std::size_t>& run_ends);

  /// @brief Açık uca `cap` stiline göre geometri ekle.
  ///
  /// Uç geometrisi gövdenin uçtaki iki köşesinden başlar ve gövdeyle yalnızca
  /// o kenarı paylaşır; yarı saydam renkte örtüşen bölge kalmaz.
  ///
  /// @param tip Uç noktası.
  /// @param outward_x `tip`'ten dışarı bakan birim yön.
  /// @param outward_y Aynı yönün y bileşeni.
  /// @param left Gövdenin uçtaki köşelerinden biri.
  /// @param right Diğer köşe.
  static void AppendCap(std::vector<Vertex>& out, const Point& tip,
                        float outward_x, float outward_y, const Point& left,
                        const Point& right, float half_width, LineCap cap);

  /// @brief Ear clipping iç implementasyonu.
  ///
  /// Ara diziler `scratch.points` ve `scratch.ear_indices`; sonuç `out`'un
  /// sonuna eklenir. `raw`, `scratch.points` olmamalı.
  static void EarClipping(const std::vector<Point>& raw, Scratch& scratch,
                          std::vector<Vertex>& out);

  /// @brief Nokta üçgenin içinde veya kenarında mı?
  ///
  /// Üçgenin köşeleriyle çakışan nokta dışarıda sayılır: kulak testinde
  /// poligonun başka yerinde tekrar eden köşe kulağı engellememeli.
  static bool PointInTriangle(const Point& p, const Point& a, const Point& b,
                              const Point& c);
};

}  // namespace sdl_painter
