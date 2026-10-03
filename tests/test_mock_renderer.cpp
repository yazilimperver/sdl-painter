/// @file test_mock_renderer.cpp
///
/// @brief mock_renderer.h'nin kendi testleri. Painter, Image ve Texture
///        testlerinin çoğu bu sahte renderer'ın kayıtlarına dayanır; kayıt
///        yanlışsa onlar da yanlış sonuç verir.

#include "sdl_painter/color.h"
#include "sdl_painter/renderer.h"
#include "sdl_painter/vertex.h"

#include <array>
#include <cstdint>
#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>
#include <vector>

#include "mock_renderer.h"

using sdl_painter::Color;
using sdl_painter::kInvalidTexture;
using sdl_painter::MockRenderer;
using sdl_painter::RendererBackend;
using sdl_painter::TexturedVertex;
using sdl_painter::TextureHandle;
using sdl_painter::Vertex;

namespace {

/// @brief Mock'a verilen 4x4 matris: her eleman sıfırdan farklı ve birbirinden
///        farklı, böylece Reset sonrası hiçbiri başlangıç değerine benzemez.
constexpr std::array<float, 16> kProjection{
    {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}};

constexpr std::array<float, 16> kZeroProjection{};

constexpr std::array<float, 9> kModel{{2, 3, 4, 5, 6, 7, 8, 9, 10}};

constexpr std::array<float, 9> kIdentityModel{{1, 0, 0, 0, 1, 0, 0, 0, 1}};

}  // namespace

// --- Doku tanımlayıcıları ---

TEST(MockRenderer, TextureHandlesStayUniqueAcrossReset) {
  MockRenderer r;
  const TextureHandle live = r.CreateTexture(nullptr, 2, 2, 4);
  r.Reset();
  const TextureHandle later = r.CreateTexture(nullptr, 2, 2, 4);

  EXPECT_NE(live, kInvalidTexture);
  EXPECT_NE(later, kInvalidTexture);
  EXPECT_NE(later, live) << "Reset sonrasi uretilen handle canli dokuyla "
                            "cakisti.";
}

TEST(MockRenderer, CreateTextureCountRestartsAfterReset) {
  MockRenderer r;
  r.CreateTexture(nullptr, 2, 2, 4);
  r.CreateTexture(nullptr, 2, 2, 4);
  r.Reset();
  r.CreateTexture(nullptr, 2, 2, 4);

  EXPECT_EQ(r.create_texture_count, 1);
}

TEST(MockRenderer, CreateTextureRecordsSizeAndChannels) {
  MockRenderer r;
  r.CreateTexture(nullptr, 7, 5, 3);

  EXPECT_EQ(r.last_create_call.w, 7);
  EXPECT_EQ(r.last_create_call.h, 5);
  EXPECT_EQ(r.last_create_call.channels, 3);
}

TEST(MockRenderer, FilteredCreateTextureRecordsSizeAndChannelsToo) {
  MockRenderer r;
  r.CreateTexture(nullptr, 9, 6, 1, sdl_painter::TextureFilter::kNearest);

  EXPECT_EQ(r.last_create_call.w, 9);
  EXPECT_EQ(r.last_create_call.h, 6);
  EXPECT_EQ(r.last_create_call.channels, 1);
}

TEST(MockRenderer, DestroyTextureRecordsHandlesInOrder) {
  MockRenderer r;
  const TextureHandle first = r.CreateTexture(nullptr, 2, 2, 4);
  const TextureHandle second = r.CreateTexture(nullptr, 2, 2, 4);
  r.DestroyTexture(second);
  r.DestroyTexture(first);

  const std::vector<TextureHandle> expected{second, first};
  EXPECT_EQ(r.destroyed_textures, expected);
}

TEST(MockRenderer, DoubleDestroyShowsUpAsTheSameHandleTwice) {
  MockRenderer r;
  const TextureHandle handle = r.CreateTexture(nullptr, 2, 2, 4);
  r.DestroyTexture(handle);
  r.DestroyTexture(handle);

  const std::vector<TextureHandle> expected{handle, handle};
  EXPECT_EQ(r.destroyed_textures, expected);
  EXPECT_EQ(r.destroy_texture_count, 2);
}

TEST(MockRenderer, DestroyingTheInvalidHandleIsNotRecorded) {
  MockRenderer r;
  r.DestroyTexture(kInvalidTexture);

  EXPECT_TRUE(r.destroyed_textures.empty());
  EXPECT_EQ(r.destroy_texture_count, 0);
}

// --- Başarısız doku yaratma ---

TEST(MockRenderer, FailNextCreateTextureReturnsInvalidHandleOnce) {
  MockRenderer r;
  r.fail_next_create_texture = true;

  const TextureHandle failed = r.CreateTexture(nullptr, 2, 2, 4);
  const TextureHandle retried = r.CreateTexture(nullptr, 2, 2, 4);

  EXPECT_EQ(failed, kInvalidTexture);
  EXPECT_NE(retried, kInvalidTexture) << "Bayrak ilk basarisizliktan sonra "
                                         "kapanmadi.";
  EXPECT_FALSE(r.fail_next_create_texture);
}

TEST(MockRenderer, FailedCreateTextureDoesNotConsumeAHandle) {
  MockRenderer r;
  const TextureHandle before = r.CreateTexture(nullptr, 2, 2, 4);
  r.fail_next_create_texture = true;
  r.CreateTexture(nullptr, 2, 2, 4);
  const TextureHandle after = r.CreateTexture(nullptr, 2, 2, 4);

  EXPECT_EQ(after, before + 1);
}

TEST(MockRenderer, FailedCreateTextureIsStillCountedAndRecorded) {
  MockRenderer r;
  r.fail_next_create_texture = true;
  r.CreateTexture(nullptr, 3, 4, 2);

  EXPECT_EQ(r.create_texture_count, 1);
  EXPECT_EQ(r.last_create_call.w, 3);
  EXPECT_EQ(r.last_create_call.h, 4);
  EXPECT_EQ(r.last_create_call.channels, 2);
}

// --- Clear ---

TEST(MockRenderer, ClearRecordsTheColor) {
  MockRenderer r;
  r.Clear(Color(1, 2, 3, 4));

  EXPECT_EQ(r.last_clear_color, Color(1, 2, 3, 4));
  EXPECT_EQ(r.CountCalls("Clear"), 1);
}

// --- Üçgen listesi sözleşmesi ---

TEST(MockRenderer, CompleteTriangleListsAreNotViolations) {
  MockRenderer r;
  r.DrawTriangles(std::vector<Vertex>(0));
  r.DrawTriangles(std::vector<Vertex>(3));
  r.DrawTextured(std::vector<TexturedVertex>(6), 1);

  EXPECT_EQ(r.contract_violation_count, 0);
}

TEST(MockRenderer, IncompleteTriangleListIsCountedForDrawTriangles) {
  MockRenderer r;
  r.report_contract_violations = false;
  r.DrawTriangles(std::vector<Vertex>(4));

  EXPECT_EQ(r.contract_violation_count, 1);
  EXPECT_EQ(r.CountCalls("DrawTriangles"), 1)
      << "Ihlal eden cagri yine de kaydedilmeli.";
}

TEST(MockRenderer, IncompleteTriangleListIsCountedForDrawTextured) {
  MockRenderer r;
  r.report_contract_violations = false;
  r.DrawTextured(std::vector<TexturedVertex>(5), 1);

  EXPECT_EQ(r.contract_violation_count, 1);
  EXPECT_EQ(r.CountCalls("DrawTextured"), 1);
}

/// @brief Varsayılan ayarda ihlal, çağıran testin kendisini düşürür; Painter'ı
///        süren testlerin denetimi kendiliğinden almasının dayanağı bu.
TEST(MockRenderer, ViolationFailsTheRunningTestByDefault) {
  MockRenderer r;
  EXPECT_NONFATAL_FAILURE(r.DrawTriangles(std::vector<Vertex>(4)),
                          "3'un kati degil");
  EXPECT_NONFATAL_FAILURE(r.DrawTextured(std::vector<TexturedVertex>(2), 1),
                          "3'un kati degil");
  EXPECT_EQ(r.contract_violation_count, 2);
}

// --- Reset ---

TEST(MockRenderer, ResetRestoresModelAndProjectionToTheirInitialValues) {
  MockRenderer r;
  r.SetModelMatrix(kModel.data());
  r.SetProjectionMatrix(kProjection.data());
  ASSERT_NE(r.last_model, kIdentityModel);
  ASSERT_NE(r.last_projection, kZeroProjection);

  r.Reset();

  EXPECT_EQ(r.last_model, kIdentityModel);
  EXPECT_EQ(r.last_projection, kZeroProjection);
}

TEST(MockRenderer, ResetClearsTextureRecords) {
  MockRenderer r;
  const TextureHandle handle = r.CreateTexture(nullptr, 7, 5, 3);
  r.DestroyTexture(handle);
  ASSERT_EQ(r.create_texture_count, 1);
  ASSERT_EQ(r.destroy_texture_count, 1);

  r.Reset();

  EXPECT_EQ(r.create_texture_count, 0);
  EXPECT_EQ(r.destroy_texture_count, 0);
  EXPECT_TRUE(r.destroyed_textures.empty());
  EXPECT_EQ(r.last_create_call.w, 0);
  EXPECT_EQ(r.last_create_call.h, 0);
  EXPECT_EQ(r.last_create_call.channels, 0);
}

TEST(MockRenderer, ResetClearsClearAndViolationRecords) {
  MockRenderer r;
  r.report_contract_violations = false;
  r.Clear(Color(1, 2, 3, 4));
  r.DrawTriangles(std::vector<Vertex>(4));
  ASSERT_EQ(r.contract_violation_count, 1);

  r.Reset();

  EXPECT_EQ(r.last_clear_color, Color());
  EXPECT_EQ(r.contract_violation_count, 0);
}

/// @brief Reset kaydedilenleri siler, testin kurduğu yapılandırmayı silmez.
TEST(MockRenderer, ResetKeepsTheTestConfiguration) {
  MockRenderer r;
  r.backend = RendererBackend::kVulkan;
  r.fail_next_create_texture = true;
  r.report_contract_violations = false;

  r.Reset();

  EXPECT_EQ(r.GetBackend(), RendererBackend::kVulkan);
  EXPECT_TRUE(r.fail_next_create_texture);
  EXPECT_FALSE(r.report_contract_violations);
}

/// @brief Reset kuralının tamamı: her gözlem alanı ve sayaç ilk durumuna
///        döner; doku tanımlayıcı sayacı ve test yapılandırması korunur.
TEST(MockRenderer, ResetRestoresEveryObservationAndKeepsTheRest) {
  MockRenderer r;
  r.backend = RendererBackend::kVulkan;
  r.report_contract_violations = false;
  r.BeginFrame();
  r.SetViewport(1, 2, 3, 4);
  r.SetScissor(5, 6, 7, 8);
  r.ClearScissor();
  r.Clear(Color(1, 2, 3, 4));
  r.SetOpacity(0.5F);
  r.SetBlendMode(sdl_painter::BlendMode::kAdditive);
  r.SetModelMatrix(kModel.data());
  r.SetProjectionMatrix(kProjection.data());
  const TextureHandle first =
      r.CreateTexture(nullptr, 7, 5, 3, sdl_painter::TextureFilter::kNearest);
  const TextureHandle second = r.CreateTexture(nullptr, 2, 2, 4);
  r.UpdateTexture(first, 1, 2, 3, 4, nullptr);
  r.DestroyTexture(first);
  r.DrawTriangles(std::vector<Vertex>(4));
  r.DrawTextured(std::vector<TexturedVertex>(3), second);
  r.fail_next_create_texture = true;
  ASSERT_FALSE(r.calls.empty());
  ASSERT_EQ(r.contract_violation_count, 1);

  r.Reset();

  // Sıfırlananlar.
  EXPECT_TRUE(r.calls.empty());
  EXPECT_TRUE(r.last_vertices.empty());
  EXPECT_TRUE(r.last_textured_vertices.empty());
  EXPECT_EQ(r.create_texture_count, 0);
  EXPECT_EQ(r.destroy_texture_count, 0);
  EXPECT_EQ(r.update_texture_count, 0);
  EXPECT_TRUE(r.update_texture_calls.empty());
  EXPECT_FLOAT_EQ(r.last_opacity, 1.0F);
  EXPECT_TRUE(r.model_at_draw.empty());
  EXPECT_TRUE(r.scissor_calls.empty());
  EXPECT_TRUE(r.viewport_calls.empty());
  EXPECT_EQ(r.clear_scissor_count, 0);
  EXPECT_TRUE(r.blend_calls.empty());
  EXPECT_EQ(r.last_filter, sdl_painter::TextureFilter::kLinear);
  EXPECT_EQ(r.last_model, kIdentityModel);
  EXPECT_EQ(r.last_projection, kZeroProjection);
  EXPECT_EQ(r.last_create_call.w, 0);
  EXPECT_TRUE(r.destroyed_textures.empty());
  EXPECT_EQ(r.last_clear_color, Color());
  EXPECT_EQ(r.contract_violation_count, 0);

  // Korunanlar.
  EXPECT_EQ(r.GetBackend(), RendererBackend::kVulkan);
  EXPECT_TRUE(r.fail_next_create_texture);
  EXPECT_FALSE(r.report_contract_violations);
  r.fail_next_create_texture = false;
  const TextureHandle third = r.CreateTexture(nullptr, 2, 2, 4);
  EXPECT_NE(third, first);
  EXPECT_NE(third, second);
}
