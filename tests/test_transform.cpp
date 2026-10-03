#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/vertex.h"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "mock_renderer.h"

// Transform sınıfı kaldırıldı; artık doğrudan glm::mat3 (column-major) kullanılır.
// Bu testler, Painter'ın transform metodlarının dayandığı iki sözleşmeyi kilitler:
//   1) affine matrislerin column-major kuruluşu (mat[col][row]),
//   2) post-multiply (sağdan çarp) M = M * op.
// Ayrıca glm::value_ptr'ın GPU'ya giden column-major byte düzenini doğrular.
// Yukarıdaki GLM testleri Painter'ı çağırmaz; sözleşmenin Painter üzerinden
// de tutulduğunu PainterTransformCompositionTest grubu doğrular.

namespace sdl_painter {
namespace {

constexpr float kEpsilon = 1e-5f;

// --- Painter ile birebir aynı affine matris kurucuları (column-major) ---

glm::mat3 MakeTranslate(float dx, float dy) {
  glm::mat3 t(1.0f);
  t[2][0] = dx;
  t[2][1] = dy;
  return t;
}

glm::mat3 MakeRotate(float angle_degrees) {
  const float r = glm::radians(angle_degrees);
  const float c = std::cos(r);
  const float s = std::sin(r);
  glm::mat3 rot(1.0f);
  rot[0][0] = c;
  rot[0][1] = s;
  rot[1][0] = -s;
  rot[1][1] = c;
  return rot;
}

glm::mat3 MakeScale(float sx, float sy) {
  glm::mat3 sc(1.0f);
  sc[0][0] = sx;
  sc[1][1] = sy;
  return sc;
}

// 2B noktayı homojen koordinatta dönüştür: p' = M * (x, y, 1).
void Map(const glm::mat3& m, float x, float y, float* out_x, float* out_y) {
  const glm::vec3 p = m * glm::vec3(x, y, 1.0f);
  *out_x = p.x;
  *out_y = p.y;
}

constexpr float kBoundsTolerance = 1e-3f;

// MockRenderer'ı süren Painter. Transform CPU'da vertex'lere gömüldüğü için
// sonuç renderer'a giden koordinatlardan okunur.
struct PainterHarness {
  MockRenderer* mock{nullptr};
  Painter painter;

  PainterHarness() : painter(MakeMock(mock), 800, 600) {}

  // Sahipliği Painter'a verir, ham pointer'ı out_mock'a yazar.
  static std::unique_ptr<MockRenderer> MakeMock(MockRenderer*& out_mock) {
    auto m = std::make_unique<MockRenderer>();
    out_mock = m.get();
    return m;
  }
};

// Renderer'a giden son vertex dizisinin eksen hizalı sınır kutusu.
struct VertexBounds {
  float min_x{0.0f};
  float min_y{0.0f};
  float max_x{0.0f};
  float max_y{0.0f};
};

VertexBounds BoundsOfLastDraw(const MockRenderer& mock) {
  const std::vector<Vertex>& verts = mock.last_vertices;
  VertexBounds b{verts.front().x, verts.front().y, verts.front().x,
                 verts.front().y};
  for (const Vertex& v : verts) {
    b.min_x = std::min(b.min_x, v.x);
    b.min_y = std::min(b.min_y, v.y);
    b.max_x = std::max(b.max_x, v.x);
    b.max_y = std::max(b.max_y, v.y);
  }
  return b;
}

}  // namespace

TEST(TransformTest, DefaultIsIdentity) {
  glm::mat3 t(1.0f);
  EXPECT_EQ(t, glm::mat3(1.0f));
}

TEST(TransformTest, IdentityMapIsNoOp) {
  glm::mat3 t(1.0f);
  float ox = 0.0f;
  float oy = 0.0f;
  Map(t, 3.0f, 4.0f, &ox, &oy);
  EXPECT_NEAR(ox, 3.0f, kEpsilon);
  EXPECT_NEAR(oy, 4.0f, kEpsilon);
}

TEST(TransformTest, Translate) {
  glm::mat3 t = MakeTranslate(5.0f, -3.0f);
  float ox = 0.0f;
  float oy = 0.0f;
  Map(t, 1.0f, 1.0f, &ox, &oy);
  EXPECT_NEAR(ox, 6.0f, kEpsilon);
  EXPECT_NEAR(oy, -2.0f, kEpsilon);
}

TEST(TransformTest, Scale) {
  glm::mat3 t = MakeScale(2.0f, 3.0f);
  float ox = 0.0f;
  float oy = 0.0f;
  Map(t, 4.0f, 5.0f, &ox, &oy);
  EXPECT_NEAR(ox, 8.0f, kEpsilon);
  EXPECT_NEAR(oy, 15.0f, kEpsilon);
}

TEST(TransformTest, Rotate90Degrees) {
  glm::mat3 t = MakeRotate(90.0f);
  float ox = 0.0f;
  float oy = 0.0f;
  // (1, 0) -> (0, 1). Matematiksel (Y yukarı) düzende saat yönü tersidir;
  // Y'nin aşağı baktığı ekran düzeninde ekranda saat yönündedir (QPainter
  // ile aynı).
  Map(t, 1.0f, 0.0f, &ox, &oy);
  EXPECT_NEAR(ox, 0.0f, kEpsilon);
  EXPECT_NEAR(oy, 1.0f, kEpsilon);
}

TEST(TransformTest, Rotate180Degrees) {
  glm::mat3 t = MakeRotate(180.0f);
  float ox = 0.0f;
  float oy = 0.0f;
  Map(t, 1.0f, 0.0f, &ox, &oy);
  EXPECT_NEAR(ox, -1.0f, kEpsilon);
  EXPECT_NEAR(oy, 0.0f, kEpsilon);
}

TEST(TransformTest, MatrixMultiplication) {
  // operator* standart matris çarpımı: (A*B)*v = A*(B*v) → B önce uygulanır.
  glm::mat3 translate = MakeTranslate(2.0f, 0.0f);
  glm::mat3 scale = MakeScale(3.0f, 1.0f);

  // translate * scale: önce scale, sonra translate.
  // (1,0) → scale(3x) → (3,0) → translate(+2) → (5,0)
  glm::mat3 combined_ts = translate * scale;
  float ox = 0.0f;
  float oy = 0.0f;
  Map(combined_ts, 1.0f, 0.0f, &ox, &oy);
  EXPECT_NEAR(ox, 5.0f, kEpsilon);
  EXPECT_NEAR(oy, 0.0f, kEpsilon);

  // scale * translate: önce translate, sonra scale.
  // (1,0) → translate(+2) → (3,0) → scale(3x) → (9,0)
  glm::mat3 combined_st = scale * translate;
  Map(combined_st, 1.0f, 0.0f, &ox, &oy);
  EXPECT_NEAR(ox, 9.0f, kEpsilon);
  EXPECT_NEAR(oy, 0.0f, kEpsilon);
}

TEST(TransformTest, ChainedOperations) {
  // Sağdan-çarp (post-multiply) semantiği: Translate(10,0) sonra Scale(2,2)
  // → vertex önce ölçeklenir, sonra ötelenir.
  // M = T(10,0) * S(2,2); v=(0,0) için: S·v=(0,0), T·(0,0)=(10,0).
  glm::mat3 t(1.0f);
  t = t * MakeTranslate(10.0f, 0.0f);
  t = t * MakeScale(2.0f, 2.0f);

  float ox = 0.0f;
  float oy = 0.0f;
  Map(t, 0.0f, 0.0f, &ox, &oy);
  EXPECT_NEAR(ox, 10.0f, kEpsilon);
  EXPECT_NEAR(oy, 0.0f, kEpsilon);

  // Ölçeklenen bir vertex için fark görünür: v=(1,0) için
  // S·v=(2,0), T·(2,0)=(12,0).
  Map(t, 1.0f, 0.0f, &ox, &oy);
  EXPECT_NEAR(ox, 12.0f, kEpsilon);
  EXPECT_NEAR(oy, 0.0f, kEpsilon);
}

TEST(TransformTest, TranslateThenRotateRotatesAroundLocalOrigin) {
  //  Translate(cx,cy) sonra Rotate(90), FillRect(-1,-1,2,2)
  // → dikdörtgenin merkezi (cx,cy)'de kalır, kendi etrafında döner.
  glm::mat3 t(1.0f);
  t = t * MakeTranslate(100.0f, 50.0f);
  t = t * MakeRotate(90.0f);

  float ox = 0.0f;
  float oy = 0.0f;
  // Lokal (1,0) noktası: önce 90° döner → (0,1), sonra (100,50)'ye ötelenir.
  Map(t, 1.0f, 0.0f, &ox, &oy);
  EXPECT_NEAR(ox, 100.0f, kEpsilon);
  EXPECT_NEAR(oy, 51.0f, kEpsilon);

  // Lokal merkez (0,0) → daima (100,50)'ye düşmeli.
  Map(t, 0.0f, 0.0f, &ox, &oy);
  EXPECT_NEAR(ox, 100.0f, kEpsilon);
  EXPECT_NEAR(oy, 50.0f, kEpsilon);
}

TEST(TransformTest, Equality) {
  glm::mat3 a = MakeTranslate(1.0f, 2.0f);
  glm::mat3 b = MakeTranslate(1.0f, 2.0f);
  glm::mat3 c = MakeTranslate(1.0f, 3.0f);
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
}

TEST(TransformTest, ValuePtrIsColumnMajor) {
  // glm::value_ptr, GPU'ya (SetModelMatrix) giden column-major byte düzenini
  // verir. Bu test, OpenGL GL_FALSE upload'u ve Vulkan repack indekslerinin
  // dayandığı düzeni kilitler: element = data[col*3 + row], tx=[6], ty=[7].
  glm::mat3 t = MakeTranslate(7.0f, 9.0f);
  const float* data = glm::value_ptr(t);
  ASSERT_NE(data, nullptr);
  EXPECT_FLOAT_EQ(data[0], 1.0f);  // m00
  EXPECT_FLOAT_EQ(data[4], 1.0f);  // m11
  EXPECT_FLOAT_EQ(data[8], 1.0f);  // m22
  EXPECT_FLOAT_EQ(data[6], 7.0f);  // tx
  EXPECT_FLOAT_EQ(data[7], 9.0f);  // ty

  // Köşegen dışı düzen: sütun 0 = (c, s, 0) -> data[0..2], sütun 1 =
  // (-s, c, 0) -> data[3..5]. 90 derecede s = 1, yani data[1] = 1 ve
  // data[3] = -1. Vulkan yeniden paketlemesi bu iki indeksi de okur.
  const glm::mat3 r = MakeRotate(90.0f);
  const float* rdata = glm::value_ptr(r);
  ASSERT_NE(rdata, nullptr);
  EXPECT_NEAR(rdata[1], 1.0f, kEpsilon);   // mat[0][1]
  EXPECT_NEAR(rdata[3], -1.0f, kEpsilon);  // mat[1][0]
}

// --- Painter üzerinden bileşim sırası ---
//
// Her senaryo yer değiştirmeyen iki işlem kullanır; sağdan çarpım (M = M * op,
// QPainter semantiği) ile soldan çarpım (M = op * M) farklı sınır kutusu
// verir. Böylece Painter'daki çarpım sırası ters çevrilirse test düşer.

TEST(PainterTransformCompositionTest, TranslateThenScaleScalesFirstThenMoves) {
  PainterHarness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.Translate(10.0f, 0.0f);
  h.painter.Scale(2.0f, 2.0f);
  h.painter.FillRect(0.0f, 0.0f, 1.0f, 1.0f);
  h.painter.End();

  // Birim kare önce 2 katına çıkar (0..2), sonra 10 öteleme alır (10..12).
  // Soldan çarpımda önce ötelenip (10..11) sonra ölçeklenirdi: x 20..22.
  ASSERT_FALSE(h.mock->last_vertices.empty()) << "Hiç çizim komutu üretilmedi.";
  const VertexBounds b = BoundsOfLastDraw(*h.mock);
  EXPECT_NEAR(b.min_x, 10.0f, kBoundsTolerance);
  EXPECT_NEAR(b.max_x, 12.0f, kBoundsTolerance);
  EXPECT_NEAR(b.min_y, 0.0f, kBoundsTolerance);
  EXPECT_NEAR(b.max_y, 2.0f, kBoundsTolerance);
}

TEST(PainterTransformCompositionTest, ScaleThenTranslateScalesTheOffset) {
  PainterHarness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.Scale(3.0f, 1.0f);
  h.painter.Translate(2.0f, 0.0f);
  h.painter.FillRect(0.0f, 0.0f, 1.0f, 1.0f);
  h.painter.End();

  // Birim kare önce 2 öteleme alır (2..3), sonra x 3 katına çıkar (6..9).
  // Soldan çarpımda önce ölçeklenip (0..3) sonra ötelenirdi: x 2..5.
  ASSERT_FALSE(h.mock->last_vertices.empty()) << "Hiç çizim komutu üretilmedi.";
  const VertexBounds b = BoundsOfLastDraw(*h.mock);
  EXPECT_NEAR(b.min_x, 6.0f, kBoundsTolerance);
  EXPECT_NEAR(b.max_x, 9.0f, kBoundsTolerance);
  EXPECT_NEAR(b.min_y, 0.0f, kBoundsTolerance);
  EXPECT_NEAR(b.max_y, 1.0f, kBoundsTolerance);
}

TEST(PainterTransformCompositionTest,
     TranslateThenRotateRotatesAroundNewOrigin) {
  PainterHarness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.Translate(100.0f, 50.0f);
  h.painter.Rotate(90.0f);
  h.painter.FillRect(0.0f, 0.0f, 10.0f, 10.0f);
  h.painter.End();

  // Kare önce 90 derece döner ((x, y) -> (-y, x): x -10..0, y 0..10), sonra
  // (100, 50) öteleme alır: x 90..100, y 50..60. Soldan çarpımda önce
  // ötelenip (100..110, 50..60) sonra dönerdi: x -60..-50, y 100..110.
  ASSERT_FALSE(h.mock->last_vertices.empty()) << "Hiç çizim komutu üretilmedi.";
  const VertexBounds b = BoundsOfLastDraw(*h.mock);
  EXPECT_NEAR(b.min_x, 90.0f, kBoundsTolerance);
  EXPECT_NEAR(b.max_x, 100.0f, kBoundsTolerance);
  EXPECT_NEAR(b.min_y, 50.0f, kBoundsTolerance);
  EXPECT_NEAR(b.max_y, 60.0f, kBoundsTolerance);
}

}  // namespace sdl_painter
