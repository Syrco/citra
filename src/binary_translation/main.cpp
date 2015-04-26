#include "common/logging/backend.h"
#include "common/logging/text_formatter.h"
#include "common/scope_exit.h"
#include "core/core.h"
#include "core/mem_map.h"
#include "core/loader/loader.h"
#include "codegen.h"

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
        LOG_CRITICAL(BinaryTranslator, "Usage: binary_translate <input_rom> <output_object> [<output_debug>]");
		return -1;
	}

	auto input_rom = argv[1];
	auto output_object = argv[2];
	auto output_debug = argc > 3 ? argv[3] : nullptr;

	Core::Init();
	Memory::Init();

	auto load_result = Loader::LoadFile(input_rom);
	if (Loader::ResultStatus::Success != load_result)
	{
        LOG_CRITICAL(BinaryTranslator, "Failed to load ROM (Error %i)!", load_result);
		return -1;
	}

	CodeGen code_generator(output_object, output_debug);
	code_generator.Run();
}