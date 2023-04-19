#ifndef __PROFILER_H__
#define __PROFILER_H__

#include <time.h>

int ProfilerStart(const char* fname);

void ProfilerStop(void);

void ProfilerFlush(void);

void ProfilerEnable(void);
void ProfilerDisable(void);



#endif	//!__PROFILER_H__