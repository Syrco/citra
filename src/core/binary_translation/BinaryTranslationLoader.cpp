#include "BinaryTranslationLoader.h"
#include "core/arm/skyeye_common/armdefs.h"
#include <llvm/Support/TargetSelect.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/ExecutionEngine/RuntimeDyld.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <core/mem_map.h>

using namespace llvm;

bool g_enabled = false;
bool g_verify = false;
ARMul_State *g_state;

std::unique_ptr<SectionMemoryManager> g_memory_manager;
std::unique_ptr<RuntimeDyld> g_dyld;
std::unique_ptr<RuntimeDyld::LoadedObjectInfo> g_loaded_object_info;

void(*g_run_function)();
bool(*g_can_run_function)();
uint32_t *g_instruction_count;

// Used by the verifier
struct SavedState
{
    SavedState() { }
    SavedState(const ARMul_State &state)
    {
        memcpy(regs, state.Reg, sizeof(regs));
        memcpy(flags, &state.NFlag, sizeof(flags));
        t_flag = state.TFlag;
    }
    void CopyTo(ARMul_State &state)
    {
        memcpy(state.Reg, regs, sizeof(regs));
        memcpy(&state.NFlag, flags, sizeof(flags));
        t_flag = state.TFlag;
    }
    void SwapWith(ARMul_State &state)
    {
        SavedState arm_state = state;
        std::swap(*this, arm_state);
        arm_state.CopyTo(state);
    }
    void Print()
    {
        LOG_ERROR(BinaryTranslator, "%08x %08x %08x %08x %08x %08x %08x %08x",
            regs[0], regs[1], regs[2], regs[3],
            regs[4], regs[5], regs[6], regs[7]);
        LOG_ERROR(BinaryTranslator, "%08x %08x %08x %08x %08x %08x %08x %08x",
            regs[8], regs[9], regs[10], regs[11],
            regs[12], regs[13], regs[14], regs[15]);
        LOG_ERROR(BinaryTranslator, "%01x %01x %01x %01x %01x", flags[0], flags[1], flags[2], flags[3], t_flag);
    }

    u32 regs[16];
    u32 flags[4];
    u32 t_flag;
};

bool operator==(const SavedState& lhs, const SavedState& rhs)
{
    return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

bool operator!=(const SavedState& lhs, const SavedState& rhs)
{
    return memcmp(&lhs, &rhs, sizeof(lhs)) != 0;
}

bool g_have_saved_state; // Whether there is a copied state
SavedState g_state_copy;
SavedState g_state_copy_before;

void BinaryTranslationLoader::Load(FileUtil::IOFile& file)
{
    if (offsetof(ARMul_State, ZFlag) - offsetof(ARMul_State, NFlag) != 4 ||
        offsetof(ARMul_State, CFlag) - offsetof(ARMul_State, NFlag) != 8 ||
        offsetof(ARMul_State, VFlag) - offsetof(ARMul_State, NFlag) != 12)
    {
        LOG_WARNING(Loader, "Flags are unordered, cannot run optimized file");
        return;
    }

    InitializeNativeTarget();

    g_memory_manager = make_unique<SectionMemoryManager>();
    g_dyld = make_unique<RuntimeDyld>(g_memory_manager.get());

    auto size = file.GetSize();
    auto buffer = make_unique<char[]>(size);

    file.ReadBytes(buffer.get(), size);
    if (!file.IsGood())
    {
        LOG_WARNING(Loader, "Cannot read optimized file");
        return;
    }

    auto object_file = object::ObjectFile::createObjectFile(MemoryBufferRef(StringRef(buffer.get(), size), ""));
    if (!object_file)
    {
        LOG_WARNING(Loader, "Cannot load optimized file");
        return;
    }

    g_loaded_object_info = g_dyld->loadObject(*object_file->get());
    if (g_dyld->hasError())
    {
        LOG_WARNING(Loader, "Cannot load optimized file, error %s", g_dyld->getErrorString().str().c_str());
        return;
    }

    g_dyld->resolveRelocations();
    g_dyld->registerEHFrames();
    g_memory_manager->finalizeMemory();

    g_run_function = static_cast<decltype(g_run_function)>(g_dyld->getSymbolAddress("Run"));
    g_can_run_function = static_cast<decltype(g_can_run_function)>(g_dyld->getSymbolAddress("CanRun"));
    auto verify_ptr = static_cast<bool*>(g_dyld->getSymbolAddress("Verify"));
    g_instruction_count = static_cast<uint32_t *>(g_dyld->getSymbolAddress("InstructionCount"));
    auto memory_read_32_ptr = static_cast<decltype(&Memory::Read32) *>(g_dyld->getSymbolAddress("Memory::Read32"));

    if (!g_run_function || !g_can_run_function || !verify_ptr || !g_instruction_count || !memory_read_32_ptr)
    {
        LOG_WARNING(Loader, "Cannot load optimized file, missing critical function");
        return;
    }

    g_verify = *verify_ptr;
    *memory_read_32_ptr = &Memory::Read32;

    g_enabled = true;

    LOG_INFO(Loader, "Binary translation enabled");
}

void BinaryTranslationLoader::SetCpuState(ARMul_State* state)
{
    if (!g_enabled) return;

    auto regs_ptr = static_cast<u32 **>(g_dyld->getSymbolAddress("Registers"));
    auto flags_ptr = static_cast<u32 **>(g_dyld->getSymbolAddress("Flags"));

    *regs_ptr = state->Reg;
    *flags_ptr = &state->NFlag;
    g_have_saved_state = false;
    g_state = state;
}

bool BinaryTranslationLoader::CanRun(bool specific_address)
{
    if (!g_enabled) return false;
    // Thumb not implemented
    if (g_state->TFlag) return false;
    if (specific_address)
        if (!g_can_run_function()) return false;
    return true;
}

bool BinaryTranslationLoader::CanRun(u32 pc, bool tflag)
{
    if (!g_enabled) return false;
    if (tflag) return false;
    std::swap(g_state->Reg[15], pc);
    auto result = g_can_run_function();
    std::swap(g_state->Reg[15], pc);
    return result;
}

uint32_t BinaryTranslationLoader::Run(uint32_t instruction_count)
{
    // No need to check the PC, Run does it anyway
    if (!CanRun(false)) return instruction_count;
    // If verify is enabled, it will run opcodes
    if (g_verify) return instruction_count;

    return RunInternal(instruction_count);
}

uint32_t BinaryTranslationLoader::RunInternal(uint32_t instruction_count)
{
    *g_instruction_count = instruction_count;
    g_run_function();

    g_state->TFlag = g_state->Reg[15] & 1;
    if (g_state->TFlag)
        g_state->Reg[15] &= 0xfffffffe;
    else
        g_state->Reg[15] &= 0xfffffffc;

    return *g_instruction_count;
}

void Swap(void *a, void *b, size_t size)
{
    auto a_char = (char *)a;
    auto b_char = (char *)b;
    for (auto i = 0; i < size; ++i)
    {
        std::swap(a_char[i], b_char[i]);
    }
}

void BinaryTranslationLoader::VerifyCallback()
{
    if (!g_enabled || !g_verify) return;

    // Swap the PC and TFlag to the old state before checking if it can run
    std::swap(g_state_copy.regs[15], g_state->Reg[15]);
    std::swap(g_state_copy.t_flag, g_state->TFlag);
    auto can_run = CanRun(true);
    std::swap(g_state_copy.regs[15], g_state->Reg[15]);
    std::swap(g_state_copy.t_flag, g_state->TFlag);

    if (g_have_saved_state && can_run)
    {
        // An opcode is finished, simulate it

        // Copy the state before
        g_state_copy_before = g_state_copy;

        // Swap to the state before the opcode
        g_state_copy.SwapWith(*g_state);

        // Run the opcode
        RunInternal(0);

        // Test
        auto current_as_saved_state = SavedState(*g_state);
        if (current_as_saved_state != g_state_copy)
        {
            LOG_ERROR(BinaryTranslator, "Verify failed");
            LOG_ERROR(BinaryTranslator, "Regs Before");
            g_state_copy_before.Print();
            LOG_ERROR(BinaryTranslator, "Regs OK");
            g_state_copy.Print();
            LOG_ERROR(BinaryTranslator, "Regs not OK");
            current_as_saved_state.Print();

            // Don't spam
            g_enabled = false;

            // Make sure it has a valid state to continue to run
            g_state_copy.CopyTo(*g_state);
        }
    }
    else
    {
        // If this opcode is not translated or there is no saved state, just save the state and continue
        g_state_copy = *g_state;

        g_have_saved_state = true;
    }
}