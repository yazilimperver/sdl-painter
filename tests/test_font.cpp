/// @file test_font.cpp
/// @brief Font yaşam döngüsü ve glyph önbelleği testleri.
///
/// Gerçek bir TTF dosyasi gerektirir; sistemde font yoksa testler atlanir.

#include "sdl_painter/font.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <utility>

#include "mock_renderer.h"
#include "test_support.h"

using sdl_painter::Font;
using sdl_painter::Glyph;
using sdl_painter::MockRenderer;

// ─── Temel yükleme ──────────────────────────────────────────────────────────

TEST(FontBasics, LoadsValidFont) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(path);
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
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(path);
  Font f(path, 24);
  ASSERT_TRUE(f.IsValid());
  int32_t w = 0;
  int32_t h = 0;
  EXPECT_TRUE(f.MeasureText("Merhaba", w, h));
  EXPECT_GT(w, 0);
  EXPECT_GT(h, 0);
}

// ─── Glyph önbelleği ────────────────────────────────────────────────────────

TEST(FontBasics, MeasuresUtf8Text) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(path);
  Font f(path, 24);
  ASSERT_TRUE(f.IsValid());

  int32_t ascii_width = 0;
  int32_t ascii_height = 0;
  ASSERT_TRUE(f.MeasureText("A", ascii_width, ascii_height));

  int32_t width = 0;
  int32_t height = 0;
  ASSERT_TRUE(f.MeasureText("A\xC3\xA7", width, height));
  EXPECT_GT(width, ascii_width);
  EXPECT_GT(height, 0);
}

TEST(FontBasics, EmptyTextClearsMeasuredSize) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(path);
  Font f(path, 24);
  ASSERT_TRUE(f.IsValid());

  int32_t width = 123;
  int32_t height = 456;
  EXPECT_FALSE(f.MeasureText("", width, height));
  EXPECT_EQ(width, 0);
  EXPECT_EQ(height, 0);
}

TEST(FontBasics, RemainingFontWorksAfterAnotherIsDestroyed) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(path);
  Font remaining(path, 24);
  ASSERT_TRUE(remaining.IsValid());
  {
    Font other(path, 18);
    ASSERT_TRUE(other.IsValid());
  }

  int32_t width = 0;
  int32_t height = 0;
  EXPECT_TRUE(remaining.MeasureText("A", width, height));
  EXPECT_GT(width, 0);
  EXPECT_GT(height, 0);
}

TEST(FontGlyphCache, SameCodepointIsCachedNotReuploaded) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(path);
  MockRenderer r;
  Font f(path, 24);
  ASSERT_TRUE(f.IsValid());

  const Glyph* g1 = f.GetGlyph(r, U'A');
  ASSERT_NE(g1, nullptr);
  EXPECT_EQ(r.update_texture_count, 1);

  const Glyph* g2 = f.GetGlyph(r, U'A');
  ASSERT_NE(g2, nullptr);
  EXPECT_EQ(r.update_texture_count, 1)
      << "Ayni karakter yeniden atlasa yuklenmemeli.";
  EXPECT_EQ(g1, g2) << "Onbellek ayni Glyph'i dondurmeli.";
}

TEST(FontGlyphCache, GlyphsShareASingleAtlasTexture) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(path);
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
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(path);
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
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(path);
  MockRenderer r;
  {
    Font f(path, 24);
    ASSERT_TRUE(f.IsValid());
    ASSERT_NE(f.GetGlyph(r, U'A'), nullptr);
    ASSERT_NE(f.GetGlyph(r, U'B'), nullptr);
    EXPECT_EQ(r.create_texture_count, 1);
  }
  EXPECT_EQ(r.destroy_texture_count, 1)
      << "Font yikiminda atlas sayfasi serbest birakilmali.";
}

// ─── Taşima (move) semantiği ────────────────────────────────────────────────

/// @brief move ctor glyph önbelleğini taşimiyordu.
///
/// Önbellek taşinmadiğinda hedef nesne boş başlar ve tüm glyph'ler yeniden
/// GPU'ya yüklenir: sessiz performans kaybi.
TEST(FontMove, MoveConstructorTransfersGlyphCache) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(path);
  MockRenderer r;
  Font src(path, 24);
  ASSERT_TRUE(src.IsValid());
  ASSERT_NE(src.GetGlyph(r, U'A'), nullptr);
  ASSERT_EQ(r.update_texture_count, 1);

  Font dst(std::move(src));
  ASSERT_TRUE(dst.IsValid());

  ASSERT_NE(dst.GetGlyph(r, U'A'), nullptr);
  EXPECT_EQ(r.update_texture_count, 1)
      << "Onbellek tasinmadi: 'A' yeniden GPU'ya yuklendi.";
}

/// @brief move-assign hedefin eski önbelleğini birakmiyordu.
///
/// En ciddi belirti: eski fontun texture'lari önbellekte kalip yeni font
/// adina döndürülüyor → ekranda yanliş karakterleri çiziliyor.
TEST(FontMove, MoveAssignmentDropsDestinationOldGlyphCache) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(path);
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

  // Eski (12pt) önbellek atilmali; 'A' yeni 48pt fontla yeniden üretilmeli.
  EXPECT_GE(r.destroy_texture_count, 1)
      << "Hedefin eski glyph texture'lari serbest birakilmadi.";

  const Glyph* big = dst.GetGlyph(r, U'A');
  ASSERT_NE(big, nullptr);
  EXPECT_GT(big->height, kSmallH)
      << "48pt font 12pt glyph'i dondurdu: eski onbellek temizlenmemis.";
}

/// @brief Move sonrasi kaynak nesne güvenle yikilabilmeli (çift serbest yok).
TEST(FontMove, MovedFromFontIsSafeToDestroy) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(path);
  MockRenderer r;
  Font src(path, 24);
  ASSERT_TRUE(src.IsValid());
  ASSERT_NE(src.GetGlyph(r, U'A'), nullptr);

  {
    Font dst(std::move(src));
    EXPECT_TRUE(dst.IsValid());
    EXPECT_FALSE(src.IsValid());  // NOLINT(bugprone-use-after-move)
  }
  // src burada yikilir; taşinmiş nesne üzerinde ikinci bir serbest birakma
  // olmamali. Toplam destroy sayisi oluşturulan texture sayisini aşmamali.
  EXPECT_LE(r.destroy_texture_count, r.create_texture_count);
}

// ─── Renderer sahipliği ─────────────────────────────────────────────────────

/// @brief Bir Font tek bir renderer'a aittir.
///
/// Önbellek yalnizca kod noktasina göre tutulduğu için ikinci bir renderer
/// ile yapilan çağri, önbellek isabetinde birinci renderer'in texture
/// handle'ini döndürüyordu. Handle'lar renderer'a yereldir; o handle ikinci
/// renderer'da başka bir texture'a işaret eder ya da hiç etmez.
TEST(FontOwnership, SecondRendererIsRejected) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(path);
  MockRenderer first;
  MockRenderer second;
  Font font(path, 24);
  ASSERT_TRUE(font.IsValid());

  ASSERT_NE(font.GetGlyph(first, U'A'), nullptr);
  EXPECT_GT(first.create_texture_count, 0);

  // Önbellek isabeti: eskiden birincinin glyph'ini döndürüyordu.
  EXPECT_EQ(font.GetGlyph(second, U'A'), nullptr);
  // Önbellekte olmayan bir karakter de yabanci atlasa yazilmamali.
  EXPECT_EQ(font.GetGlyph(second, U'B'), nullptr);
  EXPECT_EQ(second.create_texture_count, 0);
  EXPECT_EQ(second.update_texture_count, 0);
}

/// @brief Sahiplik ilk GetGlyph ile kurulur; ölçüm renderer gerektirmez.
TEST(FontOwnership, MeasurementWorksBeforeAnyRenderer) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(path);
  // Renderer, Font'tan once bildirilir: yikim ters sirada olduğu için aksi
  // halde ~Font, glyph atlasini çoktan yok edilmiş bir renderer üzerinden
  // birakmaya çalişir. Sahiplik yine de burada değil, ilk GetGlyph
  // çağrisinda kurulur; testin ölçtüğü şey o.
  MockRenderer renderer;
  Font font(path, 24);

  int32_t w = 0;
  int32_t h = 0;
  EXPECT_TRUE(font.MeasureText("AV", w, h));
  EXPECT_GT(w, 0);
  EXPECT_EQ(renderer.create_texture_count, 0)
      << "Olcum renderer'a dokunmamali.";

  EXPECT_NE(font.GetGlyph(renderer, U'A'), nullptr);
}
