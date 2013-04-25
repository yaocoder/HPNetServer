#include "defines.h"
#include "utils.h"
#include "config_file.h"
#include "init_configure.h"
#include "global_settings.h"
#include "master_thread.h"

static void InitConfigure();
static void SettingsAndPrint();
static void Run();

int main(void)
{
	InitConfigure();

	SettingsAndPrint();

	Run();

	return EXIT_SUCCESS;
}

void Run()
{
	CMasterThread masterThread;
	if(!masterThread.InitMasterThread())
	{
		LOG4CXX_FATAL(g_logger, "InitNetCore failed.");
		exit(EXIT_FAILURE);
	}

	masterThread.Run();
}

void InitConfigure()
{
	CInitConfig initConfObj;
	std::string current_path;
	assert(utils::GetCurrentPath(current_path) == true);
	initConfObj.SetConfigFilePath(std::string(current_path));
	initConfObj.InitLog4cxx("RRMode");
	if (!initConfObj.LoadConfiguration())
	{
		LOG4CXX_FATAL(g_logger, "LoadConfiguration failed.");
		exit(EXIT_FAILURE);
	}
}

void SettingsAndPrint()
{
	utils::G<CGlobalSettings>().remote_listen_port_ = utils::G<ConfigFile>().read<int> ("remote.listen.port", 12006);
	utils::G<CGlobalSettings>().thread_num_ = utils::G<ConfigFile>().read<int> ("worker.thread.num", 4);
	utils::G<CGlobalSettings>().client_heartbeat_timeout_ = utils::G<ConfigFile>().read<int>("client.heartbeat.timeout.s", 70);

	LOG4CXX_INFO(g_logger, "******remote.listen.port=" << utils::G<CGlobalSettings>().remote_listen_port_ << "******");
	LOG4CXX_INFO(g_logger, "******worker.thread.num =" << utils::G<CGlobalSettings>().thread_num_ << "******");
	LOG4CXX_INFO(g_logger, "******client.heartbeat.timeout.s =" << utils::G<CGlobalSettings>().client_heartbeat_timeout_ << "******");
}
