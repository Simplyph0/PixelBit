#define private public
#include "tictactoe.h"
#include <cstdio>
int main(){ srand(7);
 for(int lv=0;lv<5;lv++){ ticTacToeGame.level=lv; int aiW=0,huW=0,dr=0,N=3000;
  for(int g=0;g<N;g++){ auto&t=ticTacToeGame; memset(t.board,0,9); t.roundResult=0; t.turn=(g%2)?2:1; t.ptsX=t.ptsO=0;
   while(!t.roundResult){ if(t.turn==1){ uint8_t e[9],n=0; for(int i=0;i<9;i++)if(!t.board[i])e[n++]=i; t.place(e[rand()%n],1);} else t.place(t.chooseAiMove(),2); }
   if(t.roundResult==2)aiW++; else if(t.roundResult==1)huW++; else dr++; }
  printf("level %d vs random player: AI wins %2d%%  draws %2d%%  AI loses %2d%%\n",lv,aiW*100/N,dr*100/N,huW*100/N);} }
