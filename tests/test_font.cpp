/// @file test_font.cpp
/// @brief Font yaşam döngüsü ve glyph önbelleği testleri.
///
/// Gerçek bir TTF dosyası gerektirir; sistemde font yoksa testler atlanır.

#include "sdl_painter/font.h"

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <utility>

#include "mock_renderer.h"
#include "test_support.h"

using sdl_painter::Font;
using sdl_painter::Glyph;
using sdl_painter::MockRenderer;

namespace {

/// @brief Font yolu yoksa testi atlar; varsa yolu döndürür.
#define REQUIRE_FONT(var)                                         \
  const std::string var = sdl_painter::testing::FindSystemFont(); \
  if ((var).empty()) {                                            \
    GTEST_SKIP() << "Sistemde TTF font bulunamadı.";              \
  }                                                               \
  static_assert(true, "")

}  // namespace

// ─── Temel yükleme ──────────────────────────────────────────────────────────

TEST(FontBasics, LoadsValidFont) {
  REQUIRE_FONT(path);
  Font f(path, 24);
  EXPECT_TRUE(f.IsValid());
  EXPECT_EQ(f.PointSize(), 24);
  EXPECT_GT(f.Ascent(), 0);
}

TEST(FontBasics, InvalidPathYieldsInvalidFont) {
  Font f("bu_dosya_kesinlikle_yok_12345.ttf", 24);
  EXPECT_FALSE(f.IsValid());
  EXPECT_EQ(f.Handle(), nullptr);
}

TEST(FontBasics, MeasureTextReturnsPositiveSize) {
  REQUIRE_FONT(path);
  Font f(path, 24);
  ASSERT_TRUE(f.IsValid());
  int32_t w = 0;
  int32_t h = 0;
  EXPECT_TRUE(f.MeasureText("Merhaba", w, h));
  EXPECT_GT(w, 0);
  EXPECT_GT(h, 0);
}

// ─── Glyph önbelleği ────────────────────────────────────────────────────────

TEST(FontGlyphCache, SameCodepointIsCachedNotReuploaded) {
  REQUIRE_FONT(path);
  MockRenderer r;
  Font f(path, 24);
  ASSERT_TRUE(f.IsValid());

  const Glyph* g1 = f.GetGlyph(r, U'A');
  ASSERT_NE(g1, nullptr);
  EXPECT_EQ(r.update_texture_count, 1);

  const Glyph* g2 = f.GetGlyph(r, U'A');
  ASSERT_NE(g2, nullptr);
  EXPECT_EQ(r.update_texture_count, 1)
      << "Aynı karakter yeniden atlasa yüklenmemeli.";
  EXPECT_EQ(g1, g2) << "Önbellek aynı Glyph'i döndürmeli.";
}

TEST(FontGlyphCache, GlyphsShareASingleAtlasTexture) {
  REQUIRE_FONT(path);
  MockRenderer r;
  Font f(path, 24);
  ASSERT_TRUE(f.IsValid());

  const Glyph* a = f.GetGlyph(r, U'A');
  const Glyph* b = f.GetGlyph(r, U'B');
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);

  // Atlas: iki glyph icin TEK texture olusturulur, iki sub-image yuklenir.
  EXPECT_EQ(r.create_texture_count, 1)
      << "Her glyph icin ayri texture olusturuldu (atlas devrede degil).";
  EXPECT_EQ(r.update_texture_count, 2);
  EXPECT_EQ(a->texture, b->texture)
      << "Ayni fontun glyph'leri ayni atlas sayfasinda olmali.";
  EXPECT_EQ(f.AtlasPageCount(), 1u);
}

TEST(FontGlyphCache, GlyphsGetDistinctAtlasRegions) {
  REQUIRE_FONT(path);
  MockRenderer r;
  Font f(path, 24);
  ASSERT_TRUE(f.IsValid());

  const Glyph* a = f.GetGlyph(r, U'A');
  const Glyph* b = f.GetGlyph(r, U'B');
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  // Bolgeler cakismamali: en azindan u0'lar farkli olmali (ayni rafta yan yana)
  EXPECT_NE(a->u0, b->u0);
  // UV'ler [0,1] araliginda ve tutarli olmali.
  EXPECT_GE(a->u0, 0.0f);
  EXPECT_LE(a->u1, 1.0f);
  EXPECT_LT(a->u0, a->u1);
  EXPECT_LT(a->v0, a->v1);
}

TEST(FontGlyphCache, DestructorReleasesAtlasTextures) {
  REQUIRE_FONT(path);
  MockRenderer r;
  {
    Font f(path, 24);
    ASSERT_TRUE(f.IsValid());
    ASSERT_NE(f.GetGlyph(r, U'A'), nullptr);
    ASSERT_NE(f.GetGlyph(r, U'B'), nullptr);
    EXPECT_EQ(r.create_texture_count, 1);
  }
  EXPECT_EQ(r.destroy_texture_count, 1)
      << "Font yıkımında atlas sayfası serbest bırakılmalı.";
}

// ─── Taşıma (move) semantiği ────────────────────────────────────────────────

/// @brief REGRESYON (K2): move ctor glyph önbelleğini taşımıyordu.
///
/// Önbellek taşınmadığında hedef nesne boş başlar ve tüm glyph'ler yeniden
/// GPU'ya yüklenir — sessiz performans kaybı.
TEST(FontMove, MoveConstructorTransfersGlyphCache) {
  REQUIRE_FONT(path);
  MockRenderer r;
  Font src(path, 24);
  ASSERT_TRUE(src.IsValid());
  ASSERT_NE(src.GetGlyph(r, U'A'), nullptr);
  ASSERT_EQ(r.update_texture_count, 1);

  Font dst(std::move(src));
  ASSERT_TRUE(dst.IsValid());

  ASSERT_NE(dst.GetGlyph(r, U'A'), nullptr);
  EXPECT_EQ(r.update_texture_count, 1)
      << "Önbellek taşınmadı: 'A' yeniden GPU'ya yüklendi.";
}

/// @brief REGRESYON (K2): move-assign hedefin ESKİ önbelleğini bırakmıyordu.
///
/// En ciddi belirti: eski fontun texture'ları önbellekte kalıp yeni font
/// adına döndürülüyor → ekranda **yanlış karakterler** çiziliyor.
TEST(FontMove, MoveAssignmentDropsDestinationOldGlyphCache) {
  REQUIRE_FONT(path);
  MockRenderer r;

  Font dst(path, 12);  // küçük punto
  ASSERT_TRUE(dst.IsValid());
  const Glyph* small = dst.GetGlyph(r, U'A');
  ASSERT_NE(small, nullptr);
  const int32_t kSmallH = small->height;
  ASSERT_EQ(r.create_texture_count, 1);  // atlas sayfasi

  Font src(path, 48);  // belirgin biçimde büyük punto
  ASSERT_TRUE(src.IsValid());

  dst = std::move(src);
  ASSERT_TRUE(dst.IsValid());
  EXPECT_EQ(dst.PointSize(), 48);

  // Eski (12pt) önbellek atılmalı; 'A' yeni 48pt fontla yeniden üretilmeli.
  EXPECT_GE(r.destroy_texture_count, 1)
      << "Hedefin eski glyph texture'ları serbest bırakılmadı.";

  const Glyph* big = dst.GetGlyph(r, U'A');
  ASSERT_NE(big, nullptr);
  EXPECT_GT(big->height, kSmallH)
      << "48pt font 12pt glyph'i döndürdü — eski önbellek temizlenmemiş.";
}

/// @brief Move sonrası kaynak nesne güvenle yıkılabilmeli (çift serbest yok).
TEST(FontMove, MovedFromFontIsSafeToDestroy) {
  REQUIRE_FONT(path);
  MockRenderer r;
  Font src(path, 24);
  ASSERT_TRUE(src.IsValid());
  ASSERT_NE(src.GetGlyph(r, U'A'), nullptr);

  {
    Font dst(std::move(src));
    EXPECT_TRUE(dst.IsValid());
    EXPECT_FALSE(src.IsValid());  // NOLINT(bugprone-use-after-move)
  }
  // src burada yıkılır — taşınmış nesne üzerinde ikinci bir serbest bırakma
  // olmamalı. Toplam destroy sayısı oluşturulan texture sayısını aşmamalı.
  EXPECT_LE(r.destroy_texture_count, r.create_texture_count);
}
