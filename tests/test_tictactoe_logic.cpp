/// @file
/// @brief phase8_tictactoe demosunun saf oyun mantigi testleri.
///
/// Mantik cizimden bagimsiz oldugu icin (examples/tictactoe_logic.h) headless
/// olarak tam kapsamli test edilebilir.

#include <gtest/gtest.h>

#include "tictactoe_logic.h"

namespace ttt = tictactoe;

namespace {

/// @brief Verilen desene gore tahtayi doldur ('X', 'O', '.' = bos).
ttt::Board MakeBoard(const char (&pattern)[10]) {
  ttt::Board board = ttt::EmptyBoard();
  for (std::size_t i = 0; i < 9; ++i) {
    switch (pattern[i]) {
      case 'X':
        board[i] = ttt::Cell::kX;
        break;
      case 'O':
        board[i] = ttt::Cell::kO;
        break;
      default:
        board[i] = ttt::Cell::kEmpty;
        break;
    }
  }
  return board;
}

}  // namespace

// --- Baslangic durumu -------------------------------------------------------

TEST(TicTacToeBoardTest, EmptyBoardHasNoMarks) {
  const ttt::Board board = ttt::EmptyBoard();
  for (const ttt::Cell cell : board) {
    EXPECT_EQ(cell, ttt::Cell::kEmpty);
  }
}

TEST(TicTacToeBoardTest, EmptyBoardIsPlaying) {
  EXPECT_EQ(ttt::EvaluateState(ttt::EmptyBoard()), ttt::GameState::kPlaying);
}

TEST(TicTacToeBoardTest, EmptyBoardHasNoWinLine) {
  EXPECT_FALSE(ttt::FindWinLine(ttt::EmptyBoard()).valid);
}

// --- Kazanma desenleri ------------------------------------------------------

TEST(TicTacToeWinTest, DetectsEveryRow) {
  EXPECT_EQ(ttt::EvaluateState(MakeBoard("XXX.O.O..")), ttt::GameState::kWinX);
  EXPECT_EQ(ttt::EvaluateState(MakeBoard("O..XXX.O.")), ttt::GameState::kWinX);
  EXPECT_EQ(ttt::EvaluateState(MakeBoard(".O.O..XXX")), ttt::GameState::kWinX);
}

TEST(TicTacToeWinTest, DetectsEveryColumn) {
  EXPECT_EQ(ttt::EvaluateState(MakeBoard("O..O.XO..")), ttt::GameState::kWinO);
  EXPECT_EQ(ttt::EvaluateState(MakeBoard(".O..OX.OX")), ttt::GameState::kWinO);
  EXPECT_EQ(ttt::EvaluateState(MakeBoard("X.OX.O..O")), ttt::GameState::kWinO);
}

TEST(TicTacToeWinTest, DetectsBothDiagonals) {
  EXPECT_EQ(ttt::EvaluateState(MakeBoard("X.O.X.O.X")), ttt::GameState::kWinX);
  EXPECT_EQ(ttt::EvaluateState(MakeBoard("O.X.X.X.O")), ttt::GameState::kWinX);
}

TEST(TicTacToeWinTest, WinLineReportsWinningIndices) {
  const ttt::WinLine line = ttt::FindWinLine(MakeBoard("X.O.X.O.X"));
  ASSERT_TRUE(line.valid);
  EXPECT_EQ(line.a, 0);
  EXPECT_EQ(line.b, 4);
  EXPECT_EQ(line.c, 8);
}

TEST(TicTacToeWinTest, EmptyCellsDoNotFormAWin) {
  // Ust satir bos; bos hucreler ucluyu tamamlamis sayilmamali.
  EXPECT_FALSE(ttt::FindWinLine(MakeBoard("...X.O.XO")).valid);
}

// --- Berabere ---------------------------------------------------------------

TEST(TicTacToeDrawTest, FullBoardWithoutWinIsDraw) {
  EXPECT_EQ(ttt::EvaluateState(MakeBoard("XXOOOXXOX")), ttt::GameState::kDraw);
}

TEST(TicTacToeDrawTest, FullBoardWithWinIsNotDraw) {
  EXPECT_EQ(ttt::EvaluateState(MakeBoard("XXXOOXOXO")), ttt::GameState::kWinX);
}

TEST(TicTacToeDrawTest, PartialBoardIsNotDraw) {
  EXPECT_EQ(ttt::EvaluateState(MakeBoard("XO.XO....")),
            ttt::GameState::kPlaying);
}

// --- Hamle kurallari --------------------------------------------------------

TEST(TicTacToeMoveTest, PlacesMarkOnEmptyCell) {
  ttt::Board board = ttt::EmptyBoard();
  EXPECT_TRUE(ttt::TryPlaceMark(board, 4, ttt::Cell::kX));
  EXPECT_EQ(board[4], ttt::Cell::kX);
}

TEST(TicTacToeMoveTest, RejectsOccupiedCell) {
  ttt::Board board = ttt::EmptyBoard();
  ASSERT_TRUE(ttt::TryPlaceMark(board, 0, ttt::Cell::kX));
  EXPECT_FALSE(ttt::TryPlaceMark(board, 0, ttt::Cell::kO));
  EXPECT_EQ(board[0], ttt::Cell::kX);  // degismemeli
}

TEST(TicTacToeMoveTest, RejectsOutOfRangeIndex) {
  ttt::Board board = ttt::EmptyBoard();
  EXPECT_FALSE(ttt::TryPlaceMark(board, -1, ttt::Cell::kX));
  EXPECT_FALSE(ttt::TryPlaceMark(board, 9, ttt::Cell::kX));
  EXPECT_FALSE(ttt::TryPlaceMark(board, 100, ttt::Cell::kX));
}

TEST(TicTacToeMoveTest, RejectsEmptyAsPlayer) {
  ttt::Board board = ttt::EmptyBoard();
  EXPECT_FALSE(ttt::TryPlaceMark(board, 0, ttt::Cell::kEmpty));
}

TEST(TicTacToeMoveTest, RejectsMoveAfterGameEnded) {
  ttt::Board board = MakeBoard("XXX.O.O..");
  ASSERT_EQ(ttt::EvaluateState(board), ttt::GameState::kWinX);
  EXPECT_FALSE(ttt::TryPlaceMark(board, 3, ttt::Cell::kO));
  EXPECT_EQ(board[3], ttt::Cell::kEmpty);
}

TEST(TicTacToeMoveTest, NextPlayerAlternates) {
  EXPECT_EQ(ttt::NextPlayer(ttt::Cell::kX), ttt::Cell::kO);
  EXPECT_EQ(ttt::NextPlayer(ttt::Cell::kO), ttt::Cell::kX);
}

// --- Hit testing ------------------------------------------------------------

TEST(TicTacToeHitTest, MapsCornersAndCenterToCells) {
  // Tahta: (100,100) kosesinde, 300 piksel kenar → hucre 100 piksel.
  EXPECT_EQ(ttt::CellFromPoint(150.0F, 150.0F, 100.0F, 100.0F, 300.0F), 0);
  EXPECT_EQ(ttt::CellFromPoint(250.0F, 150.0F, 100.0F, 100.0F, 300.0F), 1);
  EXPECT_EQ(ttt::CellFromPoint(350.0F, 150.0F, 100.0F, 100.0F, 300.0F), 2);
  EXPECT_EQ(ttt::CellFromPoint(250.0F, 250.0F, 100.0F, 100.0F, 300.0F), 4);
  EXPECT_EQ(ttt::CellFromPoint(350.0F, 350.0F, 100.0F, 100.0F, 300.0F), 8);
}

TEST(TicTacToeHitTest, ReturnsMinusOneOutsideBoard) {
  EXPECT_EQ(ttt::CellFromPoint(50.0F, 150.0F, 100.0F, 100.0F, 300.0F), -1);
  EXPECT_EQ(ttt::CellFromPoint(150.0F, 50.0F, 100.0F, 100.0F, 300.0F), -1);
  EXPECT_EQ(ttt::CellFromPoint(450.0F, 150.0F, 100.0F, 100.0F, 300.0F), -1);
  EXPECT_EQ(ttt::CellFromPoint(150.0F, 450.0F, 100.0F, 100.0F, 300.0F), -1);
}

TEST(TicTacToeHitTest, TopLeftEdgeIsInsideBoard) {
  EXPECT_EQ(ttt::CellFromPoint(100.0F, 100.0F, 100.0F, 100.0F, 300.0F), 0);
}

TEST(TicTacToeHitTest, BottomRightEdgeIsOutsideBoard) {
  // Sag/alt sinir haric (yari acik aralik) — tasma olmamali.
  EXPECT_EQ(ttt::CellFromPoint(400.0F, 400.0F, 100.0F, 100.0F, 300.0F), -1);
}

TEST(TicTacToeHitTest, HandlesZeroSizedBoard) {
  EXPECT_EQ(ttt::CellFromPoint(0.0F, 0.0F, 0.0F, 0.0F, 0.0F), -1);
}

TEST(TicTacToeHitTest, WorksWithNonIntegerBoardSize) {
  // Duyarli yerlesimde tahta boyutu tam sayi olmayabilir.
  const float size = 301.0F;
  EXPECT_EQ(ttt::CellFromPoint(100.0F, 100.0F, 100.0F, 100.0F, size), 0);
  EXPECT_EQ(ttt::CellFromPoint(400.9F, 400.9F, 100.0F, 100.0F, size), 8);
}

// --- Tam oyun akisi ---------------------------------------------------------

TEST(TicTacToeFlowTest, PlaysOutToXWin) {
  ttt::Board board = ttt::EmptyBoard();
  ttt::Cell player = ttt::Cell::kX;

  // X: 0, 1, 2  |  O: 3, 4
  const int32_t moves[] = {0, 3, 1, 4, 2};
  for (const int32_t move : moves) {
    ASSERT_TRUE(ttt::TryPlaceMark(board, move, player))
        << "hamle basarisiz: " << move;
    if (ttt::EvaluateState(board) == ttt::GameState::kPlaying) {
      player = ttt::NextPlayer(player);
    }
  }

  EXPECT_EQ(ttt::EvaluateState(board), ttt::GameState::kWinX);
  const ttt::WinLine line = ttt::FindWinLine(board);
  ASSERT_TRUE(line.valid);
  EXPECT_EQ(line.a, 0);
  EXPECT_EQ(line.c, 2);
}
