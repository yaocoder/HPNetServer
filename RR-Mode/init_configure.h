/*
 * init_log4cxx.h
 *
 *  Created on: 2012-9-12
 *      Author: yaowei
 */

#ifndef INIT_LOG4CXX_H_
#define INIT_LOG4CXX_H_

#include "defines.h"

class CInitConfig
{
public:
	CInitConfig();
	virtual ~CInitConfig();

public:

	void InitLog4cxx(std::string project_name);

	bool LoadConfiguration();

	void SetConfigFilePath(const std::string& config_file_path)
	{
		config_file_path_ = config_file_path;
	}

private:

	DISALLOW_COPY_AND_ASSIGN(CInitConfig);
	std::string config_file_path_;
};

#endif /* INIT_LOG4CXX_H_ */
