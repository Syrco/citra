#include <iostream>
#include "common/common.h"
#include "core/loader/loader.h"
#include "common/scope_exit.h"
#include "citra/config.h"
#include "common/logging/text_formatter.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "core/core.h"
#include "core/mem_map.h"
#include "core/arm/arm_interface.h"

#include "Codegen.h"

using namespace std;

int main(int argc, const char *const *argv)
{
	std::shared_ptr<Log::Logger> logger = Log::InitGlobalLogger();
	Log::Filter log_filter(Log::Level::Debug);
	Log::SetFilter(&log_filter);
	std::thread logging_thread(Log::TextLoggingLoop, logger);
	SCOPE_EXIT({
		logger->Close();
		logging_thread.join();
	});

	if (argc < 3)
	{
		LOG_CRITICAL(Frontend, "Failed to load ROM: No ROM specified");
		return -1;
	}
	auto boot_filename = argv[1];
	auto output_filename = argv[2];

	Core::Init();
	//CoreTiming::Init();
	Memory::Init();
	//HW::Init();
	//Kernel::Init();
	//HLE::Init();
	//VideoCore::Init(emu_window);

	Loader::ResultStatus load_result = Loader::LoadFile(boot_filename);
	if (Loader::ResultStatus::Success != load_result)
	{
		LOG_CRITICAL(Frontend, "Failed to load ROM (Error %i)!", load_result);
		return -1;
	}

	auto pc = Core::g_app_core->GetPC();

	cout << "Running from " << hex << pc << endl;
	Codegen *codegen = new Codegen;
	codegen->Run(output_filename);

	return 0;
}