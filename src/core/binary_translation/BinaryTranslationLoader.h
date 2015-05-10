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
    // Checks whether the cpu state can run the specific address at the specific mode
    static bool CanRun(u32 pc, bool tflag);
    // Runs the state provided at SetCpuState.
    // Returns instruction_count + number of instructions executed
    static uint32_t Run(uint32_t instruction_count);
    // Link between Run and VerifyCallback
    static uint32_t RunInternal(uint32_t instruction_count);
    static void VerifyCallback();
};