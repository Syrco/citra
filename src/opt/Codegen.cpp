#include "Codegen.h"
#include "CodeBlock.h"
#include <llvm/Support/TargetSelect.h>
#include <llvm/MC/MCSubtargetInfo.h>
#include <llvm/MC/MCDisassembler.h>
#include <llvm/MC/MCRegisterInfo.h>
#include <llvm/MC/MCAsmInfo.h>
#include <llvm/MC/MCContext.h>
#include <llvm/MC/MCInstrInfo.h>
#include <llvm/MC/MCInstPrinter.h>
#include <llvm/Target/TargetSubtargetInfo.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/PassManager.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Target/TargetLibraryInfo.h>
#include <core/mem_map.h>
#include "common/logging/text_formatter.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "Instructions.h"
#include <iostream>
#include "Decode.h"

using namespace llvm;
using namespace std;

Codegen::Codegen() : decoder(new Decoder(this))
{
	// Arm
	LLVMInitializeARMTargetInfo();
	LLVMInitializeARMTarget();
	LLVMInitializeARMTargetMC();
	LLVMInitializeARMDisassembler();
	LLVMInitializeARMAsmPrinter();

	armContext.reset(new LLVMContext());

	std::string errorString;
	auto armTriple = "armv6-none-eabi";
	armTarget = TargetRegistry::lookupTarget(armTriple, errorString);
	if (!armTarget) __debugbreak();

	armSubtarget.reset(armTarget->createMCSubtargetInfo(armTriple, "cortex-a8", ""));
	if (!armSubtarget) __debugbreak();

	armRegisterInfo.reset(armTarget->createMCRegInfo(armTriple));
	if (!armRegisterInfo) __debugbreak();

	armAsmInfo.reset(armTarget->createMCAsmInfo(*armRegisterInfo, armTriple));
	if (!armAsmInfo) __debugbreak();

	armMCContext = llvm::make_unique<MCContext>(armAsmInfo.get(), armRegisterInfo.get(), nullptr);

	armDisassembler.reset(armTarget->createMCDisassembler(*armSubtarget, *armMCContext));
	if (!armDisassembler) __debugbreak();

	armInstrInfo.reset(armTarget->createMCInstrInfo());
	if (!armInstrInfo) __debugbreak();

	armInstPrinter.reset(armTarget->createMCInstPrinter(0, *armAsmInfo, *armInstrInfo, *armRegisterInfo, *armSubtarget));
	if (!armInstPrinter) __debugbreak();

	// Native
	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();
	InitializeNativeTargetAsmParser();
	nativeContext.reset(new LLVMContext());

	auto nativeTripleStr = sys::getProcessTriple();
#ifdef _WIN32
	nativeTripleStr += "-elf";
#endif
	nativeTriple.setTriple(nativeTripleStr);
	EngineBuilder eb(llvm::make_unique<Module>("Decoy module", *nativeContext));
	nativeTarget.reset(eb.selectTarget(nativeTriple, "", "", llvm::SmallVector<std::string, 0>()));
	if (!nativeTarget) __debugbreak();

	module = llvm::make_unique<Module>("Module", *nativeContext);
	module->setTargetTriple(nativeTripleStr);
	irBuilder = llvm::make_unique<IRBuilder<>>(*nativeContext);
	Type *arguments = {
		Type::getInt32PtrTy(*nativeContext)
	};
	codeBlockFunctionSignature = FunctionType::get(Type::getVoidTy(*nativeContext), arguments, false);
}

Codegen::~Codegen()
{

}

static const size_t TranslateStart = Memory::EXEFS_CODE_VADDR, TranslateSize = 0x25000, TranslateEnd = TranslateStart + TranslateSize;

void Codegen::Run(const char *filename)
{
	auto switchArrSize = TranslateSize / 4;
	auto switchArrMemberType = irBuilder->getInt8PtrTy();
	auto switchArrType = ArrayType::get(switchArrMemberType, switchArrSize);

	function.reset(Function::Create(codeBlockFunctionSignature, GlobalValue::LinkageTypes::ExternalLinkage, "entry", module.get()));
	registersArgument = &function->getArgumentList().front();
	//Value *arguments[] = { registers };
	//ArrayRef<Value *> argumentsRef(arguments);

	auto switchBasicBlock = BasicBlock::Create(*nativeContext, "switch", function.get());
	auto exitBasicBlock = BasicBlock::Create(*nativeContext, "exit", function.get());
	auto notHigher = BasicBlock::Create(*nativeContext, "notHigher", function.get());
	auto notNull = BasicBlock::Create(*nativeContext, "notNull", function.get());
	outOfCodeblock = BasicBlock::Create(*nativeContext, "outOfCodeblock", function.get());

	irBuilder->SetInsertPoint(notNull);
	CreateRegisters();
	TranslateBlocks();

	auto switchLocalArr = new Constant*[switchArrSize];
	std::fill(switchLocalArr, switchLocalArr + switchArrSize, ConstantPointerNull::get(switchArrMemberType));
	for (auto block : blocks)
	{
		switchLocalArr[(block.second - TranslateStart) / 4] = BlockAddress::get(function.get(), block.first);
	}
	auto switchArr = ConstantArray::get(switchArrType, ArrayRef<Constant*>(switchLocalArr, switchLocalArr + switchArrSize));
	auto switchArrGV = new GlobalVariable(*module, switchArr->getType(), true, GlobalValue::InternalLinkage, switchArr, "SwitchArray");

	irBuilder->SetInsertPoint(switchBasicBlock);
	auto pc = irBuilder->CreateLoad(irBuilder->CreateConstGEP1_32(registersArgument, 15));
	auto pcSubStart = irBuilder->CreateSub(pc, irBuilder->getInt32(TranslateStart));
	auto pcSubStartDiv4 = irBuilder->CreateAShr(pcSubStart, 2, "");
	auto higher = irBuilder->CreateICmpUGE(pcSubStartDiv4, irBuilder->getInt32(TranslateEnd / 4));
	auto iff = irBuilder->CreateCondBr(higher, exitBasicBlock, notHigher);

	irBuilder->SetInsertPoint(notHigher);
	Value *values[] = { irBuilder->getInt32(0), pcSubStartDiv4 };
	auto addr = irBuilder->CreateLoad(irBuilder->CreateGEP(switchArrGV, values));
	auto cmp = irBuilder->CreateICmpNE(addr, ConstantPointerNull::get(switchArrMemberType));
	auto iff2 = irBuilder->CreateCondBr(cmp, notNull, exitBasicBlock);

	irBuilder->SetInsertPoint(notNull);
	auto indirectBr = irBuilder->CreateIndirectBr(addr, switchArrSize);
	for (auto block : blocks)
	{
		indirectBr->addDestination(block.first);
	}

	irBuilder->SetInsertPoint(outOfCodeblock);
	StoreRegisters();
	irBuilder->CreateBr(exitBasicBlock);

	irBuilder->SetInsertPoint(exitBasicBlock);
	irBuilder->CreateRetVoid();

	raw_os_ostream os(std::cout);

	/*if (verifyModule(*module, &os))
	{
		os.flush();
		std::cin.get();
	}*/

	/*{
		std::error_code error;
		raw_fd_ostream file(string(filename) + ".ll", error, sys::fs::OpenFlags::F_RW);

		if (file.has_error())
		{
			LOG_CRITICAL(Frontend, "Failed to write bitcode file: error %s", error.message().c_str());
		}

		module->print(file, nullptr);
	}*/

	WriteFile(filename);

	/*passManager.add(createVerifierPass());
	passManager.add(createBasicAliasAnalysisPass());
	passManager.add(createInstructionCombiningPass());
	passManager.add(createReassociatePass());
	passManager.add(createGVNPass());
	passManager.add(createCFGSimplificationPass());
	passManager.add(createPromoteMemoryToRegisterPass());*/

}

void Codegen::WriteFile(const char *filename)
{
	PassManager passManager;
	FunctionPassManager functionPassManager(module.get());
	PassManagerBuilder passManagerBuilder;

	module->setDataLayout(nativeTarget->getSubtargetImpl()->getDataLayout());

	passManager.add(createVerifierPass());
	passManager.add(new TargetLibraryInfo(nativeTriple));
	passManager.add(new DataLayoutPass());
	nativeTarget->addAnalysisPasses(passManager);
	functionPassManager.add(new DataLayoutPass());
	nativeTarget->addAnalysisPasses(functionPassManager);

	passManagerBuilder.OptLevel = 3;
	passManagerBuilder.SizeLevel = 0;
	passManagerBuilder.Inliner = createFunctionInliningPass(3, 0);
	passManagerBuilder.LoopVectorize = true;
	passManagerBuilder.SLPVectorize = true;

	passManagerBuilder.populateFunctionPassManager(functionPassManager);
	passManagerBuilder.populateModulePassManager(passManager);

	LOG_INFO(Frontend, "Running function optimization");
	functionPassManager.doInitialization();
	for (auto &function : *module)
		functionPassManager.run(function);
	functionPassManager.doFinalization();

	passManager.add(createVerifierPass());

	MCContext *context;
	std::error_code error;
	raw_fd_ostream file(filename, error, sys::fs::OpenFlags::F_RW);
	if (file.has_error())
	{
		LOG_CRITICAL(Frontend, "Failed to write bitcode file: error %s", error.message().c_str());
		return;
	}
	if (nativeTarget->addPassesToEmitMC(passManager, context, file, false))
	{
		LOG_CRITICAL(Frontend, "Target does not support MC emission!");
		return;
	}
	LOG_INFO(Frontend, "Generating module");
	passManager.run(*module);
	file.flush();
}

void Codegen::RegisterBasicBlock(llvm::BasicBlock *bb, u32 pc)
{
	blocks.emplace_back(bb, pc);
}

void Codegen::TranslateBlocks()
{
	size_t largestSize = 0, largestPc = 0;
	size_t translated = 0;
	for (u32 i = TranslateStart; i < TranslateEnd;)
	{
		CodeBlock block(this);
		bool generated;
		auto firstPc = i;
		i = block.Run(i, TranslateEnd, &generated);
		if (generated)
		{
			translated += block.instructionCount;
			if (block.instructionCount > largestSize)
			{
				largestSize = block.instructionCount;
				largestPc = firstPc;
			}
		}
	}

	cout << "Translated " << dec << translated << " of " << (TranslateSize / 4) << " = " << (100.0 * translated / (TranslateSize / 4)) << "%" << endl;
	cout << "Largest block with " << largestSize << " instruction at " << hex << largestPc << endl;
}

void Codegen::CreateRegisters()
{
	/*registers = irBuilder->CreateAlloca(irBuilder->getInt32Ty(), irBuilder->getInt32(16));
	for (auto i = 0; i < 16; ++i)
	{
		auto src = irBuilder->CreateConstGEP1_32(registersArgument, i);
		auto load = irBuilder->CreateLoad(src);
		auto dst = irBuilder->CreateConstGEP1_32(registers, i);
		irBuilder->CreateStore(load, dst);
	}*/
}
void Codegen::StoreRegisters()
{
	/*for (auto i = 0; i < 16; ++i)
	{
		auto src = irBuilder->CreateConstGEP1_32(registers, i);
		auto load = irBuilder->CreateLoad(src);
		auto dst = irBuilder->CreateConstGEP1_32(registersArgument, i);
		irBuilder->CreateStore(load, dst);
	}*/
}

bool Codegen::CanRead(Register reg)
{
	return reg <= Register::SP || reg == Register::LR;
}
bool Codegen::CanWrite(Register reg)
{
	return reg <= Register::SP || reg == Register::LR;
}

Value *Codegen::Read(Register reg)
{
	return irBuilder->CreateLoad(irBuilder->CreateConstGEP1_32(registersArgument, (unsigned)reg));
}
Value *Codegen::Write(Register reg, Value *val)
{
	return irBuilder->CreateStore(val, irBuilder->CreateConstGEP1_32(registersArgument, (unsigned)reg));
}