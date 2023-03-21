#include "../include/profiler.h"

#include <string.h>
#ifdef _WIN32
#else
#include <ucontext.h> 
#include <unwind.h>
#endif // _WIN32

static const int kMaxStackDepth = 254;  // Max stack depth stored in profile

#ifdef _WIN32

void test()
{
}
#else

#define GST_SUFFIX libgcc

#define SIS_CONCAT2(a, b) a##b
#define SIS_CONCAT(a, b) SIS_CONCAT2(a,b)

#define SIS_STRINGIFY(a) SIS_STRINGIFY2(a)
#define SIS_STRINGIFY2(a) #a

#define IS_STACK_FRAMES 0
#define IS_WITH_CONTEXT 0
#define GET_STACK_TRACE_OR_FRAMES \
  SIS_CONCAT(GetStackTrace_, GST_SUFFIX)(void **result, int max_depth, int skip_count)
#undef IS_STACK_FRAMES
#undef IS_WITH_CONTEXT
#undef GET_STACK_TRACE_OR_FRAMES

#define IS_STACK_FRAMES 1
#define IS_WITH_CONTEXT 0
#define GET_STACK_TRACE_OR_FRAMES \
  SIS_CONCAT(GetStackFrames_, GST_SUFFIX)(void **result, int *sizes, int max_depth, int skip_count)
#undef IS_STACK_FRAMES
#undef IS_WITH_CONTEXT
#undef GET_STACK_TRACE_OR_FRAMES

#define IS_STACK_FRAMES 0
#define IS_WITH_CONTEXT 1
#define GET_STACK_TRACE_OR_FRAMES \
  SIS_CONCAT(GetStackTraceWithContext_, GST_SUFFIX)(void **result, int max_depth, \
                                                   int skip_count, const void *ucp)
#undef IS_STACK_FRAMES
#undef IS_WITH_CONTEXT
#undef GET_STACK_TRACE_OR_FRAMES

#define IS_STACK_FRAMES 1
#define IS_WITH_CONTEXT 1
#define GET_STACK_TRACE_OR_FRAMES \
  SIS_CONCAT(GetStackFramesWithContext_, GST_SUFFIX)(void **result, int *sizes, int max_depth, \
                                                    int skip_count, const void *ucp)
#undef IS_STACK_FRAMES
#undef IS_WITH_CONTEXT
#undef GET_STACK_TRACE_OR_FRAMES

struct CallUnrollInfo {
	int pc_offset;
	unsigned char ins[16];
	int ins_size;
	int return_sp_offset;
};

#if defined(__linux) && defined(__i386) && defined(__GNUC__)
static const CallUnrollInfo callunrollinfo[] = {
	// Entry to a function:  push %ebp;  mov  %esp,%ebp
	// Top-of-stack contains the caller IP.
	{ 0,
	  {0x55, 0x89, 0xe5}, 3,
	  0
	},
	// Entry to a function, second instruction:  push %ebp;  mov  %esp,%ebp
	// Top-of-stack contains the old frame, caller IP is +4.
	{ -1,
	  {0x55, 0x89, 0xe5}, 3,
	  4
	},
	// Return from a function: RET.
	// Top-of-stack contains the caller IP.
	{ 0,
	  {0xc3}, 1,
	  0
	}
};

inline void* GetPC(const ucontext_t& signal_ucontext) {
	// See comment above struct CallUnrollInfo.  Only try instruction
	// flow matching if both eip and esp looks reasonable.
	const int eip = signal_ucontext.uc_mcontext.gregs[REG_RIP];
	const int esp = signal_ucontext.uc_mcontext.gregs[REG_RSP];
	if ((eip & 0xffff0000) != 0 && (~eip & 0xffff0000) != 0 &&
		(esp & 0xffff0000) != 0) {
		char* eip_char = reinterpret_cast<char*>(eip);
		for (int i = 0; i < sizeof(callunrollinfo) / sizeof(*callunrollinfo); ++i) {
			if (!memcmp(eip_char + callunrollinfo[i].pc_offset,
				callunrollinfo[i].ins, callunrollinfo[i].ins_size)) {
				// We have a match.
				void** retaddr = (void**)(esp + callunrollinfo[i].return_sp_offset);
				return *retaddr;
			}
		}
	}
	return (void*)eip;
}
#else
inline void* GetPC(const ucontext_t& signal_ucontext) {
#if defined(__s390__) && !defined(__s390x__)
	// Mask out the AMODE31 bit from the PC recorded in the context.
	return (void*)((unsigned long)signal_ucontext.PC_FROM_UCONTEXT & 0x7fffffffUL);
#else
	return (void*)signal_ucontext.PC_FROM_UCONTEXT;   // defined in config.h
#endif
}
#endif


struct libgcc_backtrace_data {
	void** array;
	int skip;
	int pos;
	int limit;
};

static _Unwind_Reason_Code libgcc_backtrace_helper(struct _Unwind_Context* ctx,
	void* _data) {
	libgcc_backtrace_data* data =
		reinterpret_cast<libgcc_backtrace_data*>(_data);

	if (data->skip > 0) {
		data->skip--;
		return _URC_NO_REASON;
	}

	if (data->pos < data->limit) {
		void* ip = reinterpret_cast<void*>(_Unwind_GetIP(ctx));;
		data->array[data->pos++] = ip;
	}

	return _URC_NO_REASON;
}

//static int GET_STACK_TRACE_OR_FRAMES{
//  libgcc_backtrace_data data;
//  data.array = result;
//  // we're also skipping current and parent's frame
//  data.skip = skip_count + 2;
//  data.pos = 0;
//  data.limit = max_depth;
//
//  _Unwind_Backtrace(libgcc_backtrace_helper, &data);
//
//  if (data.pos > 1 && data.array[data.pos - 1] == NULL)
//	--data.pos;
//
//  return data.pos;
//}

static int GetStackFramesWithContext_libgcc(void** result, int* sizes, int max_depth, \
int skip_count, const void* ucp){
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

void prof_handler(int sig, siginfo_t*, void* signal_ucontext,
	void* cpu_profiler) {
	//CpuProfiler* instance = static_cast<CpuProfiler*>(cpu_profiler);

	if (instance->filter_ == NULL ||
		(*instance->filter_)(instance->filter_arg_)) {
		void* stack[kMaxStackDepth];

		stack[0] = GetPC(*reinterpret_cast<ucontext_t*>(signal_ucontext));

		int depth = GetStackFramesWithContext_libgcc(stack + 1, arraysize(stack) - 1,
			3, signal_ucontext);

		void** used_stack;
		if (depth > 0 && stack[1] == stack[0]) {
			// in case of non-frame-pointer-based unwinding we will get
			// duplicate of PC in stack[1], which we don't want
			used_stack = stack + 1;
		}
		else {
			used_stack = stack;
			depth++;  // To account for pc value in stack[0];
		}

		//instance->collector_.Add(depth, used_stack);
	}
}

static GetStackImplementation SIS_CONCAT(impl__, GST_SUFFIX) = {
  SIS_CONCAT(GetStackFrames_, GST_SUFFIX),
  SIS_CONCAT(GetStackFramesWithContext_, GST_SUFFIX),
  SIS_CONCAT(GetStackTrace_, GST_SUFFIX),
  SIS_CONCAT(GetStackTraceWithContext_, GST_SUFFIX),
  SIS_STRINGIFY(GST_SUFFIX)
};

#undef SIS_CONCAT2
#undef SIS_CONCAT


void test()
{
	prof_handler(1, 0, 0, 0);
}
#endif // _WIN32
