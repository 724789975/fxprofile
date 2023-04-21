#include "profiledata.h"
#include "../include/profiler.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <Windows.h>
#include <processthreadsapi.h>
#include <Tlhelp32.h>
#include <DbgHelp.h>

#else
#include <ucontext.h>
#include <unwind.h>
#include <signal.h>
#include <sys/time.h>
#endif // _WIN32

static const int kMaxStackDepth = 254; // Max stack depth stored in profile

#ifdef _WIN32
#else
void prof_handler(int sig, siginfo_t* info, void* signal_ucontext);
#endif	//! _WIN32

struct ProfilerState
{
	int    enabled;             /* Is profiling currently enabled? */
	time_t start_time;          /* If enabled, when was profiling started? */
	char   profile_name[1024];  /* Name of profile file being written, or '\0' */
	int    samples_gathered;    /* Number of samples gathered so far (or 0) */
};

class CpuProfiler
{
public:
	CpuProfiler();
	~CpuProfiler();

#ifdef _WIN32
	static void prof_handler(void* lpParam, BOOLEAN TimerOrWaitFired);
#else
	friend void prof_handler(int sig, siginfo_t* info, void* signal_ucontext);
#endif // _WIN32

	friend class ProfileData;

	bool Start(const char* fname, int frequency = 4000);

	void Stop();

	void FlushTable() {};

	bool Enabled() { return false; };

	void GetCurrentState(ProfilerState* state) {};

	static CpuProfiler instance_;

private:
	ProfileData   collector_;
	int frequency_;
#ifdef _WIN32
	HANDLE proc_;
	HANDLE m_hTimerQueue;
	HANDLE m_hTimerQueueTimer;
	HANDLE m_hModule;
#endif // _WIN32


	void EnableHandler();
	void DisableHandler();
};

CpuProfiler CpuProfiler::instance_;


#ifdef _WIN32
void CpuProfiler::prof_handler(void* lpParam, BOOLEAN TimerOrWaitFired)
{
	unsigned long pid = GetCurrentProcessId();
	unsigned long tid = GetCurrentThreadId();
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, pid);
	if (snap == INVALID_HANDLE_VALUE)
	{
		return;
	}
	THREADENTRY32 e = { sizeof(e) };
	BOOL ok = Thread32First(snap, &e);
	for (; ok; ok = Thread32Next(snap, &e))
	{
		if (e.th32OwnerProcessID != pid || e.th32ThreadID == tid)
			continue;
		DWORD thread_id = e.th32ThreadID;
		//cout << "open thread " << e.th32ThreadID << endl;
		HANDLE th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, thread_id);
		if (th == INVALID_HANDLE_VALUE)
		{
			//TODO error
			continue;
		}
		DWORD ret = SuspendThread(th);
		if (ret == (DWORD)-1)
		{
			CloseHandle(th);
			//TODO error
			continue;
		}
		CONTEXT ctx;
		ZeroMemory(&ctx, sizeof(ctx));
		ctx.ContextFlags = CONTEXT_ALL;
		ret = GetThreadContext(th, &ctx);
		if (!ret)
		{
			ResumeThread(th);
			CloseHandle(th);
			//TODO error
			continue;
		}
		//
		STACKFRAME sf = { 0 };
		sf.AddrPC.Offset = ctx.Rip;
		sf.AddrPC.Mode = AddrModeFlat;
		sf.AddrFrame.Offset = ctx.Rbp;
		sf.AddrFrame.Mode = AddrModeFlat;
		sf.AddrStack.Offset = ctx.Rsp;
		sf.AddrStack.Mode = AddrModeFlat;

		void* stack[kMaxStackDepth] = {0};
		int depth = 0;
		while (StackWalk(IMAGE_FILE_MACHINE_AMD64, CpuProfiler::instance_.proc_, th, &sf, &ctx,
			NULL, SymFunctionTableAccess, SymGetModuleBase, NULL))
		{
			stack[depth++] = (void*)(sf.AddrPC.Offset - (DWORD64)(CpuProfiler::instance_.m_hModule));
		}
		if (depth)
		{
			CpuProfiler::instance_.collector_.Add(depth, stack);
		}
			
		ResumeThread(th);
		CloseHandle(th);
	}
	CloseHandle(snap);
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
	   0} };

inline void* GetPC(const ucontext_t& signal_ucontext)
{
	// See comment above struct CallUnrollInfo.  Only try instruction
	// flow matching if both eip and esp looks reasonable.
	const int eip = signal_ucontext.uc_mcontext.gregs[REG_RIP];
	const int esp = signal_ucontext.uc_mcontext.gregs[REG_RSP];
	if ((eip & 0xffff0000) != 0 && (~eip & 0xffff0000) != 0 &&
		(esp & 0xffff0000) != 0)
	{
		char* eip_char = reinterpret_cast<char*>(eip);
		for (int i = 0; i < sizeof(callunrollinfo) / sizeof(*callunrollinfo); ++i)
		{
			if (!memcmp(eip_char + callunrollinfo[i].pc_offset,
				callunrollinfo[i].ins, callunrollinfo[i].ins_size))
			{
				// We have a match.
				void** retaddr = (void**)(esp + callunrollinfo[i].return_sp_offset);
				return *retaddr;
			}
		}
	}
	return (void*)eip;
}
#else
inline void* GetPC(const ucontext_t& signal_ucontext)
{
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
	void** array;
	int skip;
	int pos;
	int limit;
};

static _Unwind_Reason_Code libgcc_backtrace_helper(struct _Unwind_Context* ctx, void* _data)
{
	libgcc_backtrace_data* data =
		reinterpret_cast<libgcc_backtrace_data*>(_data);

	if (data->skip > 0)
	{
		data->skip--;
		return _URC_NO_REASON;
	}

	if (data->pos < data->limit)
	{
		void* ip = reinterpret_cast<void*>(_Unwind_GetIP(ctx));
		;
		data->array[data->pos++] = ip;
	}

	return _URC_NO_REASON;
}

static int GetStackFramesWithContext_libgcc(void** result, int max_depth
	, int skip_count, const void* uc)
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

void prof_handler(int sig, siginfo_t* info, void* signal_ucontext)
{
	CpuProfiler* instance = &CpuProfiler::instance_;

	void* stack[kMaxStackDepth];

	stack[0] = GetPC(*reinterpret_cast<ucontext_t*>(signal_ucontext));

	int depth = GetStackFramesWithContext_libgcc(stack + 1, kMaxStackDepth - 1,
		3, signal_ucontext);

	void** used_stack;
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

#endif // _WIN32


CpuProfiler::CpuProfiler()
	: frequency_(4000)
	, m_hTimerQueueTimer(INVALID_HANDLE_VALUE)
{
#ifdef _WIN32
	this->proc_ = GetCurrentProcess();
	SymInitialize(this->proc_, NULL, TRUE);

	this->m_hTimerQueue = CreateTimerQueue();
	if (INVALID_HANDLE_VALUE == this->m_hTimerQueue)
	{
		perror("sigaction");
		exit(1);
	}
	this->m_hModule = GetModuleHandle(NULL);
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

CpuProfiler::~CpuProfiler()
{
#ifdef _WIN32
	SymCleanup(this->proc_);
#endif // _WIN32
};

bool CpuProfiler::Start(const char* fname, int frequency /*= 4000*/)
{
	if (collector_.enabled())
	{
		return false;
	}

	if (!collector_.Start(fname, frequency))
	{
		return false;
	}

	this->frequency_ = frequency;

	EnableHandler();

	return true;
}

void CpuProfiler::Stop()
{
	if (!collector_.enabled())
	{
		return;
	}

	DisableHandler();

	collector_.Stop();
}

void CpuProfiler::EnableHandler()
{
	static const int kMillion = 1000;
#ifdef _WIN32
	int delay = kMillion / this->frequency_;
	if (!CreateTimerQueueTimer(&this->m_hTimerQueueTimer, this->m_hTimerQueue, prof_handler, 0
		, delay, delay 
		, WT_EXECUTEONLYONCE))
		//, WT_EXECUTEINWAITTHREAD))
	{
		this->m_hTimerQueue = NULL;
		this->m_hTimerQueueTimer = NULL;
	}
#else
	struct itimerval timer;
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
	DeleteTimerQueueTimer(this->m_hTimerQueue, this->m_hTimerQueueTimer, NULL);
	m_hTimerQueueTimer = INVALID_HANDLE_VALUE;
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
	return CpuProfiler::instance_.Start(fname, 1);
}

void ProfilerStop(void)
{
	CpuProfiler::instance_.Stop();
}



