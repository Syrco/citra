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

std::unique_ptr<SectionMemoryManager> g_memory_manager;
std::unique_ptr<RuntimeDyld> g_dyld;
std::unique_ptr<RuntimeDyld::LoadedObjectInfo> g_loaded_object_info;

void (*g_run_function)();

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

    g_enabled = true;
}

void BinaryTranslationLoader::SetCpuState(ARMul_State* state)
{
    if (!g_enabled) return;

    auto regs_ptr = static_cast<u32 **>(g_dyld->getSymbolAddress("Registers"));
    auto flags_ptr = static_cast<u32 **>(g_dyld->getSymbolAddress("Flags"));

    *regs_ptr = state->Reg;
    *flags_ptr = &state->NFlag;
}

void BinaryTranslationLoader::Run()
{
    if (!g_enabled) return;

    g_run_function();
}