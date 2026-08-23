#pragma once

#include <cstdint>

namespace sdl_painter {

/// @brief Bir karenin çizim maliyeti — profilleme ve ekran üstü gösterim için.
///
/// @ref Painter her karede doldurur; @ref Painter::GetFrameStats ile okunur.
/// Değerler **son tamamlanan** kareye aittir (`End()` çağrıldıktan sonra
/// geçerlidir).
///
/// Bu sayaçların neden önemli olduğu `examples/benchmarks/README.md` içinde
/// ölçümle anlatılır: aynı görüntüyü üreten iki çizim deseni arasında
/// @ref draw_calls farkı 20 kata varan kare süresi farkı yaratabiliyor.
struct FrameStats {
  /// @brief `Begin()`–`End()` arası CPU süresi (milisaniye).
  double cpu_frame_ms{0.0};

  /// @brief Karenin GPU süresi (milisaniye); ölçülemiyorsa 0.
  ///
  /// OpenGL backend'de `GL_TIME_ELAPSED` timer query ile ölçülür. Sonuç bir
  /// kare gecikmeli okunur (senkronizasyon duraklaması yaratmamak için), yani
  /// gösterilen değer bir önceki kareye aittir. Vulkan backend'de şu an
  /// ölçülmez ve 0 kalır.
  double gpu_frame_ms{0.0};

  /// @brief `IRenderer`'a giden çizim çağrısı sayısı
  ///        (`DrawTriangles` + `DrawTextured`).
  uint32_t draw_calls{0};

  /// @brief Geometri üreten batch (flush) sayısı.
  ///
  /// @note Bugünkü tasarımda her flush tam olarak bir çizim çağrısı ürettiği
  ///       için @ref draw_calls ile aynıdır. Ayrı tutulur, çünkü ikisi farklı
  ///       katmanı ölçer: biri batcher'ın, diğeri backend'in davranışıdır.
  uint32_t batches{0};

  /// @brief GPU'ya gönderilen toplam vertex sayısı.
  uint32_t vertices{0};

  /// @brief Batch'lenemeyen GPU durum değişikliği sayısı.
  ///
  /// Scissor (clip) ayarları, projeksiyon ve model matrisi yüklemeleri.
  /// Renk ve transform bu sayaca **girmez** — ikisi de vertex verisinde
  /// taşındığı için durum değişikliği değildir.
  uint32_t state_changes{0};
};

}  // namespace sdl_painter
