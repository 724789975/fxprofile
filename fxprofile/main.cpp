#include <windows.h>
#include <signal.h>
#include <iostream>

static void StartThreadTimer(int dwTimerType, int dwSignalNumber, int dwFrequency, short wTimerKey)
{
}



HANDLE	m_hTimerQueue;
HANDLE	m_hTimerQueueTimer;
 
//TimerRoutine�ص�����ʵ��
static void CALLBACK TimerRoutine(void* lpParam, BOOLEAN TimerOrWaitFired)
{
	//lpParamΪ�������Ĳ���
	raise(SIGINT);
}

void StartTimer()
{
 	if (m_hTimerQueue == NULL && m_hTimerQueueTimer == NULL)
 	{
 		m_hTimerQueue = CreateTimerQueue();
 		if (m_hTimerQueue != NULL)
 		{
                        //TimerRoutineΪ�ص�����
                        //40msѭ��ִ��
                        //����Ӧ�ÿ��Բ鿴CreateTimerQueueTimer�Ķ���
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