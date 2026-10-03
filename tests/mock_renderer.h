#pragma once

#include "sdl_painter/color.h"
#include "sdl_painter/renderer.h"
#include "sdl_painter/vertex.h"

#include <array>
#include <cstdint>
#include <gtest/gtest.h>
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

  /// @brief Bir CreateTexture çağrısının kaydı.
  struct CreateCall {
    int32_t w{0};
    int32_t h{0};
    int32_t channels{0};
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

  /// @brief Son CreateTexture çağrısının boyutu ve kanal sayısı.
  ///
  /// Başarısız olan (bkz. @ref fail_next_create_texture) çağrı da kaydedilir.
  CreateCall last_create_call;

  /// @brief DestroyTexture'a verilen geçerli tanımlayıcılar (sırayla).
  ///
  /// Aynı tanımlayıcının iki kez görünmesi çift silmedir. Mock, mock'un
  /// kendisinin üretmediği tanımlayıcıları da kaydeder: Texture testleri
  /// doğrudan uydurma tanımlayıcı verir.
  std::vector<TextureHandle> destroyed_textures;

  /// @brief Bir sonraki CreateTexture çağrısı başarısız olsun (test girdisi).
  ///
  /// Çağrı kInvalidTexture döner ve bayrak kendiliğinden kapanır; sonraki
  /// çağrılar normal çalışır. Tanımlayıcı üretmez.
  bool fail_next_create_texture{false};

  /// @brief Son Clear çağrısının rengi (hiç çağrılmadıysa varsayılan renk).
  Color last_clear_color;

  /// @brief Sözleşmeyi bozan çizim çağrısı sayısı.
  ///
  /// DrawTriangles ve DrawTextured, vertex sayısı 3'ün katı olmayan bir
  /// liste alırsa artar. Sözleşme renderer.h'de yalnızca DrawTriangles için
  /// yazılı; DrawTextured için örtüktür (üçgen listesi). Painter'ı süren
  /// her test bu denetimi kendiliğinden alır.
  int32_t contract_violation_count{0};

  /// @brief İhlalde ayrıca ADD_FAILURE() çağrılsın mı (test girdisi).
  ///
  /// Varsayılan açık: ihlal yapan her test düşer. Yalnızca mock'un kendi
  /// denetimini sınayan test kapatır.
  bool report_contract_violations{true};

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
  void Clear(const Color& color) override {
    last_clear_color = color;
    calls.push_back({"Clear"});
  }

  void SetOpacity(float alpha) override {
    last_opacity = alpha;
    calls.push_back({"SetOpacity", 0, kInvalidTexture, alpha});
  }

  void SetBlendMode(BlendMode mode) override {
    blend_calls.push_back(mode);
    calls.push_back({"SetBlendMode"});
  }

  void DrawTriangles(const std::vector<Vertex>& vertices) override {
    CheckTriangleList("DrawTriangles", vertices.size());
    calls.push_back({"DrawTriangles", vertices.size()});
    last_vertices = vertices;
    model_at_draw.push_back(last_model);
  }

  /// @brief Her başarılı çağrıda artan benzersiz handle döner (1'den başlar).
  ///
  /// Handle sayacı Reset() ile sıfırlanmaz; Reset sonrası üretilen handle'lar
  /// canlı dokularınkiyle çakışmaz. create_texture_count başarısız çağrıları
  /// da sayar.
  TextureHandle CreateTexture(const uint8_t*, int32_t w, int32_t h,
                              int32_t channels) override {
    ++create_texture_count;
    last_create_call = {w, h, channels};
    if (fail_next_create_texture) {
      fail_next_create_texture = false;
      return kInvalidTexture;
    }
    return mNextTextureHandle++;
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
    if (handle != kInvalidTexture) {
      ++destroy_texture_count;
      destroyed_textures.push_back(handle);
    }
  }

  void DrawTextured(const std::vector<TexturedVertex>& vertices,
                    TextureHandle texture) override {
    CheckTriangleList("DrawTextured", vertices.size());
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
      if (c.name == name)
        ++count;
    }
    return count;
  }

  /// @brief Kaydedilen her gözlemi ilk durumuna döndür.
  ///
  /// Çağrı geçmişi, sayaçlar ve "son değer" alanlarının hepsi sıfırlanır
  /// (last_model ve last_projection dahil). İki grup bilerek dışarıda kalır:
  ///  - Doku tanımlayıcı sayacı: Reset, Painter yaşarken çağrılabilir; sayaç
  ///    dönseydi yeni dokular canlı olanlarla aynı tanımlayıcıyı alırdı.
  ///  - Testin kurduğu yapılandırma: backend, fail_next_create_texture,
  ///    report_contract_violations.
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
    last_projection = {};
    last_create_call = {};
    destroyed_textures.clear();
    last_clear_color = Color();
    contract_violation_count = 0;
  }

 private:
  /// @brief Üçgen listesi sözleşmesini (vertex sayısı 3'ün katı) denetle.
  void CheckTriangleList(const char* call, std::size_t vertex_count) {
    if (vertex_count % 3 == 0) {
      return;
    }
    ++contract_violation_count;
    if (report_contract_violations) {
      ADD_FAILURE() << call << ": vertex sayisi 3'un kati degil ("
                    << vertex_count << ")";
    }
  }

  TextureHandle mNextTextureHandle{1};
};

}  // namespace sdl_painter
