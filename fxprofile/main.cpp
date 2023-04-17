#include "include/profiler.h"
#include <time.h>
#include <chrono>
#include <thread>


int main()
{
	test();
	for (size_t i = 0; i < 100000; i++)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	ProfilerStop();
	return 0;
}


















#if 0

#ifdef _WIN32
#include <WinSock2.h>
#include <windows.h>
#include <Mmsystem.h>
#pragma comment(lib, "Winmm.lib")

#else
#include <time.h>
#include <sys/time.h>
#endif // _WIN32

#include <signal.h>
#include <iostream>
#include <thread>

static void StartThreadTimer(int dwTimerType, int dwSignalNumber, int dwFrequency, short wTimerKey)
{
}


#ifdef _WIN32
HANDLE	m_hTimerQueue;
HANDLE	m_hTimerQueueTimer;

//TimerRoutine回调函数实现
static void CALLBACK TimerRoutine(void* lpParam, BOOLEAN TimerOrWaitFired)
{
	//lpParam为传过来的参数
	raise(SIGINT);
}


//(HWND, UINT, UINT_PTR, DWORD)
void TimerProc1(HWND hwnd, unsigned int uMsg, UINT_PTR idEvent, unsigned long dwTime)
{
	//raise(SIGINT);
	std::cout << __FUNCTION__ << "\n";
}

std::thread* t_thread;
void MulTimerThread(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
	raise(SIGINT);
}
#endif // _WIN32



void StartTimer()
{
#ifdef _WIN32
	auto id = timeSetEvent(1, 1, MulTimerThread, 0, TIME_PERIODIC);
	//if (m_hTimerQueue == NULL && m_hTimerQueueTimer == NULL)
	//{
	//	m_hTimerQueue = CreateTimerQueue();
	//	if (m_hTimerQueue != NULL)
	//	{
	//		//TimerRoutine为回调函数
	//		//40ms循环执行
	//		//具体应用可以查看CreateTimerQueueTimer的定义
	//		if (!CreateTimerQueueTimer(&m_hTimerQueueTimer, m_hTimerQueue, TimerRoutine, 0, 1000, 1000, WT_EXECUTEINWAITTHREAD))
	//		{
	//			m_hTimerQueue = NULL;
	//			m_hTimerQueueTimer = NULL;
	//		}
	//	}
	//	else
	//	{
	//		m_hTimerQueue = NULL;
	//		m_hTimerQueueTimer = NULL;
	//	}
	//}
#else
	struct itimerval timer;
	static const int kMillion = 1000000;
	int interval_usec = kMillion;
	timer.it_interval.tv_sec = interval_usec / kMillion;
	timer.it_interval.tv_usec = interval_usec % kMillion;
	timer.it_value = timer.it_interval;
	setitimer(2, &timer, 0);
#endif // _WIN32

}

#ifdef _WIN32
void OnSig45(int sig)
#else
void OnSig45(int n, siginfo_t* sinfo, void* p)
#endif // _WIN32
{
	std::cout << __FUNCTION__ << "\n";
#ifdef _WIN32
	signal(SIGINT, OnSig45);
#endif // _WIN32
}

int main1()
{
#ifdef _WIN32
	signal(SIGINT, OnSig45);

#else
	struct sigaction sa;
	sa.sa_sigaction = OnSig45;
	sa.sa_flags = SA_RESTART | SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sigaction(27, &sa, NULL);
#endif

	std::thread t([]()
		{
			for (; ;)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		});
	t.detach();

	t_thread = &t;

	std::thread t1([]()
		{
			for (; ;)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		});
	t1.detach();
	StartTimer();
	for (;;)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	return 0;
}


#include <Windows.h>
#include <processthreadsapi.h>
#include <Tlhelp32.h>
#include <DbgHelp.h>
#include <iostream>
#include <sstream>
#include <map>
#include <tuple>
#include <thread>
#include<assert.h>
#include <list>

#pragma comment(lib, "dbghelp.lib")

HANDLE proc;
struct TestThreadContext {
	static void workert() {
		for (int j = 0; j < 100000; ++j) {
			//if (j % 100 == 0)
				//std::cout << "thread " << GetCurrentThreadId() << " run" << std::endl;
			Sleep(100);
		}
	}
	// 获取其它线程堆栈
	// stacks {线程id， 调用栈，错误说明}
	static void get_threads_stack(std::list < std::tuple<unsigned, std::string, std::string >>& stacks) {
		auto pid = GetCurrentProcessId();
		auto tid = GetCurrentThreadId();
		//HANDLE proc = GetCurrentProcess();
		//SymInitialize(proc, NULL, TRUE);
		HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, pid);
		if (snap == INVALID_HANDLE_VALUE) {
			return;
		}
		THREADENTRY32 e = { sizeof(e) };
		BOOL ok = Thread32First(snap, &e);
		for (; ok; ok = Thread32Next(snap, &e)) {
			if (e.th32OwnerProcessID != pid || e.th32ThreadID == tid)
				continue;
			DWORD thread_id = e.th32ThreadID;
			//cout << "open thread " << e.th32ThreadID << endl;
			HANDLE th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, thread_id);
			if (th == INVALID_HANDLE_VALUE) {
				//cout << "open thread " << tid << " failed" << endl;
				stacks.emplace_back(thread_id, "", "error: open failed");
				continue;
			}
			DWORD ret = SuspendThread(th);
			if (ret == (DWORD)-1) {
				CloseHandle(th);
				stacks.emplace_back(thread_id, "", "error: SuspendThread failed");
				continue;
			}
			CONTEXT ctx;
			ZeroMemory(&ctx, sizeof(ctx));
			ctx.ContextFlags = CONTEXT_ALL;
			ret = GetThreadContext(th, &ctx);
			if (!ret) {
				ResumeThread(th);
				CloseHandle(th);
				stacks.emplace_back(thread_id, "", "error: GetThreadContext failed");
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
			//
			typedef struct tag_SYMBOL_INFO
			{
				IMAGEHLP_SYMBOL symInfo;
				TCHAR szBuffer[MAX_PATH];
			} SYMBOL_INFO, * LPSYMBOL_INFO;
			// 32位系统下变量类型是DWORD ，64位系统下则是DWORD64
			decltype(sf.AddrPC.Offset) dwDisplament = 0;
			DWORD dwDis32 = 0;
			SYMBOL_INFO stack_info = { 0 };
			PIMAGEHLP_SYMBOL pSym = (PIMAGEHLP_SYMBOL)&stack_info;
			pSym->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL);
			pSym->MaxNameLength = sizeof(SYMBOL_INFO) - offsetof(SYMBOL_INFO, symInfo.Name);
			IMAGEHLP_LINE ImageLine = { 0 };
			ImageLine.SizeOfStruct = sizeof(IMAGEHLP_LINE);
			std::string stack = "";
			while (StackWalk(IMAGE_FILE_MACHINE_AMD64, proc, th, &sf, &ctx,
				NULL, SymFunctionTableAccess, SymGetModuleBase, NULL))
			{
				char buf[512];
				if (SymGetSymFromAddr(proc, sf.AddrPC.Offset, &dwDisplament, pSym)) {
					if (SymGetLineFromAddr(proc, sf.AddrPC.Offset, &dwDis32, &ImageLine)) {
						char* fullpath = ImageLine.FileName;
						// find file name in full path
						char* f = fullpath + strlen(fullpath);
						while (*f != '\\' && f > fullpath) --f;
						if (f > fullpath)
							++f;
						snprintf(buf, sizeof(buf), "%#llx+%s [%s: %d]\n",
							pSym->Address, pSym->Name, f, ImageLine.LineNumber);
					}
					else {
						snprintf(buf, sizeof(buf), "%#llx+%s\n", pSym->Address, pSym->Name);
					}
				}
				else {
					snprintf(buf, sizeof(buf), "%#llx, err: %d\n", sf.AddrPC.Offset, GetLastError());
				}
				stack.append(buf);
			}
			stacks.emplace_back(thread_id, stack, "");
			ResumeThread(th);
			CloseHandle(th);
		}
		CloseHandle(snap);
		//SymCleanup(proc);
	}

	// 测试获取其它线程堆栈并打印
	static void master() {
		const int num = 4;
		std::thread ws[num];
		for (int j = 0; j < num; ++j)
			ws[j] = std::thread(workert);
		for (int j = 0; j < num; ++j)
			ws[j].detach();
		Sleep(5000);
		auto pid = GetCurrentProcessId();
		auto tid = GetCurrentThreadId();
		proc = GetCurrentProcess();
		SymInitialize(proc, NULL, TRUE);
		for (;;)
		{
			std::list < std::tuple<unsigned, std::string, std::string >> stacks;
			get_threads_stack(stacks);
			for (auto& t : stacks) {
				unsigned tid;
				std::string stack, error;
				std::tie(tid, stack, error) = t;
				if (error.empty()) {
					std::cout << "thread " << tid << "\n" << stack << std::endl;
				}
				else {
					std::cout << "thread " << tid << " " << error << std::endl;
				}
			}

			Sleep(5000);
		}
	}
};

int main2(int argc, char* argv[])
{
	TestThreadContext::master();
	return 0;
}

#endif


