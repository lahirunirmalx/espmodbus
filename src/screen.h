#ifndef __SCREEN_H__
#define __SCREEN_H__ 

#include <stdio.h>
#include <Arduino.h> 

void trim_string(char *str, const char *org, uint32_t length);  
void printMenuTitle(const char *top_menu);  
void printHomeScreen(const char *top_menu,uint32_t ch,uint32_t second_bar,uint32_t count);
std::string getBarString(uint32_t length) ;

#endif //__SCREEN_H__ 
