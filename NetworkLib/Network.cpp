#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "Winmm")

#include <WinSock2.h> 
#include <stdlib.h>
#include <stdio.h>


#include "CircularBuffer.h"
#include "SerializationBuffer.h"




#include <map>
#include <queue>

#define SERVERPORT 6000
#define BUFSIZE 512

HANDLE WorkerEvent;
HANDLE DeleteEvent;
RingQueue JobQueue(950000);
#define JOB_RECV		1
struct Job_Header
{
	short type = 0;
	short payloadLen = 0;
};
struct Job_Recv
{
	DWORD SessionID;
	CPacket* Serialbuf;
};
SRWLOCK Job_enquelock;
SRWLOCK Job_dequelock;

long long SC;
long long RC;

int gID;
class Session
{
public:
	bool Valid;


	//수정 필요. 질문임, valid값 필요한지
	struct Header
	{
		unsigned short Len;
	};

	Header NetworkHeader;


	unsigned long long SendFlag;

	SOCKET Sock;
	DWORD SessionID;

	OVERLAPPED SendOverlapped;
	OVERLAPPED RecvOverlapped;


	RingQueue RecvQ;
	RingQueue SendQ;




	long long Count;
	SRWLOCK seslock;

	CPacket Packet;

public:
	Session(SOCKET arg, HANDLE hcp) : RecvQ(5000), SendQ(5000)
	{
		Sock = arg; 
		SessionID = gID++;
		Count = 0; 
		Valid = true;
		SendFlag = false;
		InitializeSRWLock(&seslock);
		memset(&SendOverlapped, 0, sizeof(SendOverlapped));
		memset(&RecvOverlapped, 0, sizeof(RecvOverlapped));

		//IOCP SIGN
		CreateIoCompletionPort((HANDLE)Sock, hcp, (ULONG_PTR)this, 0);
		int bufSize = 0;
		setsockopt(Sock, SOL_SOCKET, SO_SNDBUF, (char*)&bufSize, sizeof(bufSize));
	}


	void Lock()
	{
		AcquireSRWLockExclusive(&seslock);
	}
	void Release()
	{
		ReleaseSRWLockExclusive(&seslock);
	}

	void IO_CountIncrement()
	{
		InterlockedIncrement64(&Count);
	}
	void IO_CountCheck()
	{
		int ret = InterlockedDecrement64(&Count);
		if (ret == 0)
		{

		}
			
	}

	bool IOCP_CompareRecv(OVERLAPPED* arg)
	{
		return arg == &RecvOverlapped;
	}
	bool IOCP_CompareSend(OVERLAPPED* arg)
	{
		return arg == &SendOverlapped;
	}
	void INIT_Recvbuf(WSABUF* wsabuf)
	{
		wsabuf[0].buf = RecvQ.GetRearBufferPtr();
		wsabuf[0].len = RecvQ.DirectEnqueueSize();

		wsabuf[1].buf = RecvQ.GetCircularPtr();
		wsabuf[1].len = RecvQ.GetFreeSize() - wsabuf[0].len;
	}
	void INIT_Sendbuf(WSABUF* wsabuf)
	{
		wsabuf[0].buf = SendQ.GetFrontBufferPtr();
		wsabuf[0].len = SendQ.DirectDequeueSize();

		wsabuf[1].buf = SendQ.GetCircularPtr();
		wsabuf[1].len = SendQ.GetUseSize() - wsabuf[0].len;
	}
	bool RecvPost()
	{
		WSABUF recvBuf[2];
		INIT_Recvbuf(recvBuf);

		ZeroMemory(&RecvOverlapped, sizeof(RecvOverlapped));

		DWORD recvbytes = 0;

		DWORD flags = 0;


		IO_CountIncrement();
		int retval = WSARecv(Sock, recvBuf, 2, &recvbytes, &flags, &RecvOverlapped, NULL);
		if (retval == SOCKET_ERROR)
		{
			if (WSAGetLastError() != ERROR_IO_PENDING)
			{
				IO_CountCheck();
				return false;
			}
		}

		return true;
	}
	virtual void OnRecvComplete(DWORD ID, CPacket* packet)
	{
		AcquireSRWLockExclusive(&Job_enquelock);
		Job_Header job_head;
		job_head.type = JOB_RECV;
		job_head.payloadLen = sizeof(Job_Recv);
		JobQueue.Enqueue((char*)&job_head, sizeof(job_head));


		Job_Recv job;
		job.SessionID = SessionID;
		job.Serialbuf = new CPacket;
		job.Serialbuf->putData(packet->getBufferPtr(), packet->useSize());
		job.Serialbuf->moveRear(packet->useSize());
		packet->clear();
		JobQueue.Enqueue((char*)&job, sizeof(Job_Recv));
		ReleaseSRWLockExclusive(&Job_enquelock);

		SetEvent(WorkerEvent);
	}
	void RecvComplete(DWORD Transferred)
	{
		RecvQ.MoveRear(Transferred);


		while (1)
		{
			if (RecvQ.GetUseSize() < sizeof(NetworkHeader))
			{
				break;
			}
			RecvQ.Peek((char*)&NetworkHeader, sizeof(NetworkHeader));
			if (RecvQ.GetUseSize() < sizeof(NetworkHeader) + NetworkHeader.Len)
			{
				break;
			}
			RecvQ.MoveFront(sizeof(NetworkHeader));
			RecvQ.Dequeue(Packet.getBufferPtr(), NetworkHeader.Len);
			Packet.moveRear(NetworkHeader.Len);
			OnRecvComplete(SessionID, &Packet);
			_interlockedincrement64(&RC);
		}
		RecvPost();
	}
	bool Send(char* point, int size)
	{
	
		if (size + sizeof(Header) > SendQ.GetFreeSize())
		{
			//세션을 끊어야 함
			closesocket(Sock);
			return false;
		}


		if (size > 0)
		{
			Header temp;
			temp.Len = size;
			SendQ.Enqueue((char*)&temp, sizeof(Header));
			SendQ.Enqueue(point, size);
		}
			
		
		if (SendQ.GetUseSize() < 0)
			return true;


		
		
		int prev = InterlockedExchange(&SendFlag, 1);
		if (prev)
		{
			return true;
		}


		WSABUF sendBuf[2];
		INIT_Sendbuf(sendBuf);

		DWORD sendbytes = 0;

		ZeroMemory(&SendOverlapped, sizeof(SendOverlapped));

		
		IO_CountIncrement();
		int retval = WSASend(Sock, sendBuf, 2, &sendbytes, 0, &SendOverlapped, NULL);
		if (retval == SOCKET_ERROR)
		{
			if (WSAGetLastError() != ERROR_IO_PENDING)
			{
				IO_CountCheck();
				
				return false;
			}
		}

		return true;
	}
	void SendComplete(DWORD Transferred)
	{
		SendQ.MoveFront(Transferred);
		InterlockedExchange(&SendFlag, 0);
		Send(0, 0);
	}


	//
	
	


	
};

std::map <SOCKET, Session*> SessionMap;
SRWLOCK maplock;







DWORD WINAPI IOCP_WorkerThread(LPVOID arg);
DWORD WINAPI AcceptThread(LPVOID arg);
DWORD WINAPI Recv_WorkerThread(LPVOID arg);
DWORD WINAPI DeleteThread(LPVOID arg);


int main(int argc, char* argv[])
{
	//
	InitializeSRWLock(&Job_enquelock);
	InitializeSRWLock(&Job_dequelock);
	InitializeSRWLock(&maplock);
	WorkerEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	HANDLE WorkerHandle = (HANDLE)CreateThread(NULL, NULL, Recv_WorkerThread, &WorkerEvent, NULL, NULL);

	DeleteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	HANDLE DeleteHandle = (HANDLE)CreateThread(NULL, NULL, Recv_WorkerThread, &DeleteEvent, NULL, NULL);


	//코어 수만큼 깨우는 iocp create
	HANDLE hcp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (hcp == NULL) return 1;


	//코어 수만큼 IOCP WORKER THREAD 생성
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	HANDLE hThread;
	for (int i = 0; i < (int)si.dwNumberOfProcessors * 2; i++)
	{
		hThread = CreateThread(NULL, 0, IOCP_WorkerThread, hcp, 0, NULL);
		if (hThread == NULL) return 1;
		CloseHandle(hThread);
	}


	//AcceptThread
	hThread = CreateThread(NULL, 0, AcceptThread, hcp, 0, NULL);
	if (hThread == NULL) return 1;
	CloseHandle(hThread);



	




	//
	while (1)
	{
		Sleep(10000);
	}
}

DWORD WINAPI AcceptThread(LPVOID arg)
{
	int retval;

	HANDLE hcp = (HANDLE)arg;

	//WSAStartup()
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		return 1;
	}

	//socket()
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET) return 1;

	//bind() - listensock
	SOCKADDR_IN addr;
	ZeroMemory(&addr, sizeof(addr));
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(SERVERPORT);
	addr.sin_family = AF_INET;

	int bindVal = bind(listen_sock, (SOCKADDR*)&addr, sizeof(addr));
	if (bindVal == SOCKET_ERROR) return 1;

	//listen() - listensock 
	int listenVal = listen(listen_sock, SOMAXCONN);
	if (listenVal == SOCKET_ERROR) return 1;

	//setsockopt - Linger
	LINGER Linger;
	Linger.l_onoff = 1;
	Linger.l_linger = 0;
	setsockopt(listen_sock, SOL_SOCKET, SO_LINGER, (char*)&Linger, sizeof(LINGER));

	//
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen;
	addrlen = sizeof(clientaddr);
	DWORD recvbytes, flags;
	while (1)
	{
		//accept()
		client_sock = accept(listen_sock, (SOCKADDR*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET) return 1;
		printf("[TCP server] 클라이언트 접속 : IP 주소 = %s, 포트 번호 = %d \n",
			inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
		Session* pSession = new Session(client_sock, hcp);
		pSession->RecvPost();

		//printf("-------%d-----\n", client_sock);

		//세션 맵 락
		AcquireSRWLockExclusive(&maplock);
		SessionMap.insert(std::pair<SOCKET, Session*>(client_sock, pSession));
		ReleaseSRWLockExclusive(&maplock);
	}
}

DWORD WINAPI IOCP_WorkerThread(LPVOID arg)
{
	int retval;
	HANDLE hcp = (HANDLE)arg;

	while (1)
	{
		//항상 초기화 해서 쓰레기 값을 사용하지 않게 함
		DWORD Transferred = 0;
		Session* pSession = nullptr;
		OVERLAPPED* pOverlapped = nullptr;
		retval = GetQueuedCompletionStatus(hcp, &Transferred, (PULONG_PTR)&pSession, &pOverlapped, INFINITE);

		//iocp 오류(오버랩 null아님), tiemout(오버랩 null), 작업 실패 여부(오버랩 null)
		//잡스레드 종료
		if (!pOverlapped) 
			return 1;
		
		
		
		pSession->Lock();


		
		if (pSession->IOCP_CompareRecv(pOverlapped))
		{
			pSession->RecvComplete(Transferred);
		}
		else if (pSession->IOCP_CompareSend(pOverlapped))
		{
			pSession->SendComplete(Transferred);
		}
		pSession->IO_CountCheck();

		if (pSession->Count == 0)
		{
			pSession->Valid = false;
			SetEvent(DeleteEvent);
		}
			
		

		pSession->Release();
	}
}

DWORD WINAPI DeleteThread(LPVOID arg)
{
	HANDLE DeleteEvent = *(HANDLE*)arg;

	while (1)
	{
		WaitForSingleObject(DeleteEvent, INFINITE);


		AcquireSRWLockExclusive(&maplock);
		Session* pSession;
		std::map <SOCKET, Session*>::iterator iter;
		for (iter = SessionMap.begin(); iter != SessionMap.end(); iter++)
		{
			pSession = (*iter).second;
			if (pSession->Valid == false)
			{
				SessionMap.erase(pSession->Sock);
				closesocket(pSession->Sock);
				ReleaseSRWLockExclusive(&maplock);
				break;
			}
		}
		ReleaseSRWLockExclusive(&maplock);




	}
}




DWORD WINAPI Recv_WorkerThread(LPVOID arg)
{
	HANDLE WorkerEvent = *(HANDLE*)arg;


	bool repeat = false;
	Job_Header head;
	char job[1000];
	while (1)
	{

		AcquireSRWLockExclusive(&Job_enquelock);
		if (JobQueue.GetUseSize() >= sizeof(Job_Header))
			repeat = true;
		else
			repeat = false;
		ReleaseSRWLockExclusive(&Job_enquelock);

		if (!repeat)
		{
			WaitForSingleObject(WorkerEvent, INFINITE);
		}

		/////////////////////////////////////////////////
		AcquireSRWLockExclusive(&Job_dequelock);
		if (JobQueue.GetUseSize() >= sizeof(Job_Header))
		{
			JobQueue.Peek((char*)&head, sizeof(Job_Header));
		}
		else
		{
			head.type = 0; head.payloadLen = 0;
		}

		
		if (JobQueue.GetUseSize() >= sizeof(Job_Header) + head.payloadLen)
		{
			JobQueue.MoveFront(sizeof(Job_Header));
			JobQueue.Dequeue(job, head.payloadLen);
		}
		else
		{
			head.type = 0; head.payloadLen = 0;
		}
		ReleaseSRWLockExclusive(&Job_dequelock);
	



		///////////////////////////////////////////////
		switch (head.type)
		{
		case JOB_RECV:
			Job_Recv* job_recv = (Job_Recv*)job;
			job_recv->SessionID;

			AcquireSRWLockExclusive(&maplock);
			Session* pSession;
			std::map <SOCKET, Session*>::iterator iter;
			for (iter = SessionMap.begin(); iter != SessionMap.end(); iter++)
			{
				pSession = (*iter).second;
				if (pSession->SessionID == job_recv->SessionID)
				{
					_interlockedincrement64(&SC);
					pSession->Lock();
					pSession->Send(job_recv->Serialbuf->getBufferPtr(), job_recv->Serialbuf->useSize());
					delete(job_recv->Serialbuf);
					pSession->Release();
				}
			}
			ReleaseSRWLockExclusive(&maplock);

			
			break;
		}

		

	}
}


