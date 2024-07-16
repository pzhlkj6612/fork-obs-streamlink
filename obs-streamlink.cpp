#include "obs-streamlink.h"

#include "utils.hpp"

#include <obs-module.h>

#include "python-streamlink.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-streamlink", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Streamlink Source";
}

extern "C" obs_source_info streamlink_source_info;
std::filesystem::path obs_streamlink_data_path;

bool obs_module_load(void)
{
	FF_LOG(LOG_INFO, "6666666666666666666666666666666666666");

	std::string data_path = obs_get_module_data_path(obs_current_module());
	obs_streamlink_data_path = data_path;
	// if (!std::filesystem::exists(obs_streamlink_data_path / obs_streamlink_python_ver))
	// {
	// 	FF_LOG(LOG_ERROR, "Failed to initialize streamlink source!! Python38 not found in plugin data path.");
	// 	return false;
	// }

	streamlink::Initialize();
	obs_register_source(&streamlink_source_info);
	return true;
}

