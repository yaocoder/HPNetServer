/*
 * net_core.cc
 *
 *  Created on: Mar 4, 2013
 *      Author: yaowei
 */

#include "master_thread.h"
#include "global_settings.h"
#include "utils.h"
#include "worker_threads.h"
#include "socket_wrapper.h"

CMasterThread::CMasterThread()
{
	main_base_ 		= NULL;
	listen_event_	= NULL;
	remote_listen_socket_ = 0;
	worker_thread_ptr_ 	  = new CWorkerThread;
}

CMasterThread::~CMasterThread()
{
	utils::SafeDelete(worker_thread_ptr_);
	event_base_free(main_base_);
}

bool CMasterThread::CheckLibeventVersion()
{
	const char* libevent_version = event_get_version();
	assert(libevent_version != NULL);

	LOG4CXX_TRACE(g_logger, "The libevent version is " << libevent_version);

	if (strncmp(libevent_version, "2", 1) == 0)
		return true;
	else
		return false;
}

bool CMasterThread::InitMasterThread()
{
	if(!CheckLibeventVersion())
	{
		LOG4CXX_ERROR(g_logger, "CMasterThread::InitMasterThread:The libevent version Require at  2.0.*");
		return false;
	}

	main_base_ = event_base_new();
	assert(main_base_ != NULL);

	/* 监听来自客户端的连接 */
	if(!InitRemoteListenSocket(remote_listen_socket_))
		return false;

	evutil_make_socket_nonblocking(remote_listen_socket_);

	listen_event_ = event_new(main_base_, remote_listen_socket_, EV_READ|EV_PERSIST, AccepCb, (void*)this);
	assert(listen_event_ != NULL);

	if(event_add(listen_event_, NULL) == -1)
	{
		int error_code = EVUTIL_SOCKET_ERROR();
		LOG4CXX_ERROR(g_logger, "CMasterThread::InitMasterThread:event_add errorCode = " << error_code
				      	  	  	 << ", description = " << evutil_socket_error_to_string(error_code));
		return false;
	}

	/* 创建worker线程，用来处理来自客户端的连接 */
	if(!worker_thread_ptr_->InitThreads(main_base_))
		return false;

	return true;
}

void CMasterThread::Run()
{

	LOG4CXX_INFO(g_logger, "CMasterThread::Run:Master thread has start...");

	int ret = event_base_dispatch(main_base_);
	if (-1 == ret)
	{
		int error_code = EVUTIL_SOCKET_ERROR();
		LOG4CXX_FATAL(g_logger, "CMasterThread::Run():event_base_dispatch errorCode = " << error_code
					 << ", description = " << evutil_socket_error_to_string(error_code));
		exit(1);
	}
	else if(1 == ret)
	{
		LOG4CXX_FATAL(g_logger, "CMasterThread::Run():no events were registered.");
		exit(1);
	}
}

void CMasterThread::AccepCb(evutil_socket_t listen_socket, short event, void* arg)
{
	CMasterThread* pThis = static_cast<CMasterThread*>(arg);

	evutil_socket_t sfd;
	struct sockaddr_in sin;
	socklen_t slen = sizeof(sin);

	sfd = accept(listen_socket, (struct sockaddr *) &sin, &slen);
	if (-1 == sfd)
	{
		LOG4CXX_WARN(g_logger, "CMasterThread::AccepCb:accept error = " << strerror(errno));
		return;
	}


	if (!SocketOperate::SetSocketNoBlock(sfd))
	{
		LOG4CXX_WARN(g_logger, "CMasterThread::AccepCb:SetSocketNoBlock error = " << strerror(errno))
		close(sfd);
		return;
	}

	/* 将客户端新连接分发到各个工作线程 */
	pThis->worker_thread_ptr_->DispatchSfdToWorker(sfd);
}

bool CMasterThread::InitRemoteListenSocket(evutil_socket_t& listen_socket)
{
	listen_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_socket < 0)
	{
		LOG4CXX_ERROR(g_logger, "CMasterThread::InitRemoteListenSocket:socket error = " << strerror(errno));
		return false;
	}

	int flags = 1;
	if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (void *) &flags, sizeof(flags)) != 0)
	{
		LOG4CXX_ERROR(g_logger, "CMasterThread::InitRemoteListenSocket:setsockopt SO_REUSEADDR error = " << strerror(errno));
		close(listen_socket);
		return false;
	}

	if (setsockopt(listen_socket, IPPROTO_TCP, TCP_NODELAY, (void *) &flags, sizeof(flags)) != 0)
	{
		LOG4CXX_ERROR(g_logger, "CMasterThread::InitRemoteListenSocket:setsockopt TCP_NODELAY error = " << strerror(errno));
		close(listen_socket);
		return false;
	}

	sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(utils::G<CGlobalSettings>().remote_listen_port_ );

	if (bind(listen_socket, (const sockaddr*)&servaddr, sizeof(servaddr)) != 0)
	{
		LOG4CXX_ERROR(g_logger, "CMasterThread::InitRemoteListenSocket:bind error = " << strerror(errno));
		close(listen_socket);
		return false;
	}

	if(listen(listen_socket, BACKLOG) != 0)
	{
	    LOG4CXX_ERROR(g_logger, "CMasterThread::InitRemoteListenSocket:Listen error = " << strerror(errno));
	    close(listen_socket);
	    return false;
	}

	return true;
}


