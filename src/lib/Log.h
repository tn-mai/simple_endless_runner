/**
* @file Log.h
*/
#ifndef EASYLIB_LOG_H
#define EASYLIB_LOG_H
#include <stdio.h>
#include <Windows.h>

#define LOG(...) { \
  char log_buf__[1024]; \
  snprintf(log_buf__, 1024, __VA_ARGS__); \
  fputs(log_buf__, stderr); \
  OutputDebugStringA(log_buf__); \
} (void)0

#endif // EASYLIB_LOG_H