#ifdef _WIN32
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

int main()
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
