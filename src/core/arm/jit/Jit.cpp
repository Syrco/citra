#include "Jit.h"
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/MC/MCDisassembler.h>
#include <llvm/MC/MCContext.h>
#include <llvm/MC/MCRegisterInfo.h>
#include <llvm/MC/MCAsmInfo.h>
#include <llvm/Support/Host.h>
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Object/ObjectFile.h"
#include <Target/ARM/MCTargetDesc/ARMMCTargetDesc.h>
#include <core/mem_map.h>
#include <core/arm/skyeye_common/armdefs.h>
#include <core/arm/dyncom/arm_dyncom_thumb.h>
#include <fstream>
#include <iostream>

using namespace llvm;
using namespace std;

#pragma optimize( "", off )

struct Jit::Private
{
	Jit::Private();

	std::unique_ptr<SectionMemoryManager> memoryManager;
	std::unique_ptr<RuntimeDyld> dyld;
	std::unique_ptr<RuntimeDyld::LoadedObjectInfo> object;

	void(*entry)(u32 *regs, u32 *flags);
	bool(*preset)(u32 addr);

	bool CanRun(u32 pc);
	void Run(struct ARMul_State* cpu);
};

Jit::Jit()
{

	priv = std::make_unique<Private>();
}

Jit::Private::Private()
{
	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();
	InitializeNativeTargetAsmParser();

	memoryManager = llvm::make_unique<SectionMemoryManager>();
	dyld = llvm::make_unique<RuntimeDyld>(memoryManager.get());

	int fd;
	std::ifstream file("../opt/3dscraft.obj", ios::binary);
	file.seekg(0, ios::end);
	auto size = file.tellg();
	file.seekg(0, ios::beg);

	auto buffer = new char[size];
	file.read(buffer, size);

	auto loadedObject = object::ObjectFile::createObjectFile(MemoryBufferRef(StringRef(buffer, size), ""));
	if (!loadedObject)
	{
		LOG_CRITICAL(Frontend, "Failed to load file: error %s", loadedObject.getError().message().c_str());
		__debugbreak();
	}
	object = dyld->loadObject(*loadedObject->get());
	if (dyld->hasError()) __debugbreak();

	dyld->resolveRelocations();
	dyld->registerEHFrames();
	memoryManager->finalizeMemory();

	entry = (decltype(entry))dyld->getSymbolAddress("Run");
	if (!entry) __debugbreak();

	preset = (decltype(preset))dyld->getSymbolAddress("Present");
	if (!entry) __debugbreak();

	if (offsetof(ARMul_State, ZFlag) - offsetof(ARMul_State, NFlag) != 4) __debugbreak();
	if (offsetof(ARMul_State, CFlag) - offsetof(ARMul_State, NFlag) != 8) __debugbreak();
	if (offsetof(ARMul_State, VFlag) - offsetof(ARMul_State, NFlag) != 12) __debugbreak();
}

bool Jit::CanRun(u32 pc)
{
	return priv->CanRun(pc);
}

void Jit::Run(ARMul_State* state)
{
	priv->Run(state);
}

bool Jit::Private::CanRun(u32 pc)
{
	return preset(pc);
}

void Jit::Private::Run(ARMul_State *state)
{
	entry(state->Reg, &state->NFlag);
}

void Jit::BeforeFindBB(struct ARMul_State* cpu)
{
#if 0
	static std::ofstream runRecord("run.txt", ios::binary);
	if (!runRecord) __debugbreak();
	static size_t opcodes = 0;
	/*char buf[10];
	for (auto i = 0; i < 16; ++i)
	{
		snprintf(buf, 10, "%08x ", cpu->Reg[i]);
		runRecord << buf;
	}
	runRecord << (cpu->NFlag ? "N" : " ");
	runRecord << (cpu->ZFlag ? "Z" : " ");
	runRecord << (cpu->CFlag ? "C" : " ");
	runRecord << (cpu->VFlag ? "V" : " ");
	runRecord << "\n";*/

	char buf[] = "00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000     \n";
	for (auto i = 0; i < 16; ++i)
	{
		sprintf(buf + i * 9, "%08x ", cpu->Reg[i]);
	}
	buf[16 * 9] = ' ';
	if (cpu->NFlag) buf[16 * 9 + 0] = 'N';
	if (cpu->ZFlag) buf[16 * 9 + 1] = 'Z';
	if (cpu->CFlag) buf[16 * 9 + 2] = 'C';
	if (cpu->VFlag) buf[16 * 9 + 3] = 'V';
	runRecord.write(buf, sizeof(buf) - 1);

	++opcodes;
	if (opcodes > 0x80000)
	{
		runRecord.flush();
		runRecord.close();
		_exit(0);
	}
#endif
}