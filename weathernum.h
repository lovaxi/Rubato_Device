#ifndef WEATHERNUM_H
#define WEATHERNUM_H

#include <TFT_eSPI.h> 

#include "img/tianqi/t0.h"
#include "img/tianqi/t1.h"
#include "img/tianqi/t2.h"
#include "img/tianqi/t3.h"
#include "img/tianqi/t4.h"
#include "img/tianqi/t7.h"
#include "img/tianqi/t9.h"
#include "img/tianqi/t14.h"
#include "img/tianqi/t18.h"
#include "img/tianqi/t99.h"


class WeatherNum
{
private:


public:
  void printfweather(int numx,int numy,int numw);
};


#endif
