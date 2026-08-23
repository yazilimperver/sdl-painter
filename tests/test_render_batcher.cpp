#include <glm/glm.hpp>
#include <gtest/gtest.h>

#include <vector>

#include "mock_renderer.h"
#include "sdl_painter/color.h"
#include "sdl_painter/vertex.h"

// RenderBatcher dahili başlık (src/ altında)
#include "../src/render_batcher.h"

using sdl_painter::Color;
using sdl_painter::MockRenderer;
using sdl_painter::RenderBatcher;
using sdl_painter::TexturedVertex;
using sdl_painter::Vertex;
using sdl_painter::kInvalidTexture;

// Transform artik CPU'da vertex'e uygulaniyor; batch davranisini test
// eden bu dosyada birim matris kullanilir (dönüsümün kendisi
// test_render_batcher_transform testlerinde sinanir).
static const glm::mat3 kIdentity(1.0f);

// Üç vertex'ten oluşan basit bir üçgen üretir.
static std::vector<Vertex> MakeTriangle() {
  return {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.5f, 1.0f}};
}

// İki üçgenden oluşan quad üretir (6 vertex).
static std::vector<Vertex> MakeQuad() {
  return {{0, 0}, {1, 0}, {1, 1}, {0, 0}, {1, 1}, {0, 1}};
}

static std::vector<TexturedVertex> MakeTexturedQuad() {
  return {
      {0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f, 0.0f},
      {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f},
      {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f},
  };
}

// ─── Flush davranışı ────────────────────────────────────────────────────────

TEST(RenderBatcher, FlushOnEmptyBatcherDoesNotCallRenderer) {
  MockRenderer r;
  RenderBatcher batcher(r);
  batcher.Flush();
  EXPECT_EQ(r.CountCalls("DrawTriangles"), 0);
  EXPECT_EQ(r.CountCalls("DrawTextured"), 0);
}

TEST(RenderBatcher, FlushAfterPushTrianglesSendsDrawCall) {
  MockRenderer r;
  RenderBatcher batcher(r);
  batcher.PushTriangles(MakeTriangle(), kIdentity, Color{255, 0, 0, 255}, 1.0f);
  batcher.Flush();
  EXPECT_EQ(r.CountCalls("DrawTriangles"), 1);
}

TEST(RenderBatcher, FlushClearsBuffer_SecondFlushDoesNothing) {
  MockRenderer r;
  RenderBatcher batcher(r);
  batcher.PushTriangles(MakeTriangle(), kIdentity, Color{255, 0, 0, 255}, 1.0f);
  batcher.Flush();
  batcher.Flush();  // ikinci flush boş — çağrı yapılmamalı
  EXPECT_EQ(r.CountCalls("DrawTriangles"), 1);
}

TEST(RenderBatcher, FlushSendsCorrectVertexCount) {
  MockRenderer r;
  RenderBatcher batcher(r);
  auto tri = MakeTriangle();
  batcher.PushTriangles(tri, kIdentity, Color{255, 0, 0, 255}, 1.0f);
  batcher.Flush();
  ASSERT_EQ(r.last_vertices.size(), 3u);
}

// ─── Biriktirme (batching) ──────────────────────────────────────────────────

TEST(RenderBatcher, TwoConsecutivePushesAreBatchedIntoOneDraw) {
  MockRenderer r;
  RenderBatcher batcher(r);
  Color red{255, 0, 0, 255};
  batcher.PushTriangles(MakeTriangle(), kIdentity, red, 1.0f);
  batcher.PushTriangles(MakeTriangle(), kIdentity, red, 1.0f);
  batcher.Flush();
  // Tek bir DrawTriangles çağrısı, 6 vertex
  EXPECT_EQ(r.CountCalls("DrawTriangles"), 1);
  EXPECT_EQ(r.last_vertices.size(), 6u);
}

TEST(RenderBatcher, DifferentColorsSameModeAreBatched) {
  // Renk vertex'e gömüldüğü için farklı renkler aynı batch'e girebilir.
  MockRenderer r;
  RenderBatcher batcher(r);
  batcher.PushTriangles(MakeTriangle(), kIdentity, Color{255, 0, 0, 255}, 1.0f);
  batcher.PushTriangles(MakeTriangle(), kIdentity, Color{0, 0, 255, 255}, 1.0f);
  batcher.Flush();
  EXPECT_EQ(r.CountCalls("DrawTriangles"), 1);
  EXPECT_EQ(r.last_vertices.size(), 6u);
}

// ─── State değişimi → otomatik flush ────────────────────────────────────────

TEST(RenderBatcher, ModeChangeBasicToTexturedTriggersFlushen) {
  MockRenderer r;
  RenderBatcher batcher(r);
  batcher.PushTriangles(MakeTriangle(), kIdentity, Color{255, 0, 0, 255}, 1.0f);
  // Textured push → önceki basic batch önce flush edilmeli
  batcher.PushTexturedTriangles(MakeTexturedQuad(), kIdentity, 1, Color{255, 255, 255, 255}, 1.0f);
  // Bu noktada DrawTriangles çağrısı olmuş olmalı
  EXPECT_EQ(r.CountCalls("DrawTriangles"), 1);
  batcher.Flush();
  EXPECT_EQ(r.CountCalls("DrawTextured"), 1);
}

TEST(RenderBatcher, ModeChangeTexturedToBasicTriggersFlushen) {
  MockRenderer r;
  RenderBatcher batcher(r);
  batcher.PushTexturedTriangles(MakeTexturedQuad(), kIdentity, 1, Color{255, 255, 255, 255}, 1.0f);
  batcher.PushTriangles(MakeTriangle(), kIdentity, Color{255, 0, 0, 255}, 1.0f);
  EXPECT_EQ(r.CountCalls("DrawTextured"), 1);
  batcher.Flush();
  EXPECT_EQ(r.CountCalls("DrawTriangles"), 1);
}

TEST(RenderBatcher, OpacityChangeTriggersFlushen) {
  MockRenderer r;
  RenderBatcher batcher(r);
  batcher.PushTriangles(MakeTriangle(), kIdentity, Color{255, 0, 0, 255}, 1.0f);
  batcher.PushTriangles(MakeTriangle(), kIdentity, Color{255, 0, 0, 255}, 0.5f);
  // Opacity farkı → ilk batch flush edildi
  EXPECT_EQ(r.CountCalls("DrawTriangles"), 1);
  batcher.Flush();
  EXPECT_EQ(r.CountCalls("DrawTriangles"), 2);
}

TEST(RenderBatcher, TextureChangeTriggersFlushen) {
  MockRenderer r;
  RenderBatcher batcher(r);
  batcher.PushTexturedTriangles(MakeTexturedQuad(), kIdentity, 1, Color{255, 255, 255, 255}, 1.0f);
  batcher.PushTexturedTriangles(MakeTexturedQuad(), kIdentity, 2, Color{255, 255, 255, 255}, 1.0f);
  // Texture handle değişti → flush
  EXPECT_EQ(r.CountCalls("DrawTextured"), 1);
  batcher.Flush();
  EXPECT_EQ(r.CountCalls("DrawTextured"), 2);
}

// ─── Opacity renderer'a iletiliyor mu? ─────────────────────────────────────

TEST(RenderBatcher, FlushSetsOpacityBeforeDrawCall) {
  MockRenderer r;
  RenderBatcher batcher(r);
  batcher.PushTriangles(MakeTriangle(), kIdentity, Color{255, 0, 0, 255}, 0.75f);
  batcher.Flush();
  EXPECT_FLOAT_EQ(r.last_opacity, 0.75f);
}

TEST(RenderBatcher, FlushSetsCorrectOpacityForTextured) {
  MockRenderer r;
  RenderBatcher batcher(r);
  batcher.PushTexturedTriangles(MakeTexturedQuad(), kIdentity, 1, Color{255, 255, 255, 255}, 0.3f);
  batcher.Flush();
  EXPECT_FLOAT_EQ(r.last_opacity, 0.3f);
}

// ─── Renk vertex'e gömülüyor mu? ────────────────────────────────────────────

TEST(RenderBatcher, PushTrianglesEmbedColorIntoVertices) {
  MockRenderer r;
  RenderBatcher batcher(r);
  Color red{200, 50, 10, 180};
  batcher.PushTriangles(MakeTriangle(), kIdentity, red, 1.0f);
  batcher.Flush();
  ASSERT_EQ(r.last_vertices.size(), 3u);
  for (const auto& v : r.last_vertices) {
    EXPECT_EQ(v.r, 200);
    EXPECT_EQ(v.g, 50);
    EXPECT_EQ(v.b, 10);
    EXPECT_EQ(v.a, 180);
  }
}

TEST(RenderBatcher, PushTexturedTrianglesEmbedTintIntoVertices) {
  MockRenderer r;
  RenderBatcher batcher(r);
  Color tint{128, 64, 32, 200};
  batcher.PushTexturedTriangles(MakeTexturedQuad(), kIdentity, 1, tint, 1.0f);
  batcher.Flush();
  ASSERT_FALSE(r.last_textured_vertices.empty());
  for (const auto& v : r.last_textured_vertices) {
    EXPECT_EQ(v.r, 128);
    EXPECT_EQ(v.g, 64);
    EXPECT_EQ(v.b, 32);
    EXPECT_EQ(v.a, 200);
  }
}

// ─── Buffer taşması → otomatik flush ────────────────────────────────────────

TEST(RenderBatcher, BufferOverflowTriggersMidBatchFlush) {
  MockRenderer r;
  RenderBatcher batcher(r);
  // kMaxVertices = 8192. Her push 3 vertex ekler.
  // 8191 vertex doldurup (2730 üçgen + 1 kalan), sonra 3 daha ekleyince taşar.
  constexpr std::size_t kMaxVertices = 8192;
  Color c{255, 0, 0, 255};

  // Tam olarak kMaxVertices - 2 vertex ekle (3'ün katı için 8190)
  std::size_t pushed = 0;
  while (pushed + 3 <= kMaxVertices - 2) {
    batcher.PushTriangles(MakeTriangle(), kIdentity, c, 1.0f);
    pushed += 3;
  }
  EXPECT_EQ(r.CountCalls("DrawTriangles"), 0);  // henüz flush yok

  // Bir tane daha ekle → taşma → otomatik flush
  batcher.PushTriangles(MakeTriangle(), kIdentity, c, 1.0f);
  EXPECT_EQ(r.CountCalls("DrawTriangles"), 1);

  batcher.Flush();
  EXPECT_EQ(r.CountCalls("DrawTriangles"), 2);
}

// ─── Textured batch biriktirme ───────────────────────────────────────────────

TEST(RenderBatcher, TwoTexturedPushesSameTextureBatched) {
  MockRenderer r;
  RenderBatcher batcher(r);
  Color white{255, 255, 255, 255};
  batcher.PushTexturedTriangles(MakeTexturedQuad(), kIdentity, 5, white, 1.0f);
  batcher.PushTexturedTriangles(MakeTexturedQuad(), kIdentity, 5, white, 1.0f);
  batcher.Flush();
  EXPECT_EQ(r.CountCalls("DrawTextured"), 1);
  EXPECT_EQ(r.last_textured_vertices.size(), 12u);
}
