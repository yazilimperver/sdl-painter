/// @file test_text_utf8.cpp
/// @brief UTF-8 çözümleyici testleri (Painter::DrawText'in metin yolu).
///
/// @note Kod noktaları ve bayt dizileri bilinçli olarak **escape dizisiyle**
///       yazılmıştır (`U'\u00E7'`, `"\xC3\xA7"`). Proje kaynakları BOM'suz
///       UTF-8'dir ve MSVC'ye `/utf-8` verilmediğinden, kaynak içine gömülen
///       ham çok baytlı karakter sabitleri "too many characters in constant"
///       hatası üretir. Escape kullanmak hem taşınabilir hem de bir UTF-8
///       çözümleyici testi için daha açık: hangi baytın hangi kod noktasına
///       karşılık geldiği doğrudan okunur.

#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "text_utf8.h"

using sdl_painter::detail::DecodeUTF8;

namespace {

constexpr char32_t kReplacement = U'\uFFFD';

/// @brief Tüm dizeyi çözüp kod noktası listesi döndürür.
std::vector<char32_t> DecodeAll(const std::string& s) {
  std::vector<char32_t> out;
  std::size_t i = 0;
  while (i < s.size()) {
    std::size_t advance = 0;
    out.push_back(DecodeUTF8(s.c_str() + i, s.size() - i, advance));
    // Sonsuz döngü koruması: çözümleyici daima ilerlemeli.
    EXPECT_GT(advance, 0u);
    if (advance == 0) {
      break;
    }
    i += advance;
  }
  return out;
}

}  // namespace

// ─── Geçerli diziler ────────────────────────────────────────────────────────

TEST(DecodeUTF8, AsciiSingleByte) {
  std::size_t adv = 0;
  EXPECT_EQ(DecodeUTF8("A", 1, adv), U'A');
  EXPECT_EQ(adv, 1u);
}

TEST(DecodeUTF8, TwoByteSequence) {
  // U+00E7 LATIN SMALL LETTER C WITH CEDILLA ('ç') = C3 A7
  std::size_t adv = 0;
  EXPECT_EQ(DecodeUTF8("\xC3\xA7", 2, adv), U'\u00E7');
  EXPECT_EQ(adv, 2u);
}

TEST(DecodeUTF8, TwoByteSequenceUpperBound) {
  // U+07FF, 2 baytla kodlanabilen en büyük kod noktası = DF BF
  std::size_t adv = 0;
  EXPECT_EQ(DecodeUTF8("\xDF\xBF", 2, adv), U'\u07FF');
  EXPECT_EQ(adv, 2u);
}

TEST(DecodeUTF8, ThreeByteSequence) {
  // U+20AC EURO SIGN = E2 82 AC
  std::size_t adv = 0;
  EXPECT_EQ(DecodeUTF8("\xE2\x82\xAC", 3, adv), U'\u20AC');
  EXPECT_EQ(adv, 3u);
}

TEST(DecodeUTF8, FourByteSequence) {
  // U+1F600 GRINNING FACE = F0 9F 98 80
  std::size_t adv = 0;
  EXPECT_EQ(DecodeUTF8("\xF0\x9F\x98\x80", 4, adv), U'\U0001F600');
  EXPECT_EQ(adv, 4u);
}

TEST(DecodeUTF8, FourByteSequenceUpperBound) {
  // U+10FFFF, Unicode'un en büyük kod noktası = F4 8F BF BF
  std::size_t adv = 0;
  EXPECT_EQ(DecodeUTF8("\xF4\x8F\xBF\xBF", 4, adv), U'\U0010FFFF');
  EXPECT_EQ(adv, 4u);
}

TEST(DecodeUTF8, TurkishCharactersDecodeCorrectly) {
  // "Ğüşİöç" — projenin hedef dili; hepsi 2 baytlık diziler.
  //  Ğ=U+011E(C4 9E) ü=U+00FC(C3 BC) ş=U+015F(C5 9F)
  //  İ=U+0130(C4 B0) ö=U+00F6(C3 B6) ç=U+00E7(C3 A7)
  const std::vector<char32_t> kExpected = {U'\u011E', U'\u00FC', U'\u015F',
                                           U'\u0130', U'\u00F6', U'\u00E7'};
  EXPECT_EQ(DecodeAll("\xC4\x9E\xC3\xBC\xC5\x9F\xC4\xB0\xC3\xB6\xC3\xA7"),
            kExpected);
}

TEST(DecodeUTF8, MixedAsciiAndMultibyte) {
  // "a" + ç + "b" + € + "c"
  auto v = DecodeAll(
      "a\xC3\xA7"
      "b\xE2\x82\xAC"
      "c");
  ASSERT_EQ(v.size(), 5u);
  EXPECT_EQ(v[0], U'a');
  EXPECT_EQ(v[1], U'\u00E7');
  EXPECT_EQ(v[2], U'b');
  EXPECT_EQ(v[3], U'\u20AC');
  EXPECT_EQ(v[4], U'c');
}

// ─── Bozuk girdiler — daima ilerlemeli, asla çökmemeli ──────────────────────

TEST(DecodeUTF8, EmptyInputYieldsReplacementAndAdvancesOne) {
  std::size_t adv = 0;
  EXPECT_EQ(DecodeUTF8("", 0, adv), kReplacement);
  EXPECT_EQ(adv, 1u);
}

TEST(DecodeUTF8, NullPointerIsSafe) {
  std::size_t adv = 0;
  EXPECT_EQ(DecodeUTF8(nullptr, 5, adv), kReplacement);
  EXPECT_EQ(adv, 1u);
}

TEST(DecodeUTF8, TruncatedTwoByteSequence) {
  // C3 tek başına: devamı yok.
  std::size_t adv = 0;
  EXPECT_EQ(DecodeUTF8("\xC3", 1, adv), kReplacement);
  EXPECT_EQ(adv, 1u) << "Bozuk dizide tek bayt tüketilmeli.";
}

TEST(DecodeUTF8, TruncatedFourByteSequence) {
  std::size_t adv = 0;
  EXPECT_EQ(DecodeUTF8("\xF0\x9F", 2, adv), kReplacement);
  EXPECT_EQ(adv, 1u);
}

TEST(DecodeUTF8, BadContinuationByte) {
  // C3 arkasından continuation olmayan bir bayt (0x41 = 'A').
  std::size_t adv = 0;
  EXPECT_EQ(DecodeUTF8("\xC3\x41", 2, adv), kReplacement);
  EXPECT_EQ(adv, 1u);
}

TEST(DecodeUTF8, LoneContinuationByte) {
  std::size_t adv = 0;
  EXPECT_EQ(DecodeUTF8("\x80", 1, adv), kReplacement);
  EXPECT_EQ(adv, 1u);
}

TEST(DecodeUTF8, OverlongTwoByteEncodingRejected) {
  // C0 80 = overlong kodlanmış U+0000. Güvenlik açısından reddedilmeli:
  // aksi halde "\0" filtrelerini atlatan bir kodlama kabul edilirdi.
  std::size_t adv = 0;
  EXPECT_EQ(DecodeUTF8("\xC0\x80", 2, adv), kReplacement);
  EXPECT_EQ(adv, 1u);
}

TEST(DecodeUTF8, OverlongThreeByteEncodingRejected) {
  // E0 80 80 = overlong U+0000.
  std::size_t adv = 0;
  EXPECT_EQ(DecodeUTF8("\xE0\x80\x80", 3, adv), kReplacement);
  EXPECT_EQ(adv, 1u);
}

TEST(DecodeUTF8, SurrogateRangeRejected) {
  // ED A0 80 = U+D800, UTF-16 surrogate — tek başına geçerli değil.
  std::size_t adv = 0;
  EXPECT_EQ(DecodeUTF8("\xED\xA0\x80", 3, adv), kReplacement);
  EXPECT_EQ(adv, 1u);
}

TEST(DecodeUTF8, AboveMaxCodepointRejected) {
  // F5 80 80 80 -> U+140000 > U+10FFFF.
  std::size_t adv = 0;
  EXPECT_EQ(DecodeUTF8("\xF5\x80\x80\x80", 4, adv), kReplacement);
  EXPECT_EQ(adv, 1u);
}

TEST(DecodeUTF8, GarbageInputTerminatesAndKeepsValidChars) {
  // Bozuk baytlar arasındaki geçerli karakterler korunmalı; döngü bitmeli.
  auto v = DecodeAll(
      "a\xFF\xFE"
      "b");
  ASSERT_EQ(v.size(), 4u);
  EXPECT_EQ(v[0], U'a');
  EXPECT_EQ(v[1], kReplacement);
  EXPECT_EQ(v[2], kReplacement);
  EXPECT_EQ(v[3], U'b');
}

TEST(DecodeUTF8, AllSingleBytesTerminate) {
  // 0x00..0xFF arasındaki her bayt tek başına verildiğinde advance > 0 olmalı;
  // aksi halde DrawText döngüsü sonsuza girer.
  for (int b = 0; b <= 0xFF; ++b) {
    const char kByte = static_cast<char>(b);
    std::size_t adv = 0;
    (void)DecodeUTF8(&kByte, 1, adv);
    EXPECT_GT(adv, 0u) << "bayt 0x" << std::hex << b << " ilerlemedi";
  }
}
