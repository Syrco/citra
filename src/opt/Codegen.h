#include <memory>
#include "common/common_types.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetRegistry.h>

class Codegen
{
public:
	Codegen();
	~Codegen();

	void Run(const char *filename);

	// Native
	std::unique_ptr<llvm::LLVMContext> nativeContext;
	std::unique_ptr<llvm::Module> module;
	std::unique_ptr<llvm::IRBuilder<>> irBuilder;

	// Arm
	std::unique_ptr<llvm::LLVMContext> armContext;
	const llvm::Target *armTarget;
	std::unique_ptr<llvm::MCSubtargetInfo> armSubtarget;
	std::unique_ptr<const llvm::MCDisassembler> armDisassembler;
	std::unique_ptr<const llvm::MCRegisterInfo> armRegisterInfo;
	std::unique_ptr<const llvm::MCAsmInfo> armAsmInfo;
	std::unique_ptr<llvm::MCContext> armMCContext;
	llvm::FunctionType *codeBlockFunctionSignature;
};