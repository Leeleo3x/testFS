#ifndef __LOGGING_H__
#define __LOGGING_H__


#define DEBUG 1

#ifdef DEBUG

#include <stdio.h>
#define LOG(...) (fprintf(stderr, __VA_ARGS__))

#else

#define LOG(...)

#endif


#endif // __LOGGING_H__
