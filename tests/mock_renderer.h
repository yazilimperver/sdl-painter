#pragma once

#include "sdl_painter/color.h"
#include "sdl_painter/renderer.h"
#include "sdl_painter/vertex.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace sdl_painter {

/// @brief IRenderer'ın test amaçlı sahte implementasyonu.
///
/// Çağrı geçmişini ve son gönderilen verileri kaydeder.
/// Testlerde renderer davranışını doğrulamak için kullanılır.
class MockRenderer : public IRenderer {
 public:
  /// @brief Tek bir renderer çağrısının kaydı.
  struct Call {
    std::string name;
    std::size_t vertex_count{0};
    TextureHandle texture{kInvalidTexture};
    float opacity{1.0f};
  };

  // --- Kaydedilen çağrı geçmişi ---
  std::vector<Call> calls;
  std::vector<Vertex> last_vertices;
  std::vector<TexturedVertex> last_textured_vertices;

  /// @brief Bir scissor / viewport dikdörtgeni kaydı.
  struct RectCall {
    int32_t x{0};
    int32_t y{0};
    int32_t w{0};
    int32_t h{0};
  };

  /// @brief Bir UpdateTexture (sub-image) çağrısının kaydı.
  struct UpdateCall {
    TextureHandle texture{kInvalidTexture};
    int32_t x{0};
    int32_t y{0};
    int32_t w{0};
    int32_t h{0};
  };

  // --- Sayaçlar ---
  int32_t create_texture_count{0};   ///< CreateTexture kaç kez çağrıldı
  int32_t destroy_texture_count{0};  ///< DestroyTexture kaç kez çağrıldı
  int32_t update_texture_count{0};   ///< UpdateTexture kaç kez çağrıldı
  float last_opacity{1.0f};          ///< Son SetOpacity değeri

  /// @brief SetBlendMode çağrılarının sırası (batch kırılmasını sınamak için).
  std::vector<BlendMode> blend_calls;
  /// @brief CreateTexture'a verilen son filtre.
  TextureFilter last_filter{TextureFilter::kLinear};

  std::vector<UpdateCall> update_texture_calls;  ///< UpdateTexture çağrıları

  /// @brief SetModelMatrix'e verilen son 3x3 matris (sütun-major).
  std::array<float, 9> last_model{{1, 0, 0, 0, 1, 0, 0, 0, 1}};
  /// @brief SetProjectionMatrix'e verilen son 4x4 matris (sütun-major).
  std::array<float, 16> last_projection{};

  /// @brief Bir çizim komutu issue edilirken yürürlükte olan model matrisi.
  ///
  /// DrawTriangles / DrawTextured anında `last_model`'in kopyası alınır;
  /// böylece "çizim doğru transform ile mi gitti?" sorusu test edilebilir.
  std::vector<std::array<float, 9>> model_at_draw;

  std::vector<RectCall> scissor_calls;   ///< SetScissor çağrıları (sırayla)
  std::vector<RectCall> viewport_calls;  ///< SetViewport çağrıları (sırayla)
  int32_t clear_scissor_count{0};        ///< ClearScissor kaç kez çağrıldı

  // --- IRenderer arayüzü ---

  bool Initialize(SDL_Window*) override { return true; }
  void Shutdown() override {}
  void BeginFrame() override { calls.push_back({"BeginFrame"}); }
  void EndFrame() override { calls.push_back({"EndFrame"}); }

  void SetViewport(int32_t x, int32_t y, int32_t w, int32_t h) override {
    viewport_calls.push_back({x, y, w, h});
  }
  void SetScissor(int32_t x, int32_t y, int32_t w, int32_t h) override {
    scissor_calls.push_back({x, y, w, h});
    calls.push_back({"SetScissor"});
  }
  void ClearScissor() override {
    ++clear_scissor_count;
    calls.push_back({"ClearScissor"});
  }
  void Clear(const Color&) override { calls.push_back({"Clear"}); }

  void SetOpacity(float alpha) override {
    last_opacity = alpha;
    calls.push_back({"SetOpacity", 0, kInvalidTexture, alpha});
  }

  void SetBlendMode(BlendMode mode) override {
    blend_calls.push_back(mode);
    calls.push_back({"SetBlendMode"});
  }

  void DrawTriangles(const std::vector<Vertex>& vertices) override {
    calls.push_back({"DrawTriangles", vertices.size()});
    last_vertices = vertices;
    model_at_draw.push_back(last_model);
  }

  /// @brief Her çağrıda artan benzersiz handle döner (1'den başlar).
  TextureHandle CreateTexture(const uint8_t*, int32_t, int32_t,
                              int32_t) override {
    ++create_texture_count;
    return static_cast<TextureHandle>(create_texture_count);
  }

  TextureHandle CreateTexture(const uint8_t* data, int32_t w, int32_t h,
                              int32_t channels, TextureFilter filter) override {
    last_filter = filter;
    return CreateTexture(data, w, h, channels);
  }

  void UpdateTexture(TextureHandle handle, int32_t x, int32_t y, int32_t w,
                     int32_t h, const uint8_t*) override {
    ++update_texture_count;
    update_texture_calls.push_back({handle, x, y, w, h});
  }

  void DestroyTexture(TextureHandle handle) override {
    if (handle != kInvalidTexture) ++destroy_texture_count;
  }

  void DrawTextured(const std::vector<TexturedVertex>& vertices,
                    TextureHandle texture) override {
    calls.push_back({"DrawTextured", vertices.size(), texture});
    last_textured_vertices = vertices;
    model_at_draw.push_back(last_model);
  }

  RendererBackend GetBackend() const override { return backend; }
  void SetProjectionMatrix(const float* mat4) override {
    for (std::size_t i = 0; i < last_projection.size(); ++i) {
      last_projection[i] = mat4[i];
    }
  }
  void SetModelMatrix(const float* mat3) override {
    for (std::size_t i = 0; i < last_model.size(); ++i) {
      last_model[i] = mat3[i];
    }
  }

  /// @brief Testin backend'i değiştirebilmesi için (Y-flip dallanması vb.).
  RendererBackend backend{RendererBackend::kOpenGL};

  // --- Yardımcılar ---

  /// @brief Belirli isimde kaç çağrı yapıldığını say.
  int32_t CountCalls(const std::string& name) const {
    int32_t count = 0;
    for (const auto& c : calls) {
      if (c.name == name) ++count;
    }
    return count;
  }

  /// @brief Kayıtlı çağrıları ve sayaçları sıfırla.
  void Reset() {
    calls.clear();
    last_vertices.clear();
    last_textured_vertices.clear();
    create_texture_count = 0;
    destroy_texture_count = 0;
    update_texture_count = 0;
    update_texture_calls.clear();
    last_opacity = 1.0f;
    model_at_draw.clear();
    scissor_calls.clear();
    viewport_calls.clear();
    clear_scissor_count = 0;
    blend_calls.clear();
    last_filter = TextureFilter::kLinear;
    last_model = {{1, 0, 0, 0, 1, 0, 0, 0, 1}};
  }
};

}  // namespace sdl_painter
