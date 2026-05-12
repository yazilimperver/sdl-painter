#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "sdl_painter/color.h"
#include "sdl_painter/renderer.h"
#include "sdl_painter/vertex.h"

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

  // --- Sayaçlar ---
  int32_t create_texture_count{0};   ///< CreateTexture kaç kez çağrıldı
  int32_t destroy_texture_count{0};  ///< DestroyTexture kaç kez çağrıldı
  float last_opacity{1.0f};          ///< Son SetOpacity değeri

  // --- IRenderer arayüzü ---

  bool Initialize(SDL_Window*) override { return true; }
  void Shutdown() override {}
  void BeginFrame() override { calls.push_back({"BeginFrame"}); }
  void EndFrame() override { calls.push_back({"EndFrame"}); }

  void SetViewport(int32_t, int32_t, int32_t, int32_t) override {}
  void SetScissor(int32_t, int32_t, int32_t, int32_t) override {}
  void ClearScissor() override {}
  void Clear(const Color&) override { calls.push_back({"Clear"}); }

  void SetOpacity(float alpha) override {
    last_opacity = alpha;
    calls.push_back({"SetOpacity", 0, kInvalidTexture, alpha});
  }

  void DrawTriangles(const std::vector<Vertex>& vertices) override {
    calls.push_back({"DrawTriangles", vertices.size()});
    last_vertices = vertices;
  }

  /// @brief Her çağrıda artan benzersiz handle döner (1'den başlar).
  TextureHandle CreateTexture(const uint8_t*, int32_t, int32_t,
                              int32_t) override {
    ++create_texture_count;
    return static_cast<TextureHandle>(create_texture_count);
  }

  void DestroyTexture(TextureHandle handle) override {
    if (handle != kInvalidTexture) ++destroy_texture_count;
  }

  void DrawTextured(const std::vector<TexturedVertex>& vertices,
                    TextureHandle texture) override {
    calls.push_back({"DrawTextured", vertices.size(), texture});
    last_textured_vertices = vertices;
  }

  RendererBackend GetBackend() const override { return RendererBackend::kOpenGL; }
  void SetProjectionMatrix(const float*) override {}
  void SetModelMatrix(const float*) override {}

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
    last_opacity = 1.0f;
  }
};

}  // namespace sdl_painter
