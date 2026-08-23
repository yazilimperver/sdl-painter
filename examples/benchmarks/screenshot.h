#pragma once

/// @file screenshot.h
/// @brief Arka tamponu PNG olarak kaydeden yardımcı (yalnızca OpenGL modu).
///
/// Kütüphane bir ekran görüntüsü API'si sunmuyor ve bu ölçüm aracı için bir
/// tane eklemeye değmez. Bunun yerine `glReadPixels` doğrudan
/// `SDL_GL_GetProcAddress` ile çözülür — GL 1.0 fonksiyonu olduğu için
/// Windows'ta `wglGetProcAddress` döndürmez, ama SDL `opengl32.dll`'e geri
/// düşerek doğru adresi verir.
///
/// Çağrı anı önemlidir: `SDL_GL_SwapWindow`'dan **önce** okunmalı. Bunu
/// @ref bench::CountingRenderer::CaptureBeforeNextPresent sağlar.

#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace bench {

/// @brief Renk tamponunu okuyup PNG olarak yaz.
/// @param path Hedef dosya yolu (.png).
/// @param width  Okunacak genişlik (piksel).
/// @param height Okunacak yükseklik (piksel).
/// @param front_buffer `true` ise ÖN tampon okunur — yani sunulmuş (swap
///        edilmiş) son kare. `Application` tabanlı uygulamalarda sunumdan
///        önceye girilecek bir kanca olmadığı için tek yol budur.
/// @return Başarıda `true`.
bool SaveBackBufferPng(const std::string& path, int32_t width, int32_t height,
                       bool front_buffer = false);

}  // namespace bench
