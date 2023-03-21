#include "../include/profiler.h"

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

// static int GET_STACK_TRACE_OR_FRAMES{
//   libgcc_backtrace_data data;
//   data.array = result;
//   // we're also skipping current and parent's frame
//   data.skip = skip_count + 2;
//   data.pos = 0;
//   data.limit = max_depth;
//
//   _Unwind_Backtrace(libgcc_backtrace_helper, &data);
//
//   if (data.pos > 1 && data.array[data.pos - 1] == NULL)
//	--data.pos;
//
//   return data.pos;
// }

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
// , void *cpu_profiler)
{
	prof_handler_called = true;
	// CpuProfiler* instance = static_cast<CpuProfiler*>(cpu_profiler);

	// if (instance->filter_ == NULL ||
	// 	(*instance->filter_)(instance->filter_arg_))
	{
		void *stack[kMaxStackDepth];

		stack[0] = GetPC(*reinterpret_cast<ucontext_t *>(signal_ucontext));

		int depth = GetStackFramesWithContext_libgcc(stack + 1, kMaxStackDepth - 1,
													 3, signal_ucontext);

		void **used_stack;
		if (depth > 0 && stack[1] == stack[0])
		{
			// in case of non-frame-pointer-based unwinding we will get
			// duplicate of PC in stack[1], which we don't want
			used_stack = stack + 1;
		}
		else
		{
			used_stack = stack;
			depth++; // To account for pc value in stack[0];
		}

		// instance->collector_.Add(depth, used_stack);
	}
}

#include <stdio.h>
#include <stdlib.h>
void test()
{
	struct sigaction sa;
	sa.sa_sigaction = prof_handler;
	sa.sa_flags = SA_RESTART | SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGPROF, &sa, NULL) != 0)
	{
		perror("sigaction");
		exit(1);
	}

	struct itimerval timer;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 1000;
	timer.it_value = timer.it_interval;
	setitimer(ITIMER_PROF, &timer, 0);

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
