/*
 * defines.h
 *
 *  Created on: 2012-9-12
 *      Author: yaowei
 */

#ifndef DEFINES_H_
#define DEFINES_H_

#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <arpa/inet.h>

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/thread.hpp>

#include <log4cxx/logger.h>
#include <log4cxx/logstring.h>
#include <log4cxx/propertyconfigurator.h>
using namespace log4cxx;
extern LoggerPtr g_logger;

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
            TypeName(const TypeName&); \
            void operator=(const TypeName&)

#include <event.h>



#endif /* DEFINES_H_ */
