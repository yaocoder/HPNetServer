/*
 * worker_threads.h
 *
 *  Created on: Mar 4, 2013
 *      Author: yaowei
 */

#ifndef WORK_THREAD_H_
#define WORK_THREAD_H_

#include <vector>
#include "defines.h"
#include "threadSafe_list.h"

#define DATA_BUFFER_SIZE 2048

typedef struct
{
	char buf[DATA_BUFFER_SIZE];
	unsigned int len;
	int  sfd;
}LOCAL_REV_DATA;

typedef struct {
	int sfd;
}CONN_INFO;

typedef struct {
    pthread_t 			thread_id;        	/* unique ID of this thread */
    struct event_base 	*base;    			/* libevent handle this thread uses */
    struct event 		notify_event;  		/* listen event for notify pipe */
    int 				notify_receive_fd;  /* receiving end of notify pipe */
    int 				notify_send_fd;     /* sending end of notify pipe */
    CThreadSafeList<CONN_INFO> 	list_conn;	/* queue of new connections to handle */
} LIBEVENT_THREAD;

typedef struct conn conn;
struct conn {
    int    sfd;
    int    id;
    char*  rBuf;
    int    rlen;
    char*  wBuf;
    int    wlen;
    conn   *next;     		 /* Used for generating a list of conn structures */
    LIBEVENT_THREAD *thread; /* Pointer to the thread object serving this connection */
};


class CWorkerThread
{
public:
	CWorkerThread();
	virtual ~CWorkerThread();

public:

	bool InitThreads(struct event_base* main_base);

	void DispatchSfdToWorker(int sfd);

private:

	bool SetupThread(LIBEVENT_THREAD* me);

	static void ReadPipeCb(int fd, short event, void* arg);

	static void CreateWorker(void *(*func)(void *), void *arg);

	static void *WorkerLibevent(void *arg);


	static void RegisterThreadInitialized(void);

	static void WaitForThreadRegistration(int nthreads);


	static void ClientTcpReadCb(struct bufferevent *bev, void *arg);

	static void ClientTcpErrorCb(struct bufferevent *bev, short event, void *arg);


private:

	std::vector<LIBEVENT_THREAD*> vec_libevent_thread_;
	int last_thread_;

	static int init_count_;
	static pthread_mutex_t 	init_lock_;
	static pthread_cond_t 	init_cond_;
};

#endif /* CTHREAD_H_ */
