/**
 * @created:	   		2012/05/16
 *
 * @file
 *
 * @author  			wei yao <yaocoder@gmail.com>
 *
 * @version 			1.0
 *
 * @LICENSE
 *
 * @brief				通用工具方法
 *
 */
#ifndef UTILS_H__
#define UTILS_H__

#include <iostream>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <exception>
#include <unistd.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <vector>
#include <boost/filesystem.hpp>
using namespace std;

namespace utils
{

static inline bool GetCurrentPath(std::string& current_path)
{
	try
	{
		boost::filesystem::path path = boost::filesystem::current_path();
		current_path = path.string();
		return true;
	}
	catch (boost::filesystem::filesystem_error & e)
	{
		cout << "current_path : " << current_path << ", error description :" << e.what() << endl;
		return false;
	}
}

static inline std::string int2str(int v)
{
	std::stringstream ss;
	ss << v;
	return ss.str();
}

template<class T>
class Singleton: private T
{
public:
	static T &Instance()
	{
		static Singleton<T> _instance;
		return _instance;
	}
private:
	Singleton()
	{
	}
	~Singleton()
	{
	}
};

template<typename T>
T& G()
{
	return Singleton<T>::Instance();
}

template<typename T> inline void SafeDeleteArray(T*& p)
{
	if (NULL != p)
	{
		delete[] p;
		p = 0;
	}
}

template<typename T> inline void SafeDelete(T*& p)
{
	if (NULL != p)
	{
		delete p;
		p = 0;
	}
}

}
#endif
