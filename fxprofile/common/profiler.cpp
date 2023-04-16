#include "../include/profiler.h"
#include "profiledata.h"

#include <string.h>
#ifdef _WIN32
#else
#include <ucontext.h>
#include <unwind.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#endif // _WIN32

static const int kMaxStackDepth = 254; // Max stack depth stored in profile

#ifdef _WIN32
typedef void (*ProfileHandlerCallback)();
#else
typedef void (*ProfileHandlerCallback)(int sig, siginfo_t* sig_info,
                                       void* ucontext, void* callback_arg);
#endif	//! _WIN32

struct ProfilerState {
	int    enabled;             /* Is profiling currently enabled? */
	time_t start_time;          /* If enabled, when was profiling started? */
	char   profile_name[1024];  /* Name of profile file being written, or '\0' */
	int    samples_gathered;    /* Number of samples gathered so far (or 0) */
};

struct ProfileHandlerToken {
  // Sets the callback and associated arg.
  ProfileHandlerToken(ProfileHandlerCallback cb, void* cb_arg)
      : callback(cb),
        callback_arg(cb_arg) {
  }

  // Callback function to be invoked on receiving a profile timer interrupt.
  ProfileHandlerCallback callback;
  // Argument for the callback function.
  void* callback_arg;
};

class CpuProfiler {
 public:
  CpuProfiler();
  ~CpuProfiler(){};

  bool Start(const char* fname, int frequency = 4000);

  void Stop();

  // Write the data to disk (and continue profiling).
  void FlushTable(){};

  bool Enabled(){return false;};

  void GetCurrentState(ProfilerState* state){};

  static CpuProfiler instance_;

//  private:
  ProfileData   collector_;

  int           (*filter_)(void*);
  void*         filter_arg_;

  ProfileHandlerToken* prof_handler_token_;

  int frequency_;

  // Sets up a callback to receive SIGPROF interrupt.
  void EnableHandler();

  // Disables receiving SIGPROF interrupt.
  void DisableHandler();
};

CpuProfiler CpuProfiler::instance_;


#ifdef _WIN32

void test()
{
}
#else

struct CallUnrollInfo
{
	int pc_offset;
	unsigned char ins[16];
	int ins_size;
	int return_sp_offset;
};

#if defined(__linux) && defined(__i386) && defined(__GNUC__)
static const CallUnrollInfo callunrollinfo[] = {
	// Entry to a function:  push %ebp;  mov  %esp,%ebp
	// Top-of-stack contains the caller IP.
	{0,
	 {0x55, 0x89, 0xe5},
	 3,
	 0},
	// Entry to a function, second instruction:  push %ebp;  mov  %esp,%ebp
	// Top-of-stack contains the old frame, caller IP is +4.
	{-1,
	 {0x55, 0x89, 0xe5},
	 3,
	 4},
	// Return from a function: RET.
	// Top-of-stack contains the caller IP.
	{0,
	 {0xc3},
	 1,
	 0}};

inline void *GetPC(const ucontext_t &signal_ucontext)
{
	// See comment above struct CallUnrollInfo.  Only try instruction
	// flow matching if both eip and esp looks reasonable.
	const int eip = signal_ucontext.uc_mcontext.gregs[REG_RIP];
	const int esp = signal_ucontext.uc_mcontext.gregs[REG_RSP];
	if ((eip & 0xffff0000) != 0 && (~eip & 0xffff0000) != 0 &&
		(esp & 0xffff0000) != 0)
	{
		char *eip_char = reinterpret_cast<char *>(eip);
		for (int i = 0; i < sizeof(callunrollinfo) / sizeof(*callunrollinfo); ++i)
		{
			if (!memcmp(eip_char + callunrollinfo[i].pc_offset,
						callunrollinfo[i].ins, callunrollinfo[i].ins_size))
			{
				// We have a match.
				void **retaddr = (void **)(esp + callunrollinfo[i].return_sp_offset);
				return *retaddr;
			}
		}
	}
	return (void *)eip;
}
#else
inline void* GetPC(const ucontext_t& signal_ucontext) {
#if defined(__s390__) && !defined(__s390x__)
	// Mask out the AMODE31 bit from the PC recorded in the context.
	return (void*)((unsigned long)signal_ucontext.uc_mcontext.gregs[REG_RIP] & 0x7fffffffUL);
#else
	return (void*)signal_ucontext.uc_mcontext.gregs[REG_RIP];   // defined in config.h
#endif
}
#endif

struct libgcc_backtrace_data
{
	void **array;
	int skip;
	int pos;
	int limit;
};

static _Unwind_Reason_Code libgcc_backtrace_helper(struct _Unwind_Context *ctx,
												   void *_data)
{
	libgcc_backtrace_data *data =
		reinterpret_cast<libgcc_backtrace_data *>(_data);

	if (data->skip > 0)
	{
		data->skip--;
		return _URC_NO_REASON;
	}

	if (data->pos < data->limit)
	{
		void *ip = reinterpret_cast<void *>(_Unwind_GetIP(ctx));
		;
		data->array[data->pos++] = ip;
	}

	return _URC_NO_REASON;
}

static int GetStackFramesWithContext_libgcc(void **result, int max_depth,
											int skip_count, const void *uc)
{
	libgcc_backtrace_data data;
	data.array = result;
	// we're also skipping current and parent's frame
	data.skip = skip_count + 2;
	data.pos = 0;
	data.limit = max_depth;

	_Unwind_Backtrace(libgcc_backtrace_helper, &data);

	if (data.pos > 1 && data.array[data.pos - 1] == NULL)
		--data.pos;

	return data.pos;
}

bool prof_handler_called = false;

void prof_handler(int sig, siginfo_t *info, void *signal_ucontext)
{
	prof_handler_called = true;
	CpuProfiler* instance = &CpuProfiler::instance_;

	{
		void *stack[kMaxStackDepth];

		stack[0] = GetPC(*reinterpret_cast<ucontext_t *>(signal_ucontext));

		int depth = GetStackFramesWithContext_libgcc(stack + 1, kMaxStackDepth - 1,
													 3, signal_ucontext);

		void **used_stack;
		if (depth > 0 && stack[1] == stack[0])
		{
			used_stack = stack + 1;
		}
		else
		{
			used_stack = stack;
			depth++; // To account for pc value in stack[0];
		}

		instance->collector_.Add(depth, used_stack);
	}
}

#include <stdio.h>
#include <stdlib.h>
void test()
{
	ProfilerStart("test");

	//struct sigaction sa;
	//sa.sa_sigaction = prof_handler;
	//sa.sa_flags = SA_RESTART | SA_SIGINFO;
	//sigemptyset(&sa.sa_mask);
	//if (sigaction(SIGPROF, &sa, NULL) != 0)
	//{
	//	perror("sigaction");
	//	exit(1);
	//}

	//struct itimerval timer;
	//timer.it_interval.tv_sec = 0;
	//timer.it_interval.tv_usec = 1000;
	//timer.it_value = timer.it_interval;
	//setitimer(ITIMER_PROF, &timer, 0);

	int r = 0;
  for (int i = 0; !prof_handler_called; ++i) {
    for (int j = 0; j < i; j++) {
      r ^= i;
      r <<= 1;
      r ^= j;
      r >>= 1;
    }
  }

	// struct itimerval timer;
	// static const int kMillion = 1000000;
	// int interval_usec = kMillion;
	// timer.it_interval.tv_sec = 0;
	// timer.it_interval.tv_usec = 100;
	// // timer.it_interval.tv_sec = interval_usec / kMillion;
	// // timer.it_interval.tv_usec = interval_usec % kMillion;
	// timer.it_value = timer.it_interval;
	// setitimer(2, &timer, 0);
}
#endif // _WIN32


CpuProfiler::CpuProfiler()
{
#ifdef _WIN32
#else
	struct sigaction sa;
	sa.sa_sigaction = prof_handler;
	sa.sa_flags = SA_RESTART | SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGPROF, &sa, NULL) != 0)
	{
		perror("sigaction");
		exit(1);
	}
#endif // _WIN32
}

bool CpuProfiler::Start(const char* fname, int frequency /*= 4000*/)
{
	if (collector_.enabled()) {
		return false;
	}

	if (!collector_.Start(fname, frequency)) {
		return false;
	}

	this->frequency_ = frequency;

	EnableHandler();

	return true;
}

void CpuProfiler::Stop()
{
	if (!collector_.enabled()) {
		return;
	}

	DisableHandler();

	collector_.Stop();
}

void CpuProfiler::EnableHandler()
{
#ifdef _WIN32
#else
	struct itimerval timer;
	static const int kMillion = 1000000;
	int interval_usec = kMillion / this->frequency_;
	timer.it_interval.tv_sec = interval_usec / kMillion;
	timer.it_interval.tv_usec = interval_usec % kMillion;
	timer.it_value = timer.it_interval;
	setitimer(ITIMER_PROF, &timer, 0);
#endif // _WIN32

}

void CpuProfiler::DisableHandler()
{
#ifdef _WIN32
#else
	struct itimerval timer;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;
	timer.it_value = timer.it_interval;
	setitimer(ITIMER_PROF, &timer, 0);
#endif // _WIN32
}

int ProfilerStart(const char* fname)
{
	return CpuProfiler::instance_.Start(fname);
}

void ProfilerStop(void)
{
	CpuProfiler::instance_.Stop();
}



