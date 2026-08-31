#include "weathernum.h"

#include <TJpg_Decoder.h>
//int numx;
//int numy;
//int numw;

//显示天气图标
void WeatherNum::printfweather(int numx,int numy,int numw)
{
  if(numw==00)
  {
    TJpgDec.drawJpg(numx,numy,t0, sizeof(t0));
  }
  else if(numw==01)
  {
    TJpgDec.drawJpg(numx,numy,t1, sizeof(t1));
  }
  else if(numw==02)
  {
    TJpgDec.drawJpg(numx,numy,t2, sizeof(t2));
  }
  else if(numw==03)
  {
    TJpgDec.drawJpg(numx,numy,t3, sizeof(t3));
  }
  else if(numw==04)
  {
    TJpgDec.drawJpg(numx,numy,t4, sizeof(t4));
  }
  else if(numw==07||numw==8||numw==21||numw==22)
  {
    TJpgDec.drawJpg(numx,numy,t7, sizeof(t7));
  }
  else if(numw==9||numw==10||numw==23||numw==24)
  {
    TJpgDec.drawJpg(numx,numy,t9, sizeof(t9));
  }
  else if(numw==14||numw==26)
  {
    TJpgDec.drawJpg(numx,numy,t14, sizeof(t14));
  }
  else if(numw==18)
  {
    TJpgDec.drawJpg(numx,numy,t18, sizeof(t18));
  }
  else
  {
    TJpgDec.drawJpg(numx,numy,t99, sizeof(t99));
  }

  
}
