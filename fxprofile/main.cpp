#ifdef _WIN32
#include <windows.h>
#else
#endif // _WIN32

#include <signal.h>
#include <iostream>

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
#endif // _WIN32



void StartTimer()
{
#ifdef _WIN32
 	if (m_hTimerQueue == NULL && m_hTimerQueueTimer == NULL)
 	{
 		m_hTimerQueue = CreateTimerQueue();
 		if (m_hTimerQueue != NULL)
 		{
                        //TimerRoutine为回调函数
                        //40ms循环执行
                        //具体应用可以查看CreateTimerQueueTimer的定义
 			if (!CreateTimerQueueTimer(&m_hTimerQueueTimer, m_hTimerQueue, TimerRoutine, 0, 1000, 1000, WT_EXECUTEINIOTHREAD))
 			{
 				m_hTimerQueue = NULL;
 				m_hTimerQueueTimer = NULL;
 			}
 		}
 		else
 		{
 			m_hTimerQueue = NULL;
 			m_hTimerQueueTimer = NULL;
 		}
 	}
#else
	struct itimerval timer;
	static const int kMillion = 1000000;
	int interval_usec = kMillion;
	timer.it_interval.tv_sec = interval_usec / kMillion;
	timer.it_interval.tv_usec = interval_usec % kMillion;
	timer.it_value = timer.it_interval;
	setitimer(SIGINT, &timer, 0);
#endif // _WIN32

}

void OnSIg45(int n)
{
	std::cout <<__FUNCTION__ << "\n";
	signal(SIGINT, OnSIg45);
}
 
int main()
{
	signal(SIGINT, OnSIg45);
	StartTimer();
	for(;;);
	return 0;
}