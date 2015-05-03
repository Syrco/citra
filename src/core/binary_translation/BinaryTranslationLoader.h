#include "common/common.h"
#include "common/file_util.h"
#include <memory>

struct ARMul_State;

class BinaryTranslationLoader
{
public:
    static void Load(FileUtil::IOFile& file);
    static void SetCpuState(ARMul_State *state);
    // Runs the state provided at SetCpuState.
    static void Run();
    static void VerifyCallback();
};