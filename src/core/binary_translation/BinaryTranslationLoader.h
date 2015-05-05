#include "common/common.h"
#include "common/file_util.h"
#include <memory>

struct ARMul_State;

class BinaryTranslationLoader
{
public:
    static void Load(FileUtil::IOFile& file);
    static void SetCpuState(ARMul_State *state);
    // Checks whether the cpu state can be run
    // If specific_address, checks the specific PC too
    static bool CanRun(bool specific_address);
    // Runs the state provided at SetCpuState.
    static void Run();
    // Link between Run and VerifyCallback
    static void RunInternal();
    static void VerifyCallback();
};