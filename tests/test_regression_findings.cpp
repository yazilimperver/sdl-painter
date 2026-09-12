/// @file test_regression_findings.cpp
///
/// @brief Kritik görülen bir takım hatalara yönelik refresyon testleri.

#include "sdl_painter/app/app_config.h"
#include "sdl_painter/app/application.h"
#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/font.h"
#include "sdl_painter/frame_stats.h"
#include "sdl_painter/geometry.h"
#include "sdl_painter/image.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/render_target.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "recording_target_renderer.h"
#include "test_support.h"

using sdl_painter::Alignment;
using sdl_painter::AppConfig;
using sdl_painter::Application;
using sdl_painter::Brush;
using sdl_painter::Color;
using sdl_painter::Font;
using sdl_painter::FrameStats;
using sdl_painter::Image;
using sdl_painter::kInvalidRenderTarget;
using sdl_painter::MockRenderer;
using sdl_painter::Painter;
using sdl_painter::Point;
using sdl_painter::Rect;
using sdl_painter::RendererBackend;
using sdl_painter::RenderTarget;
using sdl_painter::TexturedVertex;
using sdl_painter::TextureHandle;
using sdl_painter::Vertex;
using sdl_painter::testing::FramePolicy;
using sdl_painter::testing::HiddenWindow;
using sdl_painter::testing::LifetimeJournal;
using sdl_painter::testing::RecordingTargetRenderer;

namespace {

constexpr int32_t kScreenW = 800;
constexpr int32_t kScreenH = 600;

/// @brief Renderer sahipliğini Painter'a devreder, ham işaretçiyi saklar.
struct TargetHarness {
  RecordingTargetRenderer* mock{nullptr};
  Painter painter;

  TargetHarness()
      : TargetHarness(std::make_unique<RecordingTargetRenderer>()) {}

  explicit TargetHarness(std::unique_ptr<RecordingTargetRenderer> r)
      : mock(r.get()), painter(std::move(r), kScreenW, kScreenH) {}
};

/// @brief Bir noktanın rengini üçgen listesinden baricentrik enterpolasyonla
///        oku — rasterizer'ın yapacağı işi bağımsız olarak yapar.
///
/// Gradient testlerinde kullanacağız.
struct SampledColor {
  bool found{false};
  float r{0.0F};
  float g{0.0F};
  float b{0.0F};
  float a{0.0F};
};

SampledColor SampleTriangleList(const std::vector<Vertex>& verts, float px,
                                float py) {
  for (std::size_t i = 0; i + 2 < verts.size(); i += 3) {
    const Vertex& v0 = verts[i];
    const Vertex& v1 = verts[i + 1];
    const Vertex& v2 = verts[i + 2];

    const float kDen =
        (v1.y - v2.y) * (v0.x - v2.x) + (v2.x - v1.x) * (v0.y - v2.y);
    if (std::fabs(kDen) < 1e-6F) {
      continue;  // Dejenere üçgen.
    }
    const float kW0 =
        ((v1.y - v2.y) * (px - v2.x) + (v2.x - v1.x) * (py - v2.y)) / kDen;
    const float kW1 =
        ((v2.y - v0.y) * (px - v2.x) + (v0.x - v2.x) * (py - v2.y)) / kDen;
    const float kW2 = 1.0F - kW0 - kW1;

    // Kenar üzerindeki noktalar için küçük tolerans.
    constexpr float kEps = -1e-4F;
    if (kW0 < kEps || kW1 < kEps || kW2 < kEps) {
      continue;
    }

    SampledColor out;
    out.found = true;
    out.r = kW0 * static_cast<float>(v0.r) + kW1 * static_cast<float>(v1.r) +
            kW2 * static_cast<float>(v2.r);
    out.g = kW0 * static_cast<float>(v0.g) + kW1 * static_cast<float>(v1.g) +
            kW2 * static_cast<float>(v2.g);
    out.b = kW0 * static_cast<float>(v0.b) + kW1 * static_cast<float>(v1.b) +
            kW2 * static_cast<float>(v2.b);
    out.a = kW0 * static_cast<float>(v0.a) + kW1 * static_cast<float>(v1.a) +
            kW2 * static_cast<float>(v2.a);
    return out;
  }
  return {};
}

}  // namespace

// hata 1, move ataması fontu renderer'dan sonraya bırakıyor
//
// `Painter::operator=` önce `mRenderer = std::move(other.mRenderer)` ile eski
// renderer'ı yok ediyor, `mCurrentFont`'u ise daha sonra bırakıyor. Painter fontun son sahibiyse, font atlasındaki
// Texture destrcutor'ları artık yok edilmiş renderer üzerinden `DestroyTexture`
// çağırır
TEST(PainterLifecycleDeathTest, MoveAssignmentReleasesOwnedFontSafely) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(font_path);
  GTEST_FLAG_SET(death_test_style, "threadsafe");

  EXPECT_EXIT(
      {
        std::setvbuf(stderr, nullptr, _IONBF, 0);

        LifetimeJournal journal;

        auto dst_renderer = std::make_unique<RecordingTargetRenderer>();
        dst_renderer->SetJournal(&journal);
        Painter dst(std::move(dst_renderer), 128, 96);
        Painter src(std::make_unique<RecordingTargetRenderer>(), 128, 96);

        auto font = std::make_shared<Font>(font_path, 24);
        if (!font->IsValid()) {
          std::fprintf(stderr, "onkosul: font yuklenemedi\n");
          std::exit(3);
        }
        dst.SetFont(font);
        dst.Begin();
        dst.DrawText(4.0F, 40.0F, "A");
        dst.End();

        if (font->AtlasPageCount() == 0U || journal.textures_created == 0) {
          std::fprintf(stderr, "onkosul: atlas olusmadi (pages=%zu, tex=%d)\n",
                       font->AtlasPageCount(), journal.textures_created);
          std::exit(3);
        }

        font.reset();  // Son güçlü sahip artık dst.

        // Bu satır çıktıda görünüp bir sonraki görünmüyorsa, use-after-free
        // move atamasının ortasında sürecin ölmesine yol açmıştır.
        std::fprintf(stderr, "[move oncesi] atlas texture sayisi=%d\n",
                     journal.textures_created);

        dst = std::move(src);  //renderer fonttan ÖNCE yok ediliyor.

        std::fprintf(stderr,
                     "[move sonrasi] destroy_after_death=%d "
                     "live_textures_at_death=%d\n",
                     journal.destroy_after_death,
                     journal.live_textures_at_death);

        const bool kClean = journal.destroy_after_death == 0 &&
                            journal.live_textures_at_death == 0;
        std::exit(kClean ? 0 : 2);
      },
      ::testing::ExitedWithCode(0), "");
}

// Hata 2 — Frame sınırında aktif hedef sözleşmesi uygulanmıyo
TEST(PainterTargets, BeginStartsOnScreen) {
  TargetHarness h;  // kKeepTarget: OpenGL'de FBO bağlı kalır.
  RenderTarget a = h.painter.CreateRenderTarget(32, 32);
  ASSERT_TRUE(a.IsValid());

  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.Begin();
  ASSERT_TRUE(h.painter.SetRenderTarget(a));
  h.painter.FillRect(0.0F, 0.0F, 32.0F, 32.0F);
  h.painter.End();  // Kasıtlı: ResetRenderTarget YOK.

  h.mock->draws.clear();
  h.painter.SetBrush(Brush(Color::Blue()));
  h.painter.Begin();
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.End();

  ASSERT_EQ(h.mock->draws.size(), 1U);
  EXPECT_EQ(h.mock->draws[0].target, kInvalidRenderTarget)
      << "BULGU 2: yeni kare ekranda baslamaliydi; cizim hala onceki karenin "
         "hedefine gitti.";
  EXPECT_EQ(h.mock->draws[0].viewport.w, kScreenW)
      << "BULGU 2: viewport da hedefinkinde kaldi.";
  EXPECT_EQ(h.mock->draws[0].viewport.h, kScreenH);
}

TEST(PainterTargets, SameTargetCanBeSelectedNextFrame) {
  auto renderer = std::make_unique<RecordingTargetRenderer>();
  renderer->frame_policy = FramePolicy::kResetToScreen;  // Vulkan benzeri.
  renderer->backend = RendererBackend::kVulkan;
  TargetHarness h(std::move(renderer));

  RenderTarget a = h.painter.CreateRenderTarget(32, 32);
  ASSERT_TRUE(a.IsValid());

  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.Begin();
  ASSERT_TRUE(h.painter.SetRenderTarget(a));
  h.painter.FillRect(0.0F, 0.0F, 32.0F, 32.0F);
  h.painter.End();  // Backend kare sınırında ekrana döndü.

  h.mock->draws.clear();
  h.painter.SetBrush(Brush(Color::Blue()));
  h.painter.Begin();
  ASSERT_TRUE(h.painter.SetRenderTarget(a));
  h.painter.FillRect(0.0F, 0.0F, 32.0F, 32.0F);
  h.painter.End();

  ASSERT_EQ(h.mock->draws.size(), 1U);
  EXPECT_EQ(h.mock->draws[0].target, a.Handle())
      << "BULGU 2: Painter ayni handle'i gorup erken dondu; backend'e gecis "
         "cagrisi hic gitmedi, cizim ekrana dustu.";
}

// Hata 3  Move işlemleri çizim durumunun tamamını taşımıyor
//
// Move constructor/assignment `mActiveTarget`, hedef boyutları, saklanan
// viewport, `mAutoDrawableSize` ve kare istatistiklerini taşımıyor
TEST(PainterLifecycle, MovePreservesTargetState) {
  auto renderer = std::make_unique<RecordingTargetRenderer>();
  auto* raw = renderer.get();
  Painter src(std::move(renderer), kScreenW, kScreenH);

  src.SetViewport(20, 30, 200, 100);
  RenderTarget a = src.CreateRenderTarget(128, 128);
  ASSERT_TRUE(a.IsValid());
  ASSERT_TRUE(src.SetRenderTarget(a));

  Painter dst(std::move(src));

  raw->viewport_calls.clear();
  dst.ResetRenderTarget();

  ASSERT_FALSE(raw->viewport_calls.empty())
      << "BULGU 5: mActiveTarget tasinmadigi icin ResetRenderTarget etkisiz "
         "kaldi - ekrana hic donulmedi.";
  const auto& v = raw->viewport_calls.back();
  EXPECT_EQ(v.x, 20) << "BULGU 5: saklanan viewport tasinmadi.";
  EXPECT_EQ(v.y, 470) << "BULGU 5: GL ekran viewport'u 600-30-100 olmaliydi.";
  EXPECT_EQ(v.w, 200);
  EXPECT_EQ(v.h, 100);

  // dst, a'dan sonra bildirildigi icin once yok olurdu; hedefi renderer
  // yasarken elle birakiyoruz.
  a.Reset();
}

TEST(PainterLifecycle, MovePreservesSelfSampleGuard) {
  auto renderer = std::make_unique<RecordingTargetRenderer>();
  auto* raw = renderer.get();
  Painter src(std::move(renderer), kScreenW, kScreenH);

  RenderTarget a = src.CreateRenderTarget(128, 128);
  ASSERT_TRUE(a.IsValid());
  ASSERT_TRUE(src.SetRenderTarget(a));

  Painter dst(std::move(src));

  raw->draws.clear();
  dst.Begin();
  dst.DrawRenderTarget(a, 0.0F, 0.0F);  // Aktif hedefin kendisi.
  dst.End();

  EXPECT_TRUE(raw->draws.empty())
      << "BULGU 5: mActiveTarget tasinmadigi icin 'hedef kendi icerigini "
         "ornekleyemez' korumasi devreye girmedi.";

  // dst, a'dan sonra bildirildigi icin once yok olurdu; hedefi renderer
  // yasarken elle birakiyoruz.
  a.Reset();
}

TEST(PainterLifecycle, MovePreservesLastFrameStats) {
  Painter src(std::make_unique<RecordingTargetRenderer>(), kScreenW, kScreenH);

  src.SetBrush(Brush(Color::Red()));
  src.Begin();
  src.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  src.End();

  const FrameStats kBefore = src.GetFrameStats();
  ASSERT_GT(kBefore.draw_calls, 0U);
  ASSERT_GT(kBefore.vertices, 0U);

  Painter dst(std::move(src));
  const FrameStats kAfter = dst.GetFrameStats();

  EXPECT_EQ(kAfter.draw_calls, kBefore.draw_calls)
      << "BULGU 5: tamamlanmis karenin istatistikleri tasinmadi.";
  EXPECT_EQ(kAfter.vertices, kBefore.vertices)
      << "BULGU 5: tamamlanmis karenin istatistikleri tasinmadi.";
}

// Hata 4 Clip, koordinat sistemi değişimini izlemiyor
//
// Scissor kutusu viewport ofsetine, yüzey yüksekliğine ve Y yönüne bağlı
// hesaplanıyor (`ApplyScissor`), ama `ApplyViewport` yalnızca viewport ve
// projeksiyonu güncelliyor. Etkin bir clip; viewport
// değişiminde, hedefe geçişte ve yeniden boyutlandırmada yeniden hesaplanmıyor.

TEST(PainterClip, FollowsViewportChange) {
  TargetHarness h;
  h.painter.SetBrush(Brush(Color::Red()));

  h.painter.Begin();
  h.painter.SetClipRect(Rect{10.0F, 20.0F, 50.0F, 40.0F});
  h.painter.SetViewport(100, 50, 200, 150);
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.End();

  ASSERT_EQ(h.mock->draws.size(), 1U);
  const auto& d = h.mock->draws[0];
  ASSERT_TRUE(d.has_scissor);
  EXPECT_EQ(d.scissor.x, 110)
      << "BULGU 6: clip yeni viewport ofsetine tasinmadi (100+10).";
  EXPECT_EQ(d.scissor.y, 490)
      << "BULGU 6: GL scissor Y'si 600-(50+20)-40 olmaliydi.";
  EXPECT_EQ(d.scissor.w, 50);
  EXPECT_EQ(d.scissor.h, 40);
}

TEST(PainterClip, FollowsTargetChange) {
  TargetHarness h;
  h.painter.SetBrush(Brush(Color::Red()));
  RenderTarget a = h.painter.CreateRenderTarget(128, 128);
  ASSERT_TRUE(a.IsValid());

  h.painter.Begin();
  h.painter.SetClipRect(Rect{10.0F, 20.0F, 50.0F, 40.0F});
  ASSERT_TRUE(h.painter.SetRenderTarget(a));
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.ResetRenderTarget();  // Çizimi hedef aktifken flush eder.
  h.painter.End();

  ASSERT_EQ(h.mock->draws.size(), 1U);
  const auto& d = h.mock->draws[0];
  ASSERT_EQ(d.target, a.Handle());
  ASSERT_TRUE(d.has_scissor);
  EXPECT_EQ(d.scissor.x, 10);
  EXPECT_EQ(d.scissor.y, 20)
      << "BULGU 6: hedefe cizerken Y ekseni GPU ile ayni yonde; scissor "
         "cevrilmemeliydi.";
  EXPECT_EQ(d.scissor.w, 50);
  EXPECT_EQ(d.scissor.h, 40);
}

TEST(PainterClip, FollowsResize) {
  TargetHarness h;
  h.painter.SetBrush(Brush(Color::Red()));

  h.painter.Begin();
  h.painter.SetClipRect(Rect{10.0F, 20.0F, 50.0F, 40.0F});
  h.painter.SetDrawableSize(kScreenW, 700);
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.End();

  ASSERT_EQ(h.mock->draws.size(), 1U);
  const auto& d = h.mock->draws[0];
  ASSERT_TRUE(d.has_scissor);
  EXPECT_EQ(d.scissor.y, 640)
      << "BULGU 6: yuzey yuksekligi 700 olunca GL scissor Y'si 700-20-40 "
         "olmaliydi.";
}

// Hata 5 — Pencere boyutu ile hedef boyutu birbirine karışıyor
//
// `ResetViewport` aktif hedef olsa bile `mDrawableWidth/Height` kullanıyor
// `ApplyDrawableSize` hedef seçiliyken de viewport'u pencere boyutuna çeviriyor ve biriken çizimleri
// GPU viewport'u değişmeden önce flush etmiyor.
TEST(PainterSurface, ResetViewportUsesTargetExtent) {
  TargetHarness h;
  RenderTarget a = h.painter.CreateRenderTarget(128, 128);
  ASSERT_TRUE(a.IsValid());
  ASSERT_TRUE(h.painter.SetRenderTarget(a));
  h.painter.SetViewport(8, 8, 64, 64);

  h.mock->viewport_calls.clear();
  h.painter.ResetViewport();

  ASSERT_FALSE(h.mock->viewport_calls.empty());
  const auto& v = h.mock->viewport_calls.back();
  EXPECT_EQ(v.x, 0);
  EXPECT_EQ(v.y, 0);
  EXPECT_EQ(v.w, 128)
      << "BULGU 7: hedef bagliyken ResetViewport pencere genisligini kullandi.";
  EXPECT_EQ(v.h, 128)
      << "BULGU 7: hedef bagliyken ResetViewport pencere yuksekligini "
         "kullandi.";
}

TEST(PainterSurface, ResizeDoesNotResizeActiveTarget) {
  TargetHarness h;
  RenderTarget a = h.painter.CreateRenderTarget(128, 128);
  ASSERT_TRUE(a.IsValid());
  ASSERT_TRUE(h.painter.SetRenderTarget(a));

  h.mock->viewport_calls.clear();
  h.painter.SetDrawableSize(1024, 768);

  EXPECT_TRUE(h.mock->viewport_calls.empty())
      << "BULGU 7: hedef bagliyken pencere yeniden boyutlandirildi ve hedefin "
         "viewport'u pencere boyutuna cevrildi.";

  h.mock->viewport_calls.clear();
  h.painter.ResetRenderTarget();

  ASSERT_FALSE(h.mock->viewport_calls.empty());
  const auto& v = h.mock->viewport_calls.back();
  EXPECT_EQ(v.w, 1024)
      << "BULGU 7: ekrana donerken yeniden boyutlandirma oncesi viewport geri "
         "yuklendi.";
  EXPECT_EQ(v.h, 768);
}

TEST(PainterSurface, ResizeFlushesUsingPreviousViewport) {
  TargetHarness h;
  h.painter.SetBrush(Brush(Color::Red()));

  h.painter.Begin();
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);  // Eski yüzeye ait.
  h.painter.SetDrawableSize(1024, 768);
  h.painter.FillRect(20.0F, 20.0F, 10.0F, 10.0F);  // Yeni yüzeye ait.
  h.painter.End();

  ASSERT_EQ(h.mock->draws.size(), 2U);
  EXPECT_EQ(h.mock->draws[0].viewport.w, kScreenW)
      << "BULGU 7: yeniden boyutlandirmadan once biriken cizim, YENI viewport "
         "ile gonderildi.";
  EXPECT_EQ(h.mock->draws[0].viewport.h, kScreenH);
  EXPECT_EQ(h.mock->draws[1].viewport.w, 1024);
  EXPECT_EQ(h.mock->draws[1].viewport.h, 768);
}

// hata 6 Resource handle'ları renderer kimliği olmadan kullanılıyor
//
// Painter hedefi yalnızca sayısal handle ile kullanıyor. İki renderer'ın ilk hedefi aynı handle'ı
// alabilir; A'ya ait hedef B'ye verilince B sessizce kendi hedefini seçer.
namespace {

/// @brief İki bağımsız Painter; hedefleri kasıtlı olarak aynı handle'ı alır.
struct TwoPainters {
  RecordingTargetRenderer* raw_a{nullptr};
  RecordingTargetRenderer* raw_b{nullptr};
  Painter a;
  Painter b;
  RenderTarget target_a;
  RenderTarget target_b;

  TwoPainters()
      : TwoPainters(std::make_unique<RecordingTargetRenderer>(),
                    std::make_unique<RecordingTargetRenderer>()) {}

  TwoPainters(std::unique_ptr<RecordingTargetRenderer> ra,
              std::unique_ptr<RecordingTargetRenderer> rb)
      : raw_a(ra.get()),
        raw_b(rb.get()),
        a(std::move(ra), kScreenW, kScreenH),
        b(std::move(rb), kScreenW, kScreenH) {
    target_a = a.CreateRenderTarget(64, 64);
    target_b = b.CreateRenderTarget(32, 32);
  }
};

}  // namespace

TEST(PainterOwnership, ForeignTargetIsRejectedBySetRenderTarget) {
  TwoPainters p;
  ASSERT_TRUE(p.target_a.IsValid());
  ASSERT_TRUE(p.target_b.IsValid());
  ASSERT_EQ(p.target_a.Handle(), p.target_b.Handle())
      << "Onkosul: iki bagimsiz renderer ilk hedefe ayni handle'i vermeli; "
         "aksi halde cakisma senaryosu kurulmamis olur.";

  p.raw_b->set_target_calls.clear();
  EXPECT_FALSE(p.b.SetRenderTarget(p.target_a))
      << "BULGU 9: yabanci renderer'a ait hedef kabul edildi.";
  EXPECT_TRUE(p.raw_b->set_target_calls.empty())
      << "BULGU 9: B, yabanci handle ile kendi hedefini secti.";
}

TEST(PainterOwnership, ForeignTargetIsRejectedByReadRenderTarget) {
  TwoPainters p;
  ASSERT_TRUE(p.target_a.IsValid());
  ASSERT_TRUE(p.target_b.IsValid());
  ASSERT_EQ(p.target_a.Handle(), p.target_b.Handle());

  p.raw_b->read_target_calls.clear();
  std::vector<uint8_t> pixels;
  EXPECT_FALSE(p.b.ReadRenderTarget(p.target_a, pixels))
      << "BULGU 9: yabanci hedeften okuma kabul edildi.";
  EXPECT_TRUE(p.raw_b->read_target_calls.empty())
      << "BULGU 9: okuma istegi B renderer'ina gitti ve B kendi hedefini "
         "okudu.";
}

TEST(PainterOwnership, ForeignTargetIsRejectedByDrawRenderTarget) {
  TwoPainters p;
  ASSERT_TRUE(p.target_a.IsValid());
  ASSERT_TRUE(p.target_b.IsValid());
  ASSERT_EQ(p.target_a.Handle(), p.target_b.Handle());

  p.raw_b->draws.clear();
  p.b.Begin();
  p.b.DrawRenderTarget(p.target_a, 0.0F, 0.0F);
  p.b.End();

  EXPECT_TRUE(p.raw_b->draws.empty())
      << "BULGU 9: yabanci hedef cizildi; B kendi hedefinin texture'ini "
         "ornekledi.";
}

// hata 7  Vertex gradient'i şeklin içindeki geçişi temsil etmiyor
//
// Gradient renkleri tessellation sonrası vertex'lere yazılıyor. `FillRect` yalnızca dört köşeden iki üçgen üretiyor
//, dolayısıyla gradient'in şekil içindeki davranışı
// köşe renklerinin doğrusal karışımına indirgeniyor.
TEST(PainterGradient, RadialRectHasCenterColor) {
  TargetHarness h;
  // Sözleşme (brush.h): "Renk merkezde from, radius uzaklığında to olur."
  h.painter.SetBrush(Brush::RadialGradient(Point{50.0F, 50.0F}, 50.0F,
                                           Color::Red(), Color::Blue()));

  h.painter.Begin();
  h.painter.FillRect(0.0F, 0.0F, 100.0F, 100.0F);
  h.painter.End();

  ASSERT_EQ(h.mock->draws.size(), 1U);
  const SampledColor kCenter =
      SampleTriangleList(h.mock->draws[0].vertices, 50.0F, 50.0F);
  ASSERT_TRUE(kCenter.found) << "Ornekleme noktasi hicbir ucgene dusmedi.";

  // merkezde t = 0 → tamamen `from` (kırmızı).
  EXPECT_GT(kCenter.r, 245.0F)
      << "BULGU 11: gradient merkezi kirmizi olmaliydi; dort kose de yaricap "
         "disinda kaldigi icin seklin tamami `to` rengine dustu.";
  EXPECT_LT(kCenter.b, 10.0F)
      << "BULGU 11: gradient merkezinde mavi bilesen bulunmamaliydi.";

  // yarıçap dışındaki bir iç nokta gerçekten mavi olmalı.
  const SampledColor kCorner =
      SampleTriangleList(h.mock->draws[0].vertices, 1.0F, 1.0F);
  ASSERT_TRUE(kCorner.found);
  EXPECT_GT(kCorner.b, 245.0F);
}

TEST(PainterGradient, LinearRectClampsOutsideSegment) {
  TargetHarness h;
  // Gradient ekseni şeklin içinde bitiyor: 0–25 arası sabit `from`,
  // 75–100 arası sabit `to` olmalı.
  h.painter.SetBrush(Brush::LinearGradient(
      Point{25.0F, 0.0F}, Point{75.0F, 0.0F}, Color::Red(), Color::Blue()));

  h.painter.Begin();
  h.painter.FillRect(0.0F, 0.0F, 100.0F, 100.0F);
  h.painter.End();

  ASSERT_EQ(h.mock->draws.size(), 1U);
  const std::vector<Vertex>& verts = h.mock->draws[0].vertices;

  // Bağımsız oracle: t = clamp((x - 25) / 50, 0, 1).
  // x = 10 → t = 0 → kırmızı; x = 90 → t = 1 → mavi.
  const SampledColor kLeft = SampleTriangleList(verts, 10.0F, 50.0F);
  ASSERT_TRUE(kLeft.found);
  EXPECT_GT(kLeft.r, 245.0F)
      << "BULGU 11: gradient baslangicinin solunda renk sabit `from` "
         "olmaliydi; clamp yalnizca koselerde yapildigi icin gecis tum "
         "genislige yayildi.";
  EXPECT_LT(kLeft.b, 10.0F);

  const SampledColor kRight = SampleTriangleList(verts, 90.0F, 50.0F);
  ASSERT_TRUE(kRight.found);
  EXPECT_GT(kRight.b, 245.0F)
      << "BULGU 11: gradient bitisinin saginda renk sabit `to` olmaliydi.";
  EXPECT_LT(kRight.r, 10.0F);
}

// Aktif hedef yok edilirse Painter ekrana geri donmeli.
//
// RenderTarget::Reset renderer'in DestroyRenderTarget'ini cagiriyor; renderer
// kendi aktif hedefini birakiyordu ama Painter haberdar edilmiyordu.
// mActiveTarget olu handle'da kaliyor, dolayisiyla viewport hedefin
// boyutunda, projeksiyon hedefin Y yonunde kaliyor ve kare sonuna kadar
// EKRANA o durumla ciziliyordu.
TEST(RegressionFindings, DestroyingActiveTargetRestoresScreenState) {
  TargetHarness h;
  RenderTarget target = h.painter.CreateRenderTarget(64, 64);
  ASSERT_TRUE(target.IsValid());

  h.painter.Begin();
  ASSERT_TRUE(h.painter.SetRenderTarget(target));
  target.Reset();  // kullanici hedefi kare ortasinda birakti

  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.End();

  ASSERT_FALSE(h.mock->draws.empty());
  const auto& snap = h.mock->draws.back();
  EXPECT_EQ(snap.target, kInvalidRenderTarget);
  EXPECT_EQ(snap.viewport.w, kScreenW)
      << "BULGU 5: hedef yok edildikten sonra viewport hedefinki kaldi.";
  EXPECT_EQ(snap.viewport.h, kScreenH);
  // Ekrana cizerken (OpenGL) projeksiyonun Y'si ters cevrilmeli.
  EXPECT_LT(snap.projection[5], 0.0F)
      << "BULGU 5: ekrana cizim hedefin Y yonuyle gitti.";
}

// Painter yikildiktan sonra yikilan bir hedef renderer'a dokunmamali.
//
// Sozlesme hedefin Painter'dan once yikilmasini istiyor; ihlal edilirse
// eskiden ham IRenderer* uzerinden DestroyRenderTarget cagriliyordu.
TEST(RegressionFindings, TargetOutlivingPainterDoesNotTouchRenderer) {
  LifetimeJournal journal;
  RenderTarget target;
  {
    auto renderer = std::make_unique<RecordingTargetRenderer>();
    renderer->SetJournal(&journal);
    Painter painter(std::move(renderer), kScreenW, kScreenH);
    target = painter.CreateRenderTarget(32, 32);
    ASSERT_TRUE(target.IsValid());
  }
  ASSERT_TRUE(journal.renderer_destroyed);
  // Yikim burada, Painter oldukten SONRA calisir.
  EXPECT_NO_FATAL_FAILURE(target.Reset());
  EXPECT_EQ(journal.destroy_after_death, 0);
}

// SetOpacity sozlesmesi [0, 1]; aralik disi deger kirpilmali.
TEST(RegressionFindings, OpacityIsClampedToUnitRange) {
  TargetHarness h;
  h.painter.Begin();
  h.painter.SetOpacity(4.0F);
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.End();
  EXPECT_FLOAT_EQ(h.mock->last_opacity, 1.0F);

  h.mock->Reset();
  h.painter.Begin();
  h.painter.SetOpacity(-2.0F);
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.End();
  EXPECT_FLOAT_EQ(h.mock->last_opacity, 0.0F);
}

// Negatif boyutlu kirpma bos kirpmaya donusmeli.
//
// Ham gonderilseydi glScissor GL_INVALID_VALUE uretip ONCEKI scissor'i
// yururlukte birakir, Vulkan ise 0'a kelepceleyip her seyi kirpardi.
TEST(RegressionFindings, NegativeClipSizeBecomesEmptyClip) {
  TargetHarness h;
  h.painter.Begin();
  h.painter.SetClipRect(Rect{10.0F, 10.0F, -50.0F, -20.0F});
  h.painter.End();

  ASSERT_FALSE(h.mock->scissor_calls.empty());
  const auto& s = h.mock->scissor_calls.front();
  EXPECT_EQ(s.w, 0);
  EXPECT_EQ(s.h, 0);
}

namespace {

/// @brief Silinmiş bir texture ile yapılan çizimleri sayan renderer.
class DeletedTextureTracker : public MockRenderer {
 public:
  int32_t draws_with_deleted_texture{0};

  void DestroyTexture(TextureHandle handle) override {
    mDeleted.insert(handle);
    MockRenderer::DestroyTexture(handle);
  }

  void DrawTextured(const std::vector<TexturedVertex>& vertices,
                    TextureHandle texture) override {
    if (mDeleted.count(texture) != 0U) {
      ++draws_with_deleted_texture;
    }
    MockRenderer::DrawTextured(vertices, texture);
  }

 private:
  std::set<TextureHandle> mDeleted;
};

/// @brief Kaydedilen dokulu çizimlerin en sağdaki x koordinatı.
float RightmostTexturedX(const RecordingTargetRenderer& renderer) {
  float right = -std::numeric_limits<float>::infinity();
  for (const auto& draw : renderer.draws) {
    for (const TexturedVertex& v : draw.textured_vertices) {
      right = std::max(right, v.x);
    }
  }
  return right;
}

/// @brief Üye Image'ını OnInit'te GPU'ya yükleyip başlatmayı reddeden uygulama.
class FailingInitApp : public Application {
 public:
  explicit FailingInitApp(AppConfig config)
      : Application(std::move(config)), mImage(MakeImage()) {}

  [[nodiscard]] bool ImageUploaded() const noexcept { return mUploaded; }

 protected:
  bool OnInit() override {
    GetPainter().Begin();
    GetPainter().DrawImage(mImage, 0.0F, 0.0F);
    GetPainter().End();
    mUploaded = true;
    return false;
  }

 private:
  static Image MakeImage() {
    const std::vector<uint8_t> pixels(2U * 2U * 4U, 255U);
    return Image::CreateFromData(pixels.data(), 2, 2, 4);
  }

  Image mImage;
  bool mUploaded{false};
};

}  // namespace

// Painter fontun tek sahibi olsa bile font değişince kuyruktaki metin, atlas
// silinmeden çizilmeli.
TEST(PainterFont, ReplacingFontKeepsQueuedGlyphsAlive) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(font_path);
  auto renderer = std::make_unique<DeletedTextureTracker>();
  DeletedTextureTracker* tracker = renderer.get();
  Painter painter(std::move(renderer), kScreenW, kScreenH);
  painter.SetFont(std::make_shared<Font>(font_path, 24));
  ASSERT_TRUE(painter.GetFont()->IsValid());

  painter.Begin();
  painter.DrawText(0.0F, 30.0F, "A");
  painter.SetFont(nullptr);
  painter.End();

  ASSERT_GT(tracker->CountCalls("DrawTextured"), 0);
  EXPECT_EQ(tracker->draws_with_deleted_texture, 0);
}

// Aynı font tekrar seçilince batch bölünmemeli.
TEST(PainterFont, SelectingSameFontDoesNotSplitBatch) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(font_path);
  TargetHarness h;
  const auto font = std::make_shared<Font>(font_path, 24);
  ASSERT_TRUE(font->IsValid());
  h.painter.SetFont(font);

  h.painter.Begin();
  h.painter.DrawText(0.0F, 30.0F, "A");
  h.painter.SetFont(font);
  h.painter.DrawText(20.0F, 30.0F, "A");
  h.painter.End();

  EXPECT_EQ(h.mock->draws.size(), 1U);
}

// Bulgu A11: ölçüm kerning uyguluyor, çizim uygulamıyor; sağa hizalı metin
// dikdörtgenin dışına taşıyor. Düzeltmeyle birlikte DISABLED_ kaldırılacak.
TEST(PainterText, DISABLED_RightAlignedTextEndsAtRectEdge) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(font_path);
  TargetHarness h;
  h.painter.SetFont(std::make_shared<Font>(font_path, 48));
  ASSERT_TRUE(h.painter.GetFont()->IsValid());

  // Tek glyph'te kerning olmadığı için son glyph'in görünen sağ kenarı ile
  // ölçülen genişlik arasındaki fark buradan alınır.
  int32_t glyph_w = 0;
  int32_t glyph_h = 0;
  ASSERT_TRUE(h.painter.GetFont()->MeasureText("V", glyph_w, glyph_h));
  h.painter.Begin();
  h.painter.DrawText(0.0F, 60.0F, "V");
  h.painter.End();
  const float kInkOffset =
      RightmostTexturedX(*h.mock) - static_cast<float>(glyph_w);

  h.mock->draws.clear();
  constexpr float kRectRight = 400.0F;
  h.painter.Begin();
  h.painter.DrawText(Rect{0.0F, 0.0F, kRectRight, 100.0F}, "AVAVAVAVAV",
                     Alignment::kRight);
  h.painter.End();

  EXPECT_NEAR(RightmostTexturedX(*h.mock), kRectRight + kInkOffset, 2.0F);
}

// OnInit false dönse de türeyen sınıfın Image üyesi, Painter'ın renderer'ı
// yıkılmadan önce serbest bırakılmalı.
TEST(ApplicationLifecycleDeathTest, FailedOnInitReleasesMemberImageSafely) {
  if (HiddenWindow(RendererBackend::kOpenGL).Get() == nullptr) {
    GTEST_SKIP() << "OpenGL penceresi oluşturulamadı.";
  }
  GTEST_FLAG_SET(death_test_style, "threadsafe");

  EXPECT_EXIT(
      {
        AppConfig config;
        config.width = 64;
        config.height = 64;
        config.resizable = false;
        config.init_logger = false;
        {
          FailingInitApp app(config);
          if (app.Run() != 1 || !app.ImageUploaded()) {
            std::fprintf(stderr, "onkosul: OnInit calismadi\n");
            std::exit(3);
          }
        }
        std::exit(0);
      },
      ::testing::ExitedWithCode(0), "");
}
