#pragma once

#include <array>
#include <cstdint>

/// @file
/// @brief Tic-tac-toe saf oyun mantigi — cizim ve SDL'den tamamen bagimsiz.
///
/// Bu baslik bilincli olarak hicbir sdl_painter tipine bagimli degildir:
/// boylece hem @c phase8_tictactoe demosu hem de birim testler ayni mantigi
/// kullanir. Demo yalnizca burayi cagirir ve sonucu cizer.

namespace tictactoe {

/// @brief Bir hucrenin icerigi.
enum class Cell : uint8_t {
  kEmpty = 0,
  kX,
  kO,
};

/// @brief Oyunun genel durumu.
enum class GameState : uint8_t {
  kPlaying = 0,  ///< Oyun suruyor.
  kWinX,         ///< X kazandi.
  kWinO,         ///< O kazandi.
  kDraw,         ///< Berabere (tahta doldu, kazanan yok).
};

/// @brief 3x3 tahta — satir onceligli (index = row * 3 + col).
using Board = std::array<Cell, 9>;

/// @brief Kazanan uclusunun tahta indeksleri; kazanan yoksa anlamsizdir.
struct WinLine {
  bool valid{false};
  int32_t a{0};
  int32_t b{0};
  int32_t c{0};
};

/// @brief Tum kazanma kombinasyonlari (3 satir, 3 sutun, 2 capraz).
constexpr std::array<std::array<int32_t, 3>, 8> kWinPatterns{{
    {0, 1, 2},
    {3, 4, 5},
    {6, 7, 8},  // satirlar
    {0, 3, 6},
    {1, 4, 7},
    {2, 5, 8},  // sutunlar
    {0, 4, 8},
    {2, 4, 6},  // caprazlar
}};

/// @brief Bos bir tahta uret.
constexpr Board EmptyBoard() {
  return Board{Cell::kEmpty, Cell::kEmpty, Cell::kEmpty,
               Cell::kEmpty, Cell::kEmpty, Cell::kEmpty,
               Cell::kEmpty, Cell::kEmpty, Cell::kEmpty};
}

/// @brief Verilen indeks tahta sinirlari icinde mi?
constexpr bool IsValidIndex(int32_t index) {
  return index >= 0 && index < 9;
}

/// @brief Kazanan uclusunu bul.
/// @return Kazanan varsa @c valid=true ve uclunun indeksleri; yoksa @c valid=false.
inline WinLine FindWinLine(const Board& board) {
  for (const auto& pattern : kWinPatterns) {
    const Cell first = board[static_cast<std::size_t>(pattern[0])];
    if (first == Cell::kEmpty) {
      continue;
    }
    if (board[static_cast<std::size_t>(pattern[1])] == first &&
        board[static_cast<std::size_t>(pattern[2])] == first) {
      return WinLine{true, pattern[0], pattern[1], pattern[2]};
    }
  }
  return WinLine{};
}

/// @brief Tahtada bos hucre kaldi mi?
inline bool HasEmptyCell(const Board& board) {
  for (const Cell cell : board) {
    if (cell == Cell::kEmpty) {
      return true;
    }
  }
  return false;
}

/// @brief Tahtanin guncel durumunu hesapla.
inline GameState EvaluateState(const Board& board) {
  const WinLine line = FindWinLine(board);
  if (line.valid) {
    return board[static_cast<std::size_t>(line.a)] == Cell::kX
               ? GameState::kWinX
               : GameState::kWinO;
  }
  return HasEmptyCell(board) ? GameState::kPlaying : GameState::kDraw;
}

/// @brief Hamle yapmayi dene.
///
/// Hamle yalnizca oyun surerken ve hedef hucre bosken gecerlidir.
///
/// @param board Uzerinde oynanacak tahta (basarida degistirilir).
/// @param index Hedef hucre indeksi [0, 9).
/// @param player Hamleyi yapan oyuncu (@c kEmpty gecersizdir).
/// @return Hamle uygulandiysa true.
inline bool TryPlaceMark(Board& board, int32_t index, Cell player) {
  if (player == Cell::kEmpty || !IsValidIndex(index)) {
    return false;
  }
  if (EvaluateState(board) != GameState::kPlaying) {
    return false;
  }
  if (board[static_cast<std::size_t>(index)] != Cell::kEmpty) {
    return false;
  }
  board[static_cast<std::size_t>(index)] = player;
  return true;
}

/// @brief Sirayi diger oyuncuya devret.
constexpr Cell NextPlayer(Cell current) {
  return current == Cell::kX ? Cell::kO : Cell::kX;
}

/// @brief Ekran koordinatini tahta hucresine cevir (hit testing).
///
/// Tahta, sol ust kosesi (@p origin_x, @p origin_y) olan ve kenari
/// @p board_size piksel olan bir karedir.
///
/// @return Nokta tahtanin icindeyse hucre indeksi [0, 9); disindaysa -1.
inline int32_t CellFromPoint(float px, float py, float origin_x, float origin_y,
                             float board_size) {
  if (board_size <= 0.0F) {
    return -1;
  }
  const float local_x = px - origin_x;
  const float local_y = py - origin_y;
  if (local_x < 0.0F || local_y < 0.0F || local_x >= board_size ||
      local_y >= board_size) {
    return -1;
  }
  const float cell_size = board_size / 3.0F;
  const auto col = static_cast<int32_t>(local_x / cell_size);
  const auto row = static_cast<int32_t>(local_y / cell_size);
  // Kenar durumunda (tam sinirda) tasmayi engelle.
  const int32_t clamped_col = col > 2 ? 2 : col;
  const int32_t clamped_row = row > 2 ? 2 : row;
  return clamped_row * 3 + clamped_col;
}

}  // namespace tictactoe
