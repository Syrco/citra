#include "BinaryTranslationLoader.h"
#include "core/arm/skyeye_common/armdefs.h"
#include <llvm/Support/TargetSelect.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/ExecutionEngine/RuntimeDyld.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>

using namespace llvm;

bool g_enabled = false;
bool g_verify = false;

std::unique_ptr<SectionMemoryManager> g_memory_manager;
std::unique_ptr<RuntimeDyld> g_dyld;
std::unique_ptr<RuntimeDyld::LoadedObjectInfo> g_loaded_object_info;

void (*g_run_function)();

// Used by the verifier
bool (*g_can_run_function)();
bool g_have_saved_state; // Whether there is a copied state
ARMul_State *g_state;
u32 g_regs_copy[16];
u32 g_flags_copy[4];
u32 g_regs_copy_before[16];
u32 g_flags_copy_before[4];

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
    g_verify = *static_cast<bool*>(g_dyld->getSymbolAddress("Verify"));

    g_enabled = true;
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

void BinaryTranslationLoader::Run()
{
    // If verify is enabled, it will run opcodes
    if (!g_enabled || g_verify) return;

    g_run_function();
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

void ShowRegs(u32 *regs, u32 *flags)
{
    LOG_ERROR(BinaryTranslator, "%08x %08x %08x %08x %08x %08x %08x %08x",
        regs[0], regs[1], regs[2], regs[3],
        regs[4], regs[5], regs[6], regs[7]);
    LOG_ERROR(BinaryTranslator, "%08x %08x %08x %08x %08x %08x %08x %08x",
        regs[8], regs[9], regs[10], regs[11],
        regs[12], regs[13], regs[14], regs[15]);
    LOG_ERROR(BinaryTranslator, "%01x %01x %01x %01x", flags[0], flags[1], flags[2], flags[3]);
}

void BinaryTranslationLoader::VerifyCallback()
{
    if (!g_enabled || !g_verify) return;

    // Swap the PC to the old state before checking if it can run
    std::swap(g_regs_copy[15], g_state->Reg[15]);
    auto can_run = g_can_run_function();
    std::swap(g_regs_copy[15], g_state->Reg[15]);

    if (g_have_saved_state && can_run)
    {
        // An opcode is finished, simulate it

        // Copy the state before
        memcpy(g_regs_copy_before, g_regs_copy, sizeof(g_regs_copy));
        memcpy(g_flags_copy_before, &g_flags_copy, sizeof(g_flags_copy));

        // Swap to the state before the opcode
        Swap(g_state->Reg, g_regs_copy, sizeof(g_regs_copy));
        Swap(&g_state->NFlag, g_flags_copy, sizeof(g_flags_copy));

        // Run the opcode
        g_run_function();

        // Test
        if (memcmp(g_state->Reg, g_regs_copy, sizeof(g_regs_copy)) || memcmp(&g_state->NFlag, g_flags_copy, sizeof(g_flags_copy)))
        {
            LOG_ERROR(BinaryTranslator, "Verify failed");
            LOG_ERROR(BinaryTranslator, "Regs Before");
            ShowRegs(g_regs_copy_before, g_flags_copy_before);
            LOG_ERROR(BinaryTranslator, "Regs OK");
            ShowRegs(g_regs_copy, g_flags_copy);
            LOG_ERROR(BinaryTranslator, "Regs not OK");
            ShowRegs(g_state->Reg, &g_state->NFlag);

            // Don't spam
            g_enabled = false;

            // Make sure it has a valid state to continue to run
            Swap(g_state->Reg, g_regs_copy, sizeof(g_regs_copy));
            Swap(&g_state->NFlag, g_flags_copy, sizeof(g_flags_copy));
        }
    }
    else
    {
        // If this opcode is not translated or there is no saved state, just save the state and continue
        memcpy(g_regs_copy, g_state->Reg, sizeof(g_regs_copy));
        memcpy(g_flags_copy, &g_state->NFlag, sizeof(g_flags_copy));

        g_have_saved_state = true;
    }
}