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

	/*llvm::ExecutionEngine *executionEngine;
	IRBuilder<> *builder;
	const Target *armTarget;
	MCSubtargetInfo *armSubtarget;
	const MCDisassembler *armDisassembler;
	const MCRegisterInfo *armRegisterInfo;
	const MCAsmInfo *armAsmInfo;
	MCContext *armContext;
	Module *module;

	struct ARMul_State *state;

	using CompiledBlock = void(*)();

	vector<unique_ptr<class JitBlock>> jitBlocks;
	std::map<int, CompiledBlock> compiledBlocks;
	*/

	void(*entry)(u32 *regs);
	bool InterpreterTranslate(struct ARMul_State* cpu, u32 addr);
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

	entry = (decltype(entry))dyld->getSymbolAddress("entry");
	if (!entry) __debugbreak();

	/*auto module = llvm::make_unique<Module>("Module", getGlobalContext());
	module->setTargetTriple(sys::getProcessTriple() + "-elf");
	this->module = module.get();
	std::string errorString;
	executionEngine = EngineBuilder(std::move(module))
		.setErrorStr(&errorString)
		.setMCJITMemoryManager(llvm::make_unique<SectionMemoryManager>())
		.create();
	if (!executionEngine) __debugbreak();

	builder = new IRBuilder<>(getGlobalContext());

	StringRef triple = "armv6k-none-eabi";

	armTarget = TargetRegistry::lookupTarget(triple, errorString);
	if (!armTarget) __debugbreak();

	armSubtarget = armTarget->createMCSubtargetInfo(triple, "armv6k", "");
	if (!armSubtarget) __debugbreak();

	armRegisterInfo = armTarget->createMCRegInfo(triple);
	if (!armRegisterInfo) __debugbreak();

	armAsmInfo = armTarget->createMCAsmInfo(*armRegisterInfo, triple);
	if (!armAsmInfo) __debugbreak();

	armContext = new MCContext(armAsmInfo, armRegisterInfo, nullptr);

	armDisassembler = armTarget->createMCDisassembler(*armSubtarget, *armContext);
	if (!armDisassembler) __debugbreak();*/
}

bool Jit::InterpreterTranslate(ARMul_State* state, u32 addr)
{
	return priv->InterpreterTranslate(state, addr);
}

bool Jit::Private::InterpreterTranslate(ARMul_State *state, u32 addr)
{
	entry(state->Reg);
	return true;
}
