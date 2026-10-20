#define private public
#include "tictactoe.h"
#include <cstdio>
#include <cstdlib>

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); fails++; } } while (0)

// The player on the other side of the computer (X, mark 1): a random player, or a careful one who
// takes a win, blocks, then likes the centre and the corners, like most people do after a few games
static uint8_t randomPlayer(TicTacToeGame& t) {
  uint8_t e[9], n = 0;
  for (int i = 0; i < 9; i++) if (!t.board[i]) e[n++] = i;
  return e[rand() % n];
}
static uint8_t carefulPlayer(TicTacToeGame& t) {
  for (uint8_t mark : {1, 2}) {                 // win first, then block
    for (int i = 0; i < 9; i++) {
      if (t.board[i]) continue;
      t.board[i] = mark;
      bool win = t.winnerOf(t.board, nullptr) == mark;
      t.board[i] = 0;
      if (win) return i;
    }
  }
  if (!t.board[4]) return 4;
  uint8_t c[4], n = 0;
  for (uint8_t k : {0, 2, 6, 8}) if (!t.board[k]) c[n++] = k;
  if (n) return c[rand() % n];
  return randomPlayer(t);
}

// Plays N rounds against the computer at `level`; returns the percentages the computer wins and loses
static void rounds(int level, bool careful, int N, int& aiWins, int& aiLoses) {
  auto& t = ticTacToeGame;
  t.level = level;
  int aw = 0, hw = 0;
  for (int g = 0; g < N; g++) {
    memset(t.board, 0, 9); t.roundResult = 0; t.turn = (g % 2) ? 2 : 1; t.ptsX = t.ptsO = 0;
    while (!t.roundResult) {
      if (t.turn == 1) t.place(careful ? carefulPlayer(t) : randomPlayer(t), 1);
      else t.place(t.chooseAiMove(), 2);
    }
    if (t.roundResult == 2) aw++; else if (t.roundResult == 1) hw++;
  }
  aiWins = aw * 100 / N;
  aiLoses = hw * 100 / N;
}

int main() {
  srand(7);
  const int N = 150;
  int randWin[5], randLose[5], carefulWin[5], carefulLose[5];
  for (int lv = 0; lv < 5; lv++) {
    rounds(lv, false, N, randWin[lv], randLose[lv]);
    rounds(lv, true, N, carefulWin[lv], carefulLose[lv]);
    printf("level %d: against a random player the computer wins %2d%% and loses %2d%%; against a careful one it wins %2d%% and loses %2d%%\n",
           lv, randWin[lv], randLose[lv], carefulWin[lv], carefulLose[lv]);
  }

  // The easy levels are really easy: an ordinary careful player beats them most of the time
  CHECK(carefulLose[0] >= 70, "EASY: a careful player wins at least 70% of the rounds");
  CHECK(carefulLose[1] >= 60, "NORMAL: a careful player wins at least 60% of the rounds");
  CHECK(carefulLose[2] >= 40, "HARD: a careful player still wins at least 40% of the rounds");
  CHECK(carefulLose[3] >= 10, "EXPERT: a careful player wins now and then");
  // EASY plays at random, so it does about as well as a random player does against another one (the
  // starter alternates, so that is a little under half the rounds)
  CHECK(randWin[0] <= 50, "EASY: the computer wins no more than half the rounds against a random player");
  // Each level is at least as strong as the one below it
  // (Two neighbouring levels can differ by chance alone, and rand() differs between systems, so the slack is generous.)
  for (int lv = 1; lv < 5; lv++) {
    CHECK(carefulLose[lv] <= carefulLose[lv - 1] + 10, "a higher level is never much easier to beat (careful player)");
    CHECK(randWin[lv] >= randWin[lv - 1] - 10, "a higher level never wins much less (random player)");
  }
  // INSANE never loses
  CHECK(randLose[4] == 0 && carefulLose[4] == 0, "INSANE never loses");

  printf("%s (%d failures)\n", fails ? "SOME CHECKS FAILED" : "all tic-tac-toe checks passed", fails);
  return fails != 0;
}
