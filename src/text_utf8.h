#pragma once

#include <cstddef>
#include <cstdint>

namespace sdl_painter::detail {

/// @brief UTF-8 dizisinden tek bir Unicode kod noktasını okur.
///
/// UTF-8, Unicode kod noktalarını 1-4 bayta kodlayan değişken uzunluklu bir
/// karakter kodlamasıdır. İlk baytın yüksek bitleri sequence uzunluğunu
/// belirler, sonraki continuation byte'lar `10xxxxxx` formatındadır.
///
/// Kodlama tablosu:
/// | Uzunluk | İlk bayt    | Continuation    | Kod noktası aralığı |
/// |---------|-------------|-----------------|---------------------|
/// | 1 bayt  | `0xxxxxxx`  | —               | U+0000..U+007F      |
/// | 2 bayt  | `110xxxxx`  | `10xxxxxx`      | U+0080..U+07FF      |
/// | 3 bayt  | `1110xxxx`  | `10xxxxxx` x2   | U+0800..U+FFFF      |
/// | 4 bayt  | `11110xxx`  | `10xxxxxx` x3   | U+10000..U+10FFFF   |
///
/// Kod noktası; ilk bayttan kalan payload bitleri ile her continuation
/// byte'ın alt 6 bitinin sola kaydırmalı birleşimi sonucu elde edilir.
///
/// Geçersiz girdide (kesik dizi, bozuk continuation byte, overlong kodlama,
/// surrogate aralığı, U+10FFFF üstü) tek bayt tüketilir ve U+FFFD döner —
/// böylece çağıran döngü daima ilerler ve sonsuz döngüye girmez.
///
/// @param str Okunacak baytların başlangıç adresi.
/// @param remaining Okunabilecek maksimum bayt sayısı (string'in kalan boyutu).
/// @param advance [out] Tüketilen bayt sayısı (geçerli → 1..4; geçersiz → 1).
/// @return Unicode kod noktası; geçersiz sequence → U+FFFD.
inline char32_t DecodeUTF8(const char* str, std::size_t remaining,
                           std::size_t& advance) {
  advance = 1;
  if (str == nullptr || remaining == 0) {
    return U'\uFFFD';
  }

  // Continuation byte kontrolü: `10xxxxxx` pattern'ı.
  auto is_cont = [](uint8_t byte) { return (byte & 0xC0) == 0x80; };

  const auto kB0 = static_cast<uint8_t>(str[0]);

  // 1-bayt sequence: 0xxxxxxx → ASCII (U+0000..U+007F)
  if (kB0 < 0x80) {
    return static_cast<char32_t>(kB0);
  }

  // 2-bayt sequence: 110xxxxx 10xxxxxx → U+0080..U+07FF
  if ((kB0 & 0xE0) == 0xC0 && remaining >= 2) {
    const auto kB1 = static_cast<uint8_t>(str[1]);
    if (is_cont(kB1)) {
      const auto kCp =
          static_cast<char32_t>(((kB0 & 0x1FU) << 6U) | (kB1 & 0x3FU));
      // Overlong: 2 baytla kodlanmış ASCII geçersizdir.
      if (kCp >= 0x80) {
        advance = 2;
        return kCp;
      }
    }
  }

  // 3-bayt sequence: 1110xxxx 10xxxxxx 10xxxxxx → U+0800..U+FFFF (BMP)
  if ((kB0 & 0xF0) == 0xE0 && remaining >= 3) {
    const auto kB1 = static_cast<uint8_t>(str[1]);
    const auto kB2 = static_cast<uint8_t>(str[2]);
    if (is_cont(kB1) && is_cont(kB2)) {
      const auto kCp = static_cast<char32_t>(
          ((kB0 & 0x0FU) << 12U) | ((kB1 & 0x3FU) << 6U) | (kB2 & 0x3FU));
      // Overlong kontrolü ve UTF-16 surrogate aralığı (U+D800..U+DFFF)
      // tek başına geçerli bir kod noktası değildir.
      if (kCp >= 0x800 && !(kCp >= 0xD800 && kCp <= 0xDFFF)) {
        advance = 3;
        return kCp;
      }
    }
  }

  // 4-bayt sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx → U+10000..U+10FFFF
  // (supplementary planes — emoji, nadir CJK karakterleri, vs.)
  if ((kB0 & 0xF8) == 0xF0 && remaining >= 4) {
    const auto kB1 = static_cast<uint8_t>(str[1]);
    const auto kB2 = static_cast<uint8_t>(str[2]);
    const auto kB3 = static_cast<uint8_t>(str[3]);
    if (is_cont(kB1) && is_cont(kB2) && is_cont(kB3)) {
      const auto kCp = static_cast<char32_t>(
          ((kB0 & 0x07U) << 18U) | ((kB1 & 0x3FU) << 12U) |
          ((kB2 & 0x3FU) << 6U) | (kB3 & 0x3FU));
      if (kCp >= 0x10000 && kCp <= 0x10FFFF) {
        advance = 4;
        return kCp;
      }
    }
  }

  // Geçersiz sequence (malformed, kesik veya overlong) — tek bayt atla,
  // replacement character döndür. Çağıran döngü sonraki byte'a geçer.
  return U'\uFFFD';
}

}  // namespace sdl_painter::detail
