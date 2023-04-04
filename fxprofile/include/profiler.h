#ifndef __PROFILER_H__
#define __PROFILER_H__

#include <time.h>

int ProfilerStart(const char* fname);

//int ProfilerStartWithOptions(
//    const char* fname, const struct ProfilerOptions* options);

void ProfilerStop(void);

void ProfilerFlush(void);

void ProfilerEnable(void);
void ProfilerDisable(void);

//int ProfilingIsEnabledForAllThreads(void);

//void ProfilerRegisterThread(void);

void test();

#endif	//!__PROFILER_H__