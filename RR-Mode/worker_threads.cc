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
std::vector<LIBEVENT_THREAD*> CWorkerThread::vec_libevent_thread_;
pthread_mutex_t	CWorkerThread::init_lock_ = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  CWorkerThread::init_cond_ = PTHREAD_COND_INITIALIZER;
pthread_mutex_t CWorkerThread::cqi_freelist_lock = PTHREAD_MUTEX_INITIALIZER;
CQ_ITEM *  CWorkerThread::cqi_freelist = NULL;

/*
 * Free list management for connections.
 */
conn ** CWorkerThread::freeconns;
int CWorkerThread::freetotal;
int CWorkerThread::freecurr;
pthread_mutex_t CWorkerThread::conn_lock = PTHREAD_MUTEX_INITIALIZER;

int CWorkerThread::last_thread = -1;

CWorkerThread::CWorkerThread()
{
}

CWorkerThread::~CWorkerThread()
{
}

bool CWorkerThread::InitThreads(struct event_base* main_base)
{
	/* Free list management for connections.*/
	conn_init();

	/* Initializes worker threads */
	LOG4CXX_INFO(g_logger, "Initializes worker threads.");

	for(unsigned int i=0; i<utils::G<CGlobalSettings>().thread_num_; ++i)
	{
		LIBEVENT_THREAD* libevent_thread_ptr = new LIBEVENT_THREAD;

		/* Initializes per worker-thread with master-thread communicate pipe*/
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

	/* Create threads after we've done all the libevent setup. */
	LOG4CXX_INFO(g_logger, "Create threads after we've done all the libevent setup.");

	for (unsigned int i = 0; i < utils::G<CGlobalSettings>().thread_num_; i++)
	{
		CreateWorker(WorkerLibevent, vec_libevent_thread_.at(i));
	}

	 /* Wait for all the threads to set themselves up before returning. */
	WaitForThreadRegistration(utils::G<CGlobalSettings>().thread_num_);

	return true;
}


bool CWorkerThread::SetupThread(LIBEVENT_THREAD* me)
{
	me->base = event_base_new();
	assert(me != NULL);

	/* Listen for notifications from other threads by pipe */
	me->notify_event = *event_new(me->base, me->notify_receive_fd, EV_READ|EV_PERSIST, ThreadLibeventProcess, (void*)me);
	assert(&me->notify_event != NULL);

	if (event_add(&me->notify_event, NULL) == -1)
	{
		int error_code = EVUTIL_SOCKET_ERROR();
		LOG4CXX_ERROR(g_logger, "CWorkerThread::SetupThread:event_add errorCode = " << error_code
								<< ", description = " << evutil_socket_error_to_string(error_code));
		return false;
	}

	/* Initializes worker-threads new_conn_queue */
	me->new_conn_queue = new conn_queue;
	if (me->new_conn_queue == NULL)
	{
		perror("Failed to allocate memory for connection queue");
		return false;
	}
	ConnQueueInit(me->new_conn_queue);

	return true;
}

void CWorkerThread::ThreadLibeventProcess(int fd, short event, void* arg)
{

	LIBEVENT_THREAD *libevent_thread_ptr = static_cast<LIBEVENT_THREAD*>(arg);
	assert(libevent_thread_ptr != NULL);

	/* read from master-thread had write, a byte represents a connection */
	char buf[1];
	if (read(fd, buf, 1) != 1)
	{
		LOG4CXX_ERROR(g_logger, "CWorkerThread::ThreadLibeventProcess:Can't read from libevent pipe.");
		return;
	}

	//将主线程塞到队列中的连接pop出来
	CQ_ITEM *item;
	item = ConnQueuePop(libevent_thread_ptr->new_conn_queue);

	if (NULL != item)
	{
		//初始化新连接，注册事件监听
		conn* c = conn_new(item, libevent_thread_ptr);
		if(NULL == c)
		{
			LOG4CXX_ERROR(g_logger, "CWorkerThread::ThreadLibeventProcess:Can't listen for events on sfd = " << item->sfd);
			close(item->sfd);
		}

		//回收item
		ConnQueueItemFree(item);
	}
}


conn* CWorkerThread::conn_new(const CQ_ITEM* item, LIBEVENT_THREAD* libevent_thread_ptr)
{
	conn* c = conn_from_freelist();
	if(NULL == c)
	{
		c = new conn[1];
		if(NULL == c)
		{
			LOG4CXX_ERROR(g_logger, "CWorkerThread::conn_new:new conn error.");
			return NULL;
		}

		try
		{
			c->rBuf = new char[DATA_BUFFER_SIZE];
			c->wBuf = new char[DATA_BUFFER_SIZE];
		}
		catch(std::bad_alloc &)
		{
			conn_free(c);
			LOG4CXX_ERROR(g_logger, "CWorkerThread::conn_new:new buf error.");
			return NULL;
		}
	}

	c->sfd 	= item->sfd;
	c->id   = item->id;
	c->rlen = 0;
	c->wlen = 0;
	c->thread = libevent_thread_ptr;

	int flag = EV_READ | EV_PERSIST;
	struct bufferevent *client_tcp_event = bufferevent_socket_new(libevent_thread_ptr->base, c->sfd, BEV_OPT_CLOSE_ON_FREE);
	if(NULL == client_tcp_event)
	{
		if(conn_add_to_freelist(c))
		{
			conn_free(c);
		}
		int error_code = EVUTIL_SOCKET_ERROR();
		LOG4CXX_ERROR(g_logger, "CWorkerThread::conn_new:bufferevent_socket_new errorCode = " << error_code
				                 << ", description = " << evutil_socket_error_to_string(error_code));

		return NULL;
	}

	bufferevent_setcb(client_tcp_event, ClientTcpReadCb, NULL, ClientTcpErrorCb, (void*)c);

	struct timeval heartbeat_sec;
	heartbeat_sec.tv_sec = utils::G<CGlobalSettings>().client_heartbeat_timeout_;
	heartbeat_sec.tv_usec= 0;
	bufferevent_set_timeouts(client_tcp_event, &heartbeat_sec, NULL);

	bufferevent_enable(client_tcp_event, flag);

	return c;

}

void CWorkerThread::ClientTcpReadCb(struct bufferevent *bev, void *arg)
{
	conn* c = static_cast<conn*>(arg);
	assert(c != NULL);

	int recv_size = 0;
	if ((recv_size = bufferevent_read(bev, c->rBuf + c->rlen, DATA_BUFFER_SIZE-c->rlen)) > 0)
	{
		c->rlen = c->rlen + recv_size;
	}

	/* 找到\r\n，并把\r\n处填充为\0 */
	if (memchr(c->rBuf, '\n', c->rlen) != NULL)
	{

	}
}

void CWorkerThread::ClientTcpErrorCb(struct bufferevent *bev, short event, void *arg)
{
	conn* c = static_cast<conn*>(arg);

	if (event & BEV_EVENT_TIMEOUT)
	{
		LOG4CXX_WARN(g_logger, "CWorkerThread::ClientTcpErrorCb:TimeOut.");
	}
	else if (event & BEV_EVENT_EOF)
	{
	}
	else if (event & BEV_EVENT_ERROR)
	{
		int error_code = EVUTIL_SOCKET_ERROR();
		LOG4CXX_WARN(g_logger, "CWorkerThread::ClientTcpErrorCb:some other errorCode = " << error_code
                			    << ", description = " << evutil_socket_error_to_string(error_code));
	}

	conn_close(c, bev);
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

	//LOG4CXX_TRACE(g_logger, "CWorkerThread::WorkerLibevent:WorkThread id = " << me->thread_id << " has start..." << me->base);

	RegisterThreadInitialized();

	event_base_dispatch(me->base);

	return NULL;
}

/*
 * Dispatches a new connection to another thread. This is only ever called
 * from the main thread, either during initialization  or because
 * of an incoming connection.
 */
void CWorkerThread::DispatchSfdToWorker(int sfd, int id)
{
	CQ_ITEM *item = ConnQueueItemNew();

	/* RR */
	int tid = (last_thread + 1) % utils::G<CGlobalSettings>().thread_num_;
	LIBEVENT_THREAD *thread = vec_libevent_thread_.at(tid);
	last_thread = tid;

	item->sfd = sfd;
	item->id  = id;

	//LOG4CXX_TRACE(g_logger,  "CWorkerThread::DispatchSfdToWorker:dispatch threadId = " << thread->thread_id);

	ConnQueuePush(thread->new_conn_queue, item);

	char buf[1];
	buf[0] = 'c';
	if (write(thread->notify_send_fd, buf, 1) != 1)
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

/*
 * Initializes a connection queue.
 */
void CWorkerThread::ConnQueueInit(CQ *cq)
{
	pthread_mutex_init(&cq->lock, NULL);
	pthread_cond_init(&cq->cond, NULL);
	cq->head = NULL;
	cq->tail = NULL;
}

/*
 * Returns a fresh connection queue item.
 */
CQ_ITEM * CWorkerThread::ConnQueueItemNew(void)
{
	CQ_ITEM *item = NULL;

	pthread_mutex_lock(&cqi_freelist_lock);
	if (cqi_freelist)
	{
		item = cqi_freelist;
		cqi_freelist = item->next;
	}
	pthread_mutex_unlock(&cqi_freelist_lock);

	if (NULL == item)
	{
		/* Allocate a bunch of items at once to reduce fragmentation */
		item = new CQ_ITEM[ITEMS_PER_ALLOC];
		if (NULL == item)
			return NULL;

		/*
		 * Link together all the new items except the first one，
		 * (which we'll return to the caller) for placement on the freelist.
		 */
		for (int i = 2; i < ITEMS_PER_ALLOC; i++)
			item[i - 1].next = &item[i];

		pthread_mutex_lock(&cqi_freelist_lock);
		item[ITEMS_PER_ALLOC - 1].next = cqi_freelist;
		cqi_freelist = &item[1];
		pthread_mutex_unlock(&cqi_freelist_lock);
	}

	return item;
}

/*
 * Adds an item to a connection queue.
 */
void CWorkerThread::ConnQueuePush(CQ *cq, CQ_ITEM *item)
{
	item->next = NULL;

	pthread_mutex_lock(&cq->lock);
	if (NULL == cq->tail)
		cq->head = item;
	else
		cq->tail->next = item;
	cq->tail = item;
	pthread_cond_signal(&cq->cond);
	pthread_mutex_unlock(&cq->lock);
}

/*
 * Looks for an item on a connection queue, but doesn't block if there isn't one.
 * Returns the item, or NULL if no item is available
 */
CQ_ITEM* CWorkerThread::ConnQueuePop(CQ *cq)
{
	CQ_ITEM *item;

	pthread_mutex_lock(&cq->lock);
	item = cq->head;
	if (NULL != item)
	{
		cq->head = item->next;
		if (NULL == cq->head)
			cq->tail = NULL;
	}
	pthread_mutex_unlock(&cq->lock);

	return item;
}

/*
 * Frees a connection queue item (adds it to the freelist.)
 */
void CWorkerThread::ConnQueueItemFree(CQ_ITEM *item)
{
	pthread_mutex_lock(&cqi_freelist_lock);
	item->next = cqi_freelist;
	cqi_freelist = item;
	pthread_mutex_unlock(&cqi_freelist_lock);
}

/*
 * Free list management for connections.
 */
void CWorkerThread::conn_init(void)
{
	freetotal = 200;
	freecurr = 0;

	freeconns = new conn*[freetotal];

	return;
}

/*
 * Returns a connection from the freelist, if any.
 */
conn * CWorkerThread::conn_from_freelist()
{
	conn *c;

	pthread_mutex_lock(&conn_lock);
	if (freecurr > 0)
	{
		c = freeconns[--freecurr];
	}
	else
	{
		c = NULL;
	}
	pthread_mutex_unlock(&conn_lock);

	return c;
}

/*
 * Adds a connection to the freelist. 0 = success.
 */
bool CWorkerThread::conn_add_to_freelist(conn *c)
{
	bool ret = true;
	pthread_mutex_lock(&conn_lock);
	if (freecurr < freetotal)
	{
		freeconns[freecurr++] = c;
		ret = false;
	}
	else
	{
		/* try to enlarge free connections array */
		size_t newsize = freetotal * 2;
		conn **new_freeconns = new conn*[freetotal];
		if (new_freeconns)
		{
			freetotal = newsize;
			freeconns = new_freeconns;
			freeconns[freecurr++] = c;
			ret = false;
		}
	}
	pthread_mutex_unlock(&conn_lock);
	return ret;
}

/*
 * Frees a connection.
 */
void CWorkerThread::conn_free(conn *c)
{
	if (c)
	{
		utils::SafeDelete(c->rBuf);
		utils::SafeDelete(c->wBuf);
		utils::SafeDelete(c);
	}
}

void CWorkerThread::conn_close(conn *c, struct bufferevent *bev)
{
	assert(c != NULL);

	/* delete the event, the socket and the conn */
	bufferevent_free(bev);

	LOG4CXX_TRACE(g_logger, "CWorkerThread::conn_close sfd = " << c->sfd);

	/* if the connection has big buffers, just free it */
	if (conn_add_to_freelist(c))
	{
		conn_free(c);
	}

	return;
}





