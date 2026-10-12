#define private public
#include "rps.h"
#include <cstdio>
static uint8_t beats2(uint8_t a,uint8_t b){ return a==b?0:(((a+1)%3==b)?2:1); }
int main(){ srand(3);
 const char* names[]={"random player","always rock","cycles R-P-S","rock 50% / else random","repeats winning move, switches after loss"};
 for(int style=0;style<5;style++){ printf("%s:\n",names[style]);
  for(int lv=0;lv<5;lv++){ rpsGame.level=lv; int aiW=0,huW=0,dr=0,N=4000;
   rpsGame.startSeries(); uint8_t last=0,lastRes=0; 
   for(int r=0;r<N;r++){ uint8_t human;
     if(style==0)human=rand()%3; else if(style==1)human=0; else if(style==2)human=r%3;
     else if(style==3)human=(rand()%2)?0:rand()%3; else human=(lastRes==1)?last:(rand()%3);   // lastRes 1 = human won
     uint8_t ai=rpsGame.aiChoice(); rpsGame.learn(human); uint8_t res=beats2(human,ai); lastRes=res; last=human;
     if(res==0)dr++; else if(res==1)huW++; else aiW++; }
   printf("  level %d: AI wins %2d%%, draws %2d%%, player wins %2d%%\n",lv,aiW*100/N,dr*100/N,huW*100/N);} } }
