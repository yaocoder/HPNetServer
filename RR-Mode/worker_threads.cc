/*
 * CThread.cc
 *
 *  Created on: Mar 4, 2013
 *      Author: yaowei
 */

#include "worker_threads.h"
#include "global_settings.h"
#include "utils.h"

#define ITEMS_PER_ALLOC 64

int CWorkerThread::init_count_ = 0;
pthread_mutex_t	CWorkerThread::init_lock_ = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  CWorkerThread::init_cond_ = PTHREAD_COND_INITIALIZER;



CWorkerThread::CWorkerThread()
{
	last_thread_ = -1;
}

CWorkerThread::~CWorkerThread()
{
}

/* 初始化worker线程池 */
bool CWorkerThread::InitThreads(struct event_base* main_base)
{

	LOG4CXX_INFO(g_logger, "Initializes worker threads...");

	for(unsigned int i=0; i<utils::G<CGlobalSettings>().thread_num_; ++i)
	{
		LIBEVENT_THREAD* libevent_thread_ptr = new LIBEVENT_THREAD;
		/* 建立每个worker线程和主监听线程通信的管道 */
		int fds[2];
		if (pipe(fds) != 0)
		{
			LOG4CXX_ERROR(g_logger, "CThread::InitThreads:Can't create notify pipe");
			return false;
		}
		libevent_thread_ptr->notify_receive_fd = fds[0];
		libevent_thread_ptr->notify_send_fd	   = fds[1];

		if(!SetupThread(libevent_thread_ptr))
		{
			utils::SafeDelete(libevent_thread_ptr);
			LOG4CXX_ERROR(g_logger, "CThread::InitThreads:SetupThread failed.");
			return false;
		}

		vec_libevent_thread_.push_back(libevent_thread_ptr);
	}

	LOG4CXX_INFO(g_logger, "Create threads success. we will done all the libevent setup.");

	for (unsigned int i = 0; i < utils::G<CGlobalSettings>().thread_num_; i++)
	{
		CreateWorker(WorkerLibevent, vec_libevent_thread_.at(i));
	}

	 /* 等待所有线程都已经启动完毕. */
	WaitForThreadRegistration(utils::G<CGlobalSettings>().thread_num_);

	return true;
}

void CWorkerThread::CreateWorker(void *(*func)(void *), void *arg)
{
	pthread_t thread;
	pthread_attr_t attr;
	int ret;

	pthread_attr_init(&attr);

	if ((ret = pthread_create(&thread, &attr, func, arg)) != 0)
	{
		LOG4CXX_FATAL(g_logger, "CWorkerThread::CreateWorker:Can't create thread:" << strerror(ret));
		exit(1);
	}
}


void *CWorkerThread::WorkerLibevent(void *arg)
{
	LIBEVENT_THREAD *me = static_cast<LIBEVENT_THREAD *>(arg);

	me->thread_id = pthread_self();

	RegisterThreadInitialized();

	event_base_dispatch(me->base);

	return NULL;
}

bool CWorkerThread::SetupThread(LIBEVENT_THREAD* me)
{
	me->base = event_base_new();
	assert(me != NULL);

	/* 通过每个worker线程的读管道监听来自master的通知 */
	me->notify_event = *event_new(me->base, me->notify_receive_fd, EV_READ|EV_PERSIST, ReadPipeCb, (void*)me);
	assert(&me->notify_event != NULL);

	if (event_add(&me->notify_event, NULL) == -1)
	{
		int error_code = EVUTIL_SOCKET_ERROR();
		LOG4CXX_ERROR(g_logger, "CWorkerThread::SetupThread:event_add errorCode = " << error_code
								<< ", description = " << evutil_socket_error_to_string(error_code));
		return false;
	}

	return true;
}

void CWorkerThread::ReadPipeCb(int fd, short event, void* arg)
{

	LIBEVENT_THREAD *libevent_thread_ptr = static_cast<LIBEVENT_THREAD*>(arg);
	assert(libevent_thread_ptr != NULL);

	/* read from master-thread had write, a byte 代表一个客户端连接 */
	char buf[1];
	if (read(fd, buf, 1) != 1)
	{
		LOG4CXX_ERROR(g_logger, "CWorkerThread::ThreadLibeventProcess:Can't read from libevent pipe.");
		return;
	}

	/* 将主线程塞到队列中的连接pop出来 */
	CONN_INFO connInfo = libevent_thread_ptr->list_conn.pop_front();

	/*初始化新连接，将连接事件注册入libevent */

}



void CWorkerThread::ClientTcpReadCb(struct bufferevent *bev, void *arg)
{

}

void CWorkerThread::ClientTcpErrorCb(struct bufferevent *bev, short event, void *arg)
{

}


void CWorkerThread::DispatchSfdToWorker(int sfd)
{
	/* Round Robin*/
	int tid = (last_thread_ + 1) % utils::G<CGlobalSettings>().thread_num_;
	LIBEVENT_THREAD *libevent_thread_ptr = vec_libevent_thread_.at(tid);
	last_thread_ = tid;

	/* 将新连接的加入此worker线程连接队列 */
	CONN_INFO connInfo;
	connInfo.sfd = sfd;
	libevent_thread_ptr->list_conn.push_back(connInfo);

	/* 通知此worker线程有新连接到来，可以读取了 */
	char buf[1];
	buf[0] = 'c';
	if (write(libevent_thread_ptr->notify_send_fd, buf, 1) != 1)
	{
		LOG4CXX_WARN(g_logger, "CWorkerThread::DispatchSfdToWorker:Writing to thread notify pipe");
	}
}

void CWorkerThread::RegisterThreadInitialized(void)
{
    pthread_mutex_lock(&init_lock_);
    init_count_++;
    if(init_count_ == int(utils::G<CGlobalSettings>().thread_num_))
    {
    	pthread_cond_signal(&init_cond_);
    }
    pthread_mutex_unlock(&init_lock_);
}

void CWorkerThread::WaitForThreadRegistration(int nthreads)
{
	pthread_mutex_lock(&init_lock_);
    pthread_cond_wait(&init_cond_, &init_lock_);
    pthread_mutex_unlock(&init_lock_);
}



