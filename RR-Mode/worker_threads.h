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

class CWorkerThread
{
public:
	CWorkerThread();
	virtual ~CWorkerThread();

public:

	bool InitThreads(struct event_base* main_base);

	static void DispatchSfdToWorker(int sfd, int id);

private:

	bool SetupThread(LIBEVENT_THREAD* me);

	static void ThreadLibeventProcess(int fd, short event, void* arg);

	static conn *conn_new(const CQ_ITEM* item, LIBEVENT_THREAD* libevent_thread_ptr) ;


	static void CreateWorker(void *(*func)(void *), void *arg);

	static void *WorkerLibevent(void *arg);


	static void RegisterThreadInitialized(void);

	static void WaitForThreadRegistration(int nthreads);


	static void ConnQueueInit(CQ *cq);

	static CQ_ITEM * ConnQueueItemNew(void);

	static void ConnQueuePush(CQ *cq, CQ_ITEM *item);

	static CQ_ITEM* ConnQueuePop(CQ *cq);

	static void ConnQueueItemFree(CQ_ITEM *item);



	static void ClientTcpReadCb(struct bufferevent *bev, void *arg);

	static void ClientTcpErrorCb(struct bufferevent *bev, short event, void *arg);


	static void conn_init(void);

	static conn *conn_from_freelist();

	static bool conn_add_to_freelist(conn *c);

	static void conn_free(conn *c);

	static void conn_close(conn *c, struct bufferevent *bev);

private:

	static std::vector<LIBEVENT_THREAD*> vec_libevent_thread_;

	static int init_count_;
	static pthread_mutex_t 	init_lock_;
	static pthread_cond_t 	init_cond_;

	/* Free list of CQ_ITEM structs */
	static CQ_ITEM *cqi_freelist;
	static pthread_mutex_t 	cqi_freelist_lock;

	static int last_thread;

	/*
	 * Free list management for connections.
	 */

	static conn **freeconns;
	static int freetotal;
	static int freecurr;
	/* Lock for connection freelist */
	static pthread_mutex_t conn_lock;
};

#endif /* CTHREAD_H_ */
