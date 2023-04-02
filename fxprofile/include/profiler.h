#ifndef __PROFILER_H__
#define __PROFILER_H__

#include <time.h>

struct ProfilerState {
  int    enabled;             /* Is profiling currently enabled? */
  time_t start_time;          /* If enabled, when was profiling started? */
  char   profile_name[1024];  /* Name of profile file being written, or '\0' */
  int    samples_gathered;    /* Number of samples gathered so far (or 0) */
};

void test();

#endif	//!__PROFILER_H__