/**
  ******************************************************************************
  * @file   : videoutils.h
  * @author : wind
  * @version: 0.0.1
  * @date   : 2017/01/14
  * @brief  :
  ******************************************************************************
  */

#ifndef __VIDEOUTILS_H
#define __VIDEOUTILS_H

// #include <stdlib.h>
#include <cstdlib>

bool yuyv_to_rgb(unsigned char *yuyv,unsigned char *rgb,int height,int width);
bool yuyv_to_bgr(unsigned char *yuyv,unsigned char *rgb,int height,int width);



#endif/*__VIDEOUTILS_H*/ 