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
	Type *arguments[] = {
		Type::getInt32PtrTy(*nativeContext), Type::getInt1PtrTy(*nativeContext)
	};
	codeBlockFunctionSignature = FunctionType::get(Type::getVoidTy(*nativeContext), arguments, false);
}

Codegen::~Codegen()
{

}

struct BinSearch
{
	size_t min;
	size_t mid;
	size_t max;
	BinSearch(size_t max) : min(0), mid(max / 2), max(max) { }
	BinSearch(size_t min, size_t max) : min(min), mid((min + max) / 2), max(max) { }
	BinSearch l() { return BinSearch(min, mid); }
	BinSearch r() { return BinSearch(mid, max); }
};

static const size_t TranslateSize = 0x24000;
//static const size_t TranslateSize = 0x40;
//static const size_t TranslateSize = 0x13AD0;
//static const size_t TranslateSize = BinSearch(0x25000).r().r().r().r().l().l().r().r().l().l().l().l().mid;
//static const size_t TranslateSize = BinSearch(0x25000).r().r().r().r().l().l().r().r().l().l().l().l().mid + 4;


static const size_t TranslateStart = Memory::EXEFS_CODE_VADDR;
static const size_t TranslateEnd = TranslateStart + TranslateSize;

void Codegen::Run(const char *filename)
{
	GenerateEntryFunction();
	GeneratePresentFunction();

	WriteFile(filename);
}

void Codegen::GenerateEntryFunction()
{
	function.reset(Function::Create(codeBlockFunctionSignature, GlobalValue::LinkageTypes::ExternalLinkage, "Run", module.get()));
	registersArgument = &function->getArgumentList().front();
	flagsArgument = function->getArgumentList().getNext(registersArgument);

	auto switchBasicBlock = BasicBlock::Create(*nativeContext, "switch", function.get());
	auto exitBasicBlock = BasicBlock::Create(*nativeContext, "exit", function.get());
	inToCodeBlock = BasicBlock::Create(*nativeContext, "notNull", function.get());
	outOfCodeblock = BasicBlock::Create(*nativeContext, "outOfCodeblock", function.get());

	irBuilder->SetInsertPoint(inToCodeBlock);
	CreateRegisters();
	TranslateBlocks();
	GenerateSwitchArray();

	Value *addr;

	irBuilder->SetInsertPoint(switchBasicBlock);
	auto pc = irBuilder->CreateLoad(irBuilder->CreateConstGEP1_32(registersArgument, 15));
	GenerateSwitchArrayIf(pc, function.get(),
		switchBasicBlock, inToCodeBlock, exitBasicBlock,
		&addr);

	irBuilder->SetInsertPoint(inToCodeBlock);
	auto indirectBr = irBuilder->CreateIndirectBr(addr, switchArraySize);
	for (auto p : blocks)
	{
		indirectBr->addDestination(p.second->loadBlock.get());
	}

	irBuilder->SetInsertPoint(outOfCodeblock);
	StoreRegisters();
	irBuilder->CreateBr(exitBasicBlock);

	irBuilder->SetInsertPoint(exitBasicBlock);
	irBuilder->CreateRetVoid();
}

void Codegen::GeneratePresentFunction()
{
	Type *arguments = { irBuilder->getInt32Ty() };
	auto presentFunctionSignature = FunctionType::get(irBuilder->getInt1Ty(), arguments, false);
	auto presentFunction = Function::Create(presentFunctionSignature, GlobalValue::LinkageTypes::ExternalLinkage, "Present", module.get());
	auto pc = &presentFunction->getArgumentList().front();

	Value *addr;
	auto switchBasicBlock = BasicBlock::Create(*nativeContext, "switch", presentFunction);
	auto trueBasicBlock = BasicBlock::Create(*nativeContext, "true", presentFunction);
	auto falseBasicBlock = BasicBlock::Create(*nativeContext, "false", presentFunction);

	irBuilder->SetInsertPoint(switchBasicBlock);
	GenerateSwitchArrayIf(pc, presentFunction,
		switchBasicBlock, trueBasicBlock, falseBasicBlock,
		&addr);

	irBuilder->SetInsertPoint(trueBasicBlock);
	irBuilder->CreateRet(irBuilder->getInt1(true));

	irBuilder->SetInsertPoint(falseBasicBlock);
	irBuilder->CreateRet(irBuilder->getInt1(false));
}

void Codegen::GenerateSwitchArray()
{
	switchArraySize = TranslateSize / 4;
	switchArrayMemberType = irBuilder->getInt8PtrTy();
	auto switchArrType = ArrayType::get(switchArrayMemberType, switchArraySize);
	switchArrayNull = ConstantPointerNull::get(switchArrayMemberType);
	auto switchLocalArr = new Constant*[switchArraySize];
	std::fill(switchLocalArr, switchLocalArr + switchArraySize, switchArrayNull);
	for (auto p : blocks)
	{
		switchLocalArr[(p.first - TranslateStart) / 4] = BlockAddress::get(function.get(), p.second->loadBlock.get());
	}
	auto switchArrayConst = ConstantArray::get(switchArrType, ArrayRef<Constant*>(switchLocalArr, switchLocalArr + switchArraySize));
	delete switchLocalArr;
	switchArray = new GlobalVariable(*module, switchArrayConst->getType(), true, GlobalValue::InternalLinkage, switchArrayConst, "SwitchArray");
}

void Codegen::GenerateSwitchArrayIf(llvm::Value *offset, llvm::Function *function,
	llvm::BasicBlock *enterBasicBlock, llvm::BasicBlock *exitTrueBasicBlock, llvm::BasicBlock *exitFalseBasicBlock,
	llvm::Value **pointer)
{
	auto bb0 = enterBasicBlock;                                     // if(pc <)
	auto bb1 = BasicBlock::Create(*nativeContext, "bb1", function); //  if(value != 0)
	auto bb2 = exitTrueBasicBlock;									//   then
	auto bb3 = exitFalseBasicBlock;									// else

	irBuilder->SetInsertPoint(bb0);
	auto pc = offset;
	auto pcSubStart = irBuilder->CreateSub(pc, irBuilder->getInt32(TranslateStart));
	auto pcSubStartDiv4 = irBuilder->CreateAShr(pcSubStart, 2);
	auto higher = irBuilder->CreateICmpUGE(pcSubStartDiv4, irBuilder->getInt32(TranslateSize / 4));
	auto iff = irBuilder->CreateCondBr(higher, bb3, bb1);

	irBuilder->SetInsertPoint(bb1);
	Value *values[] = { irBuilder->getInt32(0), pcSubStartDiv4 };
	auto addr = irBuilder->CreateLoad(irBuilder->CreateGEP(switchArray, values));
	auto cmp = irBuilder->CreateICmpNE(addr, switchArrayNull);
	auto iff2 = irBuilder->CreateCondBr(cmp, bb2, bb3);

	*pointer = addr;
}

void Codegen::WriteLL(const char *filename)
{
	LOG_INFO(Frontend, "Writing .ll");
	std::error_code error;
	raw_fd_ostream file(string(filename) + ".ll", error, sys::fs::OpenFlags::F_RW);

	if (file.has_error())
	{
		LOG_CRITICAL(Frontend, "Failed to write bitcode file: error %s", error.message().c_str());
	}

	module->print(file, nullptr);
	LOG_INFO(Frontend, "Done");
}

void Codegen::WriteFile(const char *filename)
{
	PassManager passManager;
	FunctionPassManager functionPassManager(module.get());
	PassManagerBuilder passManagerBuilder;

	//auto bb = blocks[0x1003ac]->basicBlock.get();
	//bb->dump();

	WriteLL(filename);
	raw_os_ostream os(std::cout);
	if (verifyModule(*module, &os))
	{
		os.flush();
		//WriteLL(filename);
		std::cin.get();
	}

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

void Codegen::TranslateBlocks()
{
	size_t largestSize = 0, largestPc = 0;
	size_t translated = 0;
	CodeBlock *lastBlock = nullptr;
	size_t linked = 0, terminated = 0, jumpFailed = 0;
	for (u32 i = TranslateStart; i < TranslateEnd; i += 4)
	{
		auto codeBlock = new CodeBlock(this, i);
		if (codeBlock->AddInstruction())
		{
			if (lastBlock && !lastBlock->jumpAddress)
			{
				lastBlock->jumpAddress = i;
			}
			blocks.insert(make_pair(i, codeBlock));
			++translated;
		}
		else
		{
			if (lastBlock)
			{
				if (lastBlock->lastBlock->getTerminator()) __debugbreak();
			}
			delete codeBlock;
			codeBlock = nullptr;
		}
		lastBlock = codeBlock;
	}

	for (auto p : blocks)
	{
		auto block = p.second;
		if (!block->jumpAddress) continue;
		auto i = blocks.find(block->jumpAddress);
		if (i != blocks.end())
		{
			CodeBlock::Link(block, i->second);
			++linked;
		}
	}
	for (auto p : blocks)
	{
		auto block = p.second;
		if (!block->jumpAddress) continue;
		auto i = blocks.find(block->jumpAddress);
		if (i == blocks.end())
		{
			block->JumpFailed();
			++jumpFailed;
		}
	}
	for (auto block : blocks)
	{
		if (!block.second->jumpAddress)
		{
			block.second->Terminate();
			++terminated;
		}
	}

	cout << "Translated " << hex << translated << " of " << (TranslateSize / 4) << " = " << dec << (100.0 * translated / (TranslateSize / 4)) << "%" << endl;
	cout << "Total " << blocks.size() << " blocks" << endl;
	cout << linked <<     " linked     blocks = " << (100.0 * linked / blocks.size()) << "%" << endl;
	cout << terminated << " terminated blocks = " << (100.0 * terminated / blocks.size()) << "%" << endl;
	cout << jumpFailed << " jumpFailed blocks = " << (100.0 * jumpFailed / blocks.size()) << "%" << endl;
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

llvm::Type *Codegen::RegType(Register reg)
{
	return reg < Register::N ? irBuilder->getInt32Ty() : irBuilder->getInt1Ty();
}
Value *Codegen::RegGEP(Register reg)
{
	auto src = reg < Register::N ? registersArgument : flagsArgument;
	auto offset = reg < Register::N ? (unsigned)reg : (((unsigned)reg - (unsigned)Register::N) * 4);
	return irBuilder->CreateConstGEP1_32(src, offset);
}
Value *Codegen::Read(Register reg)
{
	return irBuilder->CreateLoad(RegGEP(reg));
}
Value *Codegen::Write(Register reg, Value *val)
{
	return irBuilder->CreateStore(val, RegGEP(reg));
}