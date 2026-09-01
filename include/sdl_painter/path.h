#pragma once

#include "sdl_painter/export.h"
#include "sdl_painter/geometry.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sdl_painter {

/// @brief Varsayılan düzleştirme toleransı (piksel).
///
/// Eğri ile onu temsil eden kırık çizgi arasındaki azami sapma. Yarım pikselin
/// altı, ekranda ayırt edilemez; 0.25 bunun da altında kalarak ölçeklenmiş
/// çizimde bir miktar pay bırakır.
inline constexpr float kDefaultFlatness = 0.25F;

/// @brief Bir @ref Path içindeki tek parça — düzleştirilmiş nokta dizisi.
///
/// Eğriler @ref Path içine eklenirken kırık çizgiye çevrilir; burada artık
/// Bézier kontrol noktası yoktur, yalnızca çizilecek noktalar vardır.
struct SubPath {
  /// @brief Parçanın noktaları (en az bir tane).
  std::vector<Point> points;

  /// @brief @ref Path::Close ile kapatıldı mı?
  ///
  /// Kapalı parçanın son noktası ilkine bağlanır ve çizerken tüm köşelere
  /// birleşim uygulanır; açık parçanın iki ucuna uç stili uygulanır.
  bool closed{false};
};

/// @brief Doğru parçaları ve Bézier eğrilerinden oluşan çizim yolu.
///
/// QPainter'daki `QPainterPath` karşılığı. Yol, birbirinden bağımsız
/// alt yollardan (@ref SubPath) oluşur; her @ref MoveTo yeni bir alt yol
/// başlatır. @ref Painter::DrawPath yolun çerçevesini, @ref Painter::FillPath
/// dolgusunu çizer.
///
/// @par Eğriler eklenirken düzleştirilir
/// Kontrol noktaları saklanmaz. Bir eğri eklendiği anda, @ref Flatness
/// toleransını sağlayacak sayıda doğru parçasına bölünür ve yalnızca sonuç
/// noktaları tutulur. Bu, çizim yolunu basit tutar (mevcut kalın çizgi ve
/// ear clipping tessellation'ı olduğu gibi kullanılır) ve aynı yolu birçok
/// kez çizen kodun düzleştirme bedelini bir kez ödemesini sağlar.
///
/// @warning Bunun bedeli: yol kurulduktan sonra @ref Painter::Scale ile
///          büyütülürse düzleştirme o ölçeğe göre yapılmadığı için eğride
///          köşelenme görünebilir. Büyük ölçekte çizilecek bir yol, daha
///          küçük bir @ref Flatness değeriyle kurulmalıdır.
///
/// @par Dolgu ve delikler
/// @ref Painter::FillPath her alt yolu bağımsız olarak doldurur; bir alt
/// yolun diğerinin içinde kalması onu delik yapmaz (even-odd / nonzero dolgu
/// kuralı uygulanmaz). "O" harfi gibi delikli şekiller bu sürümde doğru
/// doldurulmaz.
///
/// @code
/// sdl_painter::Path path;
/// path.MoveTo(20.0F, 100.0F);
/// path.CubicTo(60.0F, 20.0F, 140.0F, 180.0F, 180.0F, 100.0F);
/// painter.SetPen(sdl_painter::Pen(sdl_painter::Color::White(), 3.0F));
/// painter.DrawPath(path);
/// @endcode
class SDLPAINTER_API Path {
 public:
  /// @brief Varsayılan toleransla (@ref kDefaultFlatness) boş yol.
  Path() = default;

  /// @brief Düzleştirme toleransını belirterek boş yol oluştur.
  ///
  /// @param flatness Azami sapma (piksel). Pozitif değilse
  ///        @ref kDefaultFlatness kullanılır.
  explicit Path(float flatness);

  /// @brief Yeni bir alt yol başlat ve imleci oraya taşı.
  ///
  /// Yürürlükteki alt yol tek noktadan ibaretse (hiç çizgi eklenmemişse)
  /// atılır — boş parça üretilmez.
  void MoveTo(float x, float y);

  /// @copydoc MoveTo(float,float)
  void MoveTo(const Point& point);

  /// @brief İmleçten verilen noktaya doğru parçası ekle.
  ///
  /// Yol boşsa önce `(x, y)` noktasında örtük bir @ref MoveTo yapılır, yani
  /// çizgi eklenmez; ilk çağrı yalnızca başlangıcı belirler.
  void LineTo(float x, float y);

  /// @copydoc LineTo(float,float)
  void LineTo(const Point& point);

  /// @brief İkinci dereceden (quadratic) Bézier eğrisi ekle.
  ///
  /// @param cx Kontrol noktasının x'i.
  /// @param cy Kontrol noktasının y'si.
  /// @param x  Bitiş noktasının x'i.
  /// @param y  Bitiş noktasının y'si.
  void QuadTo(float cx, float cy, float x, float y);

  /// @copydoc QuadTo(float,float,float,float)
  void QuadTo(const Point& control, const Point& end);

  /// @brief Üçüncü dereceden (cubic) Bézier eğrisi ekle.
  ///
  /// Font kavisleri ve vektör çizim programlarının kullandığı eğri tipi.
  ///
  /// @param c1x Birinci kontrol noktasının x'i.
  /// @param c1y Birinci kontrol noktasının y'si.
  /// @param c2x İkinci kontrol noktasının x'i.
  /// @param c2y İkinci kontrol noktasının y'si.
  /// @param x   Bitiş noktasının x'i.
  /// @param y   Bitiş noktasının y'si.
  void CubicTo(float c1x, float c1y, float c2x, float c2y, float x, float y);

  /// @copydoc CubicTo(float,float,float,float,float,float)
  void CubicTo(const Point& control1, const Point& control2, const Point& end);

  /// @brief Yürürlükteki alt yolu kapat.
  ///
  /// Kapanış doğrusu nokta olarak eklenmez; parça yalnızca kapalı
  /// işaretlenir ve çizim aşamasında son nokta ilkine bağlanır. Böylece
  /// kapanış köşesi de birleşim (join) alır.
  ///
  /// Kapatmadan sonra imleç alt yolun başlangıç noktasına döner
  /// (QPainter ve SVG davranışı). Sonraki @ref LineTo oradan devam eder ve
  /// yeni bir alt yol başlatır — kapalı bir parçaya ekleme yapılamaz.
  void Close();

  /// @brief Tüm alt yolları sil; tolerans korunur.
  void Clear() noexcept;

  /// @brief Çizilecek hiçbir parça yok mu?
  ///
  /// Tek noktadan ibaret parçalar çizim üretmez ve burada da yok sayılır.
  [[nodiscard]] bool IsEmpty() const noexcept;

  /// @brief Yürürlükteki imleç konumu (yol boşsa orijin).
  [[nodiscard]] Point CurrentPoint() const noexcept { return mCurrent; }

  /// @brief Düzleştirilmiş alt yollar.
  [[nodiscard]] const std::vector<SubPath>& SubPaths() const noexcept {
    return mSubPaths;
  }

  /// @brief Yürürlükteki düzleştirme toleransı (piksel).
  [[nodiscard]] float Flatness() const noexcept { return mFlatness; }

  /// @brief Bu yolun toplam nokta sayısı (tanı ve test için).
  [[nodiscard]] std::size_t PointCount() const noexcept;

 private:
  /// @brief Yürürlükteki alt yola nokta ekle; yoksa oluştur.
  void AppendPoint(const Point& point);

  /// @brief Son alt yol tek noktadan ibaretse onu at.
  ///
  /// Ardışık @ref MoveTo çağrıları veya hemen ardından @ref Clear gelmesi
  /// hiçbir şey çizmeyen parçalar bırakabilir; bunlar burada elenir ki
  /// @ref SubPaths tüketicisi bu durumu ayrıca kontrol etmek zorunda kalmasın.
  void DropDegenerateSubPath();

  /// @brief Eğri eklemeden önce bir başlangıç noktası olduğundan emin ol.
  ///
  /// İmleçsiz bir yola doğrudan @ref QuadTo / @ref CubicTo çağrılırsa,
  /// eğrinin başlangıcı olarak orijin yerine imlecin son değeri kullanılır ve
  /// alt yol orada başlatılır.
  void EnsureStarted();

  /// @brief Bir Bézier'i kaç doğru parçasına böleceğini hesapla.
  ///
  /// İkinci türevin üst sınırından türetilir: düzgün örneklenmiş `n` parçalı
  /// bir eğride sapma `max|B''| / (8 n²)` ile sınırlıdır, dolayısıyla
  /// `n = ceil(sqrt(max|B''| / (8 * flatness)))` toleransı garantiler.
  ///
  /// @param second_derivative_bound `max|B''(t)|` için bir üst sınır.
  /// @return [1, @ref kMaxCurveSegments] aralığında parça sayısı.
  [[nodiscard]] int32_t SegmentsForCurve(float second_derivative_bound) const;

  /// @brief Bir eğrinin bölünebileceği azami parça sayısı.
  ///
  /// Tessellator'daki daire segment sınırıyla aynı gerekçe: bu sayının
  /// üstünde parça başına piksel altı uzunluğa inilir ve tek çağrıda üretilen
  /// vertex sayısı denetimden çıkar.
  static constexpr int32_t kMaxCurveSegments = 256;

  std::vector<SubPath> mSubPaths;

  /// İmleç — bir sonraki parçanın başlangıcı.
  Point mCurrent;

  /// Yürürlükteki alt yolun başlangıcı (@ref Close imleci buraya döndürür).
  Point mSubPathStart;

  /// @ref MoveTo çağrıldı ama henüz nokta üretilmedi mi?
  ///
  /// Ardışık @ref MoveTo çağrılarının boş parça bırakmaması için imleç
  /// "bekleyen" tutulur; ilk gerçek çizim komutunda alt yol açılır.
  bool mPendingStart{false};

  float mFlatness{kDefaultFlatness};
};

}  // namespace sdl_painter
