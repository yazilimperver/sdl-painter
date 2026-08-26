/// @file test_backend_parity.cpp
/// @brief "Aynı kod iki backend'de aynı sonucu üretir" iddiasının sınavı.
///
/// README bu iddiayı açıkça yapıyor ama bugüne kadar **hiçbir yerde
/// doğrulanmıyordu**. Sonucu: Vulkan'da kırpmanın hiç uygulanmadığının fark
/// edilmesi günler almıştı — hiçbir test pikselleri karşılaştırmıyordu.
///
/// @par Neden repoda referans PNG yok
/// Klasik "altın görüntü" testi, beklenen çıktıyı repoya gömer ve her
/// karşılaştırmayı ona yapar. Burada bilinçli olarak farklı bir yol seçildi:
/// aynı sahne **her iki backend'de** çizilir ve sonuçlar birbiriyle
/// karşılaştırılır. Gerekçeler:
///
///   1. Doğrulanmak istenen iddia tam olarak budur — "iki backend aynı sonucu
///      verir". Gömülü bir PNG tek backend'in regresyonunu yakalar, iki
///      backend'in ayrışmasını değil.
///   2. Gömülü PNG sürücüye bağımlıdır: lavapipe, MSVC/NVIDIA ve Mesa aynı
///      sahnede piksel düzeyinde ufak farklar üretir. Böyle bir referans ya
///      sürekli kırılır ya da toleransı o kadar gevşetilir ki hiçbir şey
///      yakalamaz.
///   3. Kaynak paketine ikili dosya eklemez (tarball zaten büyük).
///
/// Bedeli: her iki backend'in **birlikte** aynı yanlışı yapması durumu
/// yakalanmaz. Bu kabul edilmiş bir sınırdır ve buradaki yönelim/renk
/// assert'leri (bkz. `test_render_target.cpp`) o boşluğun bir kısmını kapatır.

#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/image.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/path.h"
#include "sdl_painter/pen.h"
#include "sdl_painter/render_target.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "test_support.h"

namespace {

using sdl_painter::Brush;
using sdl_painter::Color;
using sdl_painter::Image;
using sdl_painter::Painter;
using sdl_painter::Path;
using sdl_painter::Pen;
using sdl_painter::Point;
using sdl_painter::Rect;
using sdl_painter::RendererBackend;
using sdl_painter::RenderTarget;

constexpr int32_t kSceneW = 160;
constexpr int32_t kSceneH = 120;

/// @brief Kanal başına kabul edilen fark.
///
/// Sıfır beklemek gerçekçi değil: iki sürücü aynı üçgeni aynı kurallarla
/// rasterleştirse bile kenar piksellerinde ve enterpolasyonda son bitler
/// ayrışabilir. Eşik ölçülerek belirlendi — gerçek fark bunun çok altında
/// (ayrıntı testin çıktısında raporlanır).
constexpr int32_t kChannelTolerance = 8;

/// @brief Eşiği aşmasına izin verilen piksel oranı.
///
/// **Ölçülen gerçek değer: 0.** Bu sahne, gerçek sürücülerde (Windows/MSVC,
/// NVIDIA GL + Vulkan) iki backend'de **bayt bayt aynı** çıkıyor. Eşiğin
/// sıfırdan büyük olmasının tek sebebi CI'daki yazılım rasterleştiricileri
/// (lavapipe / llvmpipe): onların kenar piksellerinde ufak farklar üretmesi
/// mümkün ve bu testin bir sürücü farkı yüzünden kırmızıya dönmesi
/// istenmiyor.
///
/// Yine de dar tutuldu: bir şekil kaybolursa, yer değiştirirse veya bir
/// karıştırma faktörü ayrışırsa fark bunun kat kat üstüne çıkar — nitekim
/// bu test yazılırken bulunan üç hatanın en küçüğü %4'tü. Ölçülen oran her
/// koşuda basılır, sessizce gevşemez.
constexpr double kMaxDifferingRatio = 0.005;

/// @brief Dosya gerektirmeyen prosedürel doku (RGBA dama tahtası).
Image MakeCheckerImage(int32_t size = 16) {
  std::vector<uint8_t> pixels(static_cast<std::size_t>(size) * size * 4);
  for (int32_t y = 0; y < size; ++y) {
    for (int32_t x = 0; x < size; ++x) {
      const bool on = ((x / 4) + (y / 4)) % 2 == 0;
      auto* p = pixels.data() + ((static_cast<std::size_t>(y) * size) + x) * 4;
      p[0] = on ? 220 : 40;
      p[1] = on ? 90 : 40;
      p[2] = 60;
      p[3] = 255;
    }
  }
  return Image::CreateFromData(pixels.data(), size, size, 4);
}

/// @brief Karşılaştırılacak sahne — **deterministik**, zaman/rastgelelik yok.
///
/// Kasıtlı olarak zengin: dolgu, çerçeve, kesikli kalem, Bézier yolu,
/// gradient, doku, kırpma, transform ve opaklık aynı karede. Bir backend'de
/// çalışıp diğerinde çalışmayan her yol bu sahneden geçiyor.
void DrawScene(Painter& painter, const Image& image) {
  painter.Clear(Color{18, 20, 30, 255});

  // Düz dolgular
  painter.SetPen(Pen::NoPen());
  painter.SetBrush(Brush(Color{60, 120, 200, 255}));
  painter.FillRect(6.0F, 6.0F, 44.0F, 26.0F);
  painter.FillCircle(74.0F, 19.0F, 13.0F);
  painter.FillEllipse(112.0F, 19.0F, 18.0F, 11.0F);

  // Çerçeveler: uç ve birleşim stilleri
  Pen stroke(Color{240, 200, 80, 255}, 3.0F);
  stroke.SetCapStyle(sdl_painter::LineCap::kRound);
  stroke.SetJoinStyle(sdl_painter::LineJoin::kMiter);
  painter.SetPen(stroke);
  painter.DrawRect(6.0F, 38.0F, 44.0F, 22.0F);
  painter.DrawPolyline({{56.0F, 60.0F}, {70.0F, 40.0F}, {84.0F, 60.0F}});

  // Kesikli kalem
  Pen dashed(Color{130, 235, 170, 255}, 2.0F);
  dashed.SetDashPattern({6.0F, 4.0F});
  painter.SetPen(dashed);
  painter.DrawLine(6.0F, 66.0F, 152.0F, 66.0F);

  // Konkav poligon (ear clipping)
  const std::vector<Point> kConcave = {{92.0F, 38.0F},  {126.0F, 38.0F},
                                       {126.0F, 60.0F}, {112.0F, 60.0F},
                                       {112.0F, 48.0F}, {92.0F, 48.0F}};
  painter.SetPen(Pen::NoPen());
  painter.SetBrush(Brush(Color{120, 200, 120, 220}));
  painter.FillPolygon(kConcave);

  // Bézier yolu — dolgu ve çerçeve birlikte
  Path path;
  path.MoveTo(10.0F, 108.0F);
  path.CubicTo(30.0F, 74.0F, 62.0F, 74.0F, 82.0F, 108.0F);
  path.Close();
  painter.SetBrush(Brush(Color{200, 140, 240, 200}));
  painter.FillPath(path);
  painter.SetPen(Pen(Color{240, 220, 255, 255}, 2.0F));
  painter.DrawPath(path);

  // Gradient fırça (vertex renk enterpolasyonu)
  painter.SetPen(Pen::NoPen());
  painter.SetBrush(Brush::LinearGradient({92.0F, 74.0F}, {150.0F, 108.0F},
                                         Color{255, 80, 40, 255},
                                         Color{40, 80, 255, 255}));
  painter.FillRect(92.0F, 74.0F, 58.0F, 34.0F);

  // Doku — üç aşırı yükleme de
  painter.DrawImage(image, 54.0F, 6.0F);
  painter.DrawImage(image, Rect{132.0F, 38.0F, 22.0F, 20.0F});
  painter.DrawImage(image, Rect{0.0F, 0.0F, 8.0F, 8.0F},
                    Rect{132.0F, 6.0F, 14.0F, 14.0F});

  // Kırpma
  painter.SetClipRect(Rect{20.0F, 84.0F, 40.0F, 24.0F});
  painter.SetBrush(Brush(Color{255, 80, 80, 200}));
  painter.FillRect(0.0F, 0.0F, 160.0F, 120.0F);
  painter.ClearClip();

  // Transform yığını + opaklık
  painter.Save();
  painter.SetOpacity(0.5F);
  painter.Translate(80.0F, 60.0F);
  painter.Rotate(30.0F);
  painter.Scale(1.5F, 0.75F);
  painter.SetBrush(Brush(Color{200, 200, 255, 255}));
  painter.FillRect(-12.0F, -8.0F, 24.0F, 16.0F);
  painter.Restore();

  // Karıştırma modu
  painter.SetBlendMode(sdl_painter::BlendMode::kAdditive);
  painter.SetBrush(Brush(Color{90, 60, 20, 255}));
  painter.FillCircle(40.0F, 96.0F, 14.0F);
  painter.SetBlendMode(sdl_painter::BlendMode::kAlpha);
}

/// @brief Sahneyi verilen backend'de bir hedefe çizip pikselleri döndür.
///
/// @param out_pixels Başarıda RGBA8 içerik buraya yazılır.
/// @param out_skip Ortam uygun değilse sebep buraya yazılır ve `false` döner.
bool RenderScene(RendererBackend backend, std::vector<uint8_t>& out_pixels,
                 std::string& out_skip) {
  const sdl_painter::testing::HiddenWindow window(backend, 192, 144);
  if (window.Get() == nullptr) {
    out_skip = std::string("Pencere olusturulamadi: ") + window.Error();
    return false;
  }

  Painter painter(window.Get(), backend);
  if (!painter.IsValid()) {
    out_skip = "Backend baslatilamadi (uygun surucu/ICD yok olabilir).";
    return false;
  }

  const RenderTarget target = painter.CreateRenderTarget(kSceneW, kSceneH);
  if (!target.IsValid()) {
    out_skip = "Cizim hedefi olusturulamadi.";
    return false;
  }

  // Image, Painter'dan ONCE yikilmali (yasam dongusu sozlesmesi); bu yuzden
  // ic kapsamda tutuluyor ve okuma o kapsamin icinde yapiliyor.
  {
    const Image image = MakeCheckerImage();
    if (!image.IsValid()) {
      out_skip = "Prosedurel doku olusturulamadi.";
      return false;
    }

    painter.Begin();
    if (!painter.SetRenderTarget(target)) {
      out_skip = "Hedefe gecilemedi.";
      return false;
    }
    DrawScene(painter, image);
    painter.ResetRenderTarget();
    painter.End();

    if (!painter.ReadRenderTarget(target, out_pixels)) {
      out_skip = "Hedef geri okunamadi.";
      return false;
    }
  }
  return true;
}

/// @brief Fark haritasının çözünürlüğü (kaba ızgara).
constexpr int32_t kMapRows = 24;
constexpr int32_t kMapCols = 40;

/// @brief İki tamponun farkını özetler.
struct DiffReport {
  std::size_t differing_pixels{0};  ///< Toleransı aşan piksel sayısı
  int32_t worst_delta{0};           ///< En büyük kanal farkı
  int32_t worst_x{0};
  int32_t worst_y{0};
  /// Kaba ızgarada hücre başına farklı piksel sayısı.
  std::vector<int32_t> map;
};

/// @brief Fark haritasını okunabilir bir ASCII ızgaraya çevir.
///
/// Bir parite testi düştüğünde "yüzde kaç" tek başına işe yaramaz; farkın
/// sahnenin neresinde toplandığını görmek gerekir. Nokta = fark yok,
/// rakam = o hücredeki farklı piksel yoğunluğu.
std::string RenderMap(const DiffReport& report) {
  std::string out = "\n";
  for (int32_t row = 0; row < kMapRows; ++row) {
    for (int32_t col = 0; col < kMapCols; ++col) {
      const int32_t count =
          report.map[(static_cast<std::size_t>(row) * kMapCols) +
                     static_cast<std::size_t>(col)];
      if (count == 0) {
        out.push_back('.');
      } else if (count >= 9) {
        out.push_back('#');
      } else {
        out.push_back(static_cast<char>('0' + count));
      }
    }
    out.push_back('\n');
  }
  return out;
}

DiffReport Compare(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b,
                   int32_t width, int32_t height) {
  DiffReport report;
  report.map.assign(static_cast<std::size_t>(kMapRows) * kMapCols, 0);
  for (int32_t y = 0; y < height; ++y) {
    for (int32_t x = 0; x < width; ++x) {
      const std::size_t base =
          ((static_cast<std::size_t>(y) * static_cast<std::size_t>(width)) +
           static_cast<std::size_t>(x)) *
          4U;
      int32_t pixel_delta = 0;
      for (std::size_t c = 0; c < 4U; ++c) {
        const auto lhs = static_cast<int32_t>(a[base + c]);
        const auto rhs = static_cast<int32_t>(b[base + c]);
        pixel_delta = std::max(pixel_delta, std::abs(lhs - rhs));
      }
      if (pixel_delta > kChannelTolerance) {
        ++report.differing_pixels;
        const int32_t row = y * kMapRows / height;
        const int32_t col = x * kMapCols / width;
        ++report.map[(static_cast<std::size_t>(row) * kMapCols) +
                     static_cast<std::size_t>(col)];
      }
      if (pixel_delta > report.worst_delta) {
        report.worst_delta = pixel_delta;
        report.worst_x = x;
        report.worst_y = y;
      }
    }
  }
  return report;
}

/// @brief En kotu pikselin cevresindeki degerleri yan yana yaz.
///
/// "Yuzde kac farkli" bir hatayi teshis etmeye yetmez; farkin karakterini
/// (kaymis mi, rengi mi farkli, biri bos mu) ancak degerler gosterir.
std::string Neighbourhood(const std::vector<uint8_t>& a,
                          const std::vector<uint8_t>& b, int32_t cx,
                          int32_t cy) {
  std::ostringstream out;
  out << "\nen buyuk farkin cevresi (GL | VK), merkez (" << cx << ", " << cy
      << "):\n";
  for (int32_t y = cy - 2; y <= cy + 2; ++y) {
    if (y < 0 || y >= kSceneH) {
      continue;
    }
    for (int32_t x = cx - 2; x <= cx + 2; ++x) {
      if (x < 0 || x >= kSceneW) {
        continue;
      }
      const std::size_t i = ((static_cast<std::size_t>(y) * kSceneW) + x) * 4U;
      // Alfa da yazilir: ilk teshiste yalnizca RGB basiliyordu ve degerler
      // ayni gorundugu icin gercek fark (alfa karistirma faktorleri) neredeyse
      // gozden kacacakti.
      out << "(" << static_cast<int32_t>(a[i]) << ","
          << static_cast<int32_t>(a[i + 1]) << ","
          << static_cast<int32_t>(a[i + 2]) << ","
          << static_cast<int32_t>(a[i + 3]) << "|" << static_cast<int32_t>(b[i])
          << "," << static_cast<int32_t>(b[i + 1]) << ","
          << static_cast<int32_t>(b[i + 2]) << ","
          << static_cast<int32_t>(b[i + 3]) << ") ";
    }
    out << "\n";
  }
  return out.str();
}

// ---------------------------------------------------------------------------

#ifdef SDLPAINTER_HAS_VULKAN

TEST(BackendParity, OpenGLAndVulkanProduceTheSameImage) {
  const sdl_painter::testing::ValidationGuard guard;

  std::vector<uint8_t> gl_pixels;
  std::string skip;
  if (!RenderScene(RendererBackend::kOpenGL, gl_pixels, skip)) {
    GTEST_SKIP() << "OpenGL: " << skip;
  }

  std::vector<uint8_t> vk_pixels;
  if (!RenderScene(RendererBackend::kVulkan, vk_pixels, skip)) {
    GTEST_SKIP() << "Vulkan: " << skip;
  }

  ASSERT_EQ(gl_pixels.size(), vk_pixels.size());
  ASSERT_EQ(gl_pixels.size(), static_cast<std::size_t>(kSceneW) * kSceneH * 4U);

  const DiffReport report = Compare(gl_pixels, vk_pixels, kSceneW, kSceneH);
  const auto total = static_cast<double>(kSceneW) * kSceneH;
  const double ratio = static_cast<double>(report.differing_pixels) / total;

  // Olculen degeri her kosuda yaz: esigin gercege ne kadar yakin oldugu
  // gorunur kalsin, sessizce gevsemesin.
  std::cout << "[  PARITE  ] toleransi asan piksel: " << report.differing_pixels
            << "/" << static_cast<int32_t>(total) << " (" << (ratio * 100.0)
            << "%), en buyuk kanal farki " << report.worst_delta << " @ ("
            << report.worst_x << ", " << report.worst_y << ")\n";

  EXPECT_LE(ratio, kMaxDifferingRatio)
      << "OpenGL ve Vulkan ciktilari beklenenden fazla ayrisiyor."
      << Neighbourhood(gl_pixels, vk_pixels, report.worst_x, report.worst_y)
      << RenderMap(report);
}

/// Aynı backend iki kez çalıştırıldığında **birebir** aynı çıktıyı vermeli.
///
/// Parite testinin ön koşulu: çizim kendi içinde deterministik değilse iki
/// backend'i karşılaştırmanın anlamı olmaz. Bu test, bir farkın kaynağının
/// "backend farkı" mı yoksa "kararsız çizim" mi olduğunu ayırt etmeyi sağlar.
TEST(BackendParity, SameBackendIsDeterministic) {
  const sdl_painter::testing::ValidationGuard guard;

  std::vector<uint8_t> first;
  std::string skip;
  if (!RenderScene(RendererBackend::kOpenGL, first, skip)) {
    GTEST_SKIP() << skip;
  }
  std::vector<uint8_t> second;
  if (!RenderScene(RendererBackend::kOpenGL, second, skip)) {
    GTEST_SKIP() << skip;
  }
  EXPECT_EQ(first, second);
}

#else

TEST(BackendParity, SkippedWithoutVulkan) {
  GTEST_SKIP() << "Vulkan backend derlenmedi; karsilastiracak ikinci backend "
                  "yok (-o \"&:with_vulkan=True\" ile derleyin).";
}

#endif  // SDLPAINTER_HAS_VULKAN

}  // namespace
