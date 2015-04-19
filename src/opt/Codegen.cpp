#include "Codegen.h"
#include "CodeBlock.h"
#include <llvm/Support/TargetSelect.h>
#include <llvm/MC/MCSubtargetInfo.h>
#include <llvm/MC/MCDisassembler.h>
#include <llvm/MC/MCRegisterInfo.h>
#include <llvm/MC/MCAsmInfo.h>
#include <llvm/MC/MCContext.h>
#include <llvm/PassManager.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_os_ostream.h>
#include <core/mem_map.h>
#include "common/logging/text_formatter.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include <iostream>

using namespace llvm;

Codegen::Codegen()
{
	// Arm
	LLVMInitializeARMTargetInfo();
	LLVMInitializeARMTarget();
	LLVMInitializeARMTargetMC();
	LLVMInitializeARMDisassembler();

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

	// Native
	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();
	InitializeNativeTargetAsmParser();
	nativeContext.reset(new LLVMContext());

	module = llvm::make_unique<Module>("Module", *nativeContext);
	irBuilder = llvm::make_unique<IRBuilder<>>(*nativeContext);
	Type *arguments = {
		Type::getInt32PtrTy(*nativeContext)
	};
	codeBlockFunctionSignature = FunctionType::get(Type::getVoidTy(*nativeContext), arguments, false);
}

Codegen::~Codegen()
{

}

void Codegen::Run(const char *filename)
{
	std::vector<CodeBlock *> blocks;
	CodeBlock *block = nullptr;
	//auto size = 0x100;
	auto size = 0x25000;
	for (u32 i = Memory::EXEFS_CODE_VADDR; i < Memory::EXEFS_CODE_VADDR + size;)
	{
		auto block = new CodeBlock(this);
		bool generated;
		i = block->Run(i, Memory::EXEFS_CODE_VADDR_END, &generated);
		if (generated) blocks.emplace_back(block);
		else delete block;
	}

	std::unique_ptr<Function> entry(Function::Create(codeBlockFunctionSignature, GlobalValue::LinkageTypes::ExternalLinkage, "entry", module.get()));
	auto registers = &entry->getArgumentList().front();
	Value *arguments[] = { registers };
	ArrayRef<Value *> argumentsRef(arguments);
	
	auto switchBasicBlock = BasicBlock::Create(*nativeContext, "switch", entry.get());
	auto exitBasicBlock = BasicBlock::Create(*nativeContext, "exit", entry.get());
	irBuilder->SetInsertPoint(switchBasicBlock);
	auto pc = irBuilder->CreateLoad(irBuilder->CreateGEP(registers, irBuilder->getInt32(15)));
	auto switchInst = irBuilder->CreateSwitch(pc, exitBasicBlock, blocks.size());

	for (auto &block : blocks)
	{
		auto caseBasicBlock = BasicBlock::Create(*nativeContext, "", entry.get());
		irBuilder->SetInsertPoint(caseBasicBlock);
		irBuilder->CreateCall(block->function.get(), argumentsRef, "");
		irBuilder->CreateRetVoid();

		switchInst->addCase(irBuilder->getInt32(block->pc), caseBasicBlock);
	}

	irBuilder->SetInsertPoint(exitBasicBlock);
	irBuilder->CreateRetVoid();

	raw_os_ostream os(std::cout);

	if (verifyModule(*module, &os))
	{
		os.flush();
		std::cin.get();
	}

	//module->dump();

	std::error_code error;
	raw_fd_ostream file(filename, error, sys::fs::OpenFlags::F_RW);

	if (file.has_error())
	{
		LOG_CRITICAL(Frontend, "Failed to write bitcode file: error %s", error.message().c_str());
	}

	WriteBitcodeToFile(module.get(), file);
}