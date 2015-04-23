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
#include "Instruction.h"
#include "Decode.h"

class Codegen
{
public:
	Codegen();
	~Codegen();

	void Run(const char *filename);
	void RegisterBasicBlock(llvm::BasicBlock *bb, u32 pc);

	void GenerateEntryFunction();
	void GeneratePresentFunction();
	void GenerateSwitchArray();
	void GenerateSwitchArrayIf(llvm::Value *offset, llvm::Function *function,
		llvm::BasicBlock *enterBasicBlock, llvm::BasicBlock *exitTrueBasicBlock, llvm::BasicBlock *exitFalseBasicBlock,
		llvm::Value **pointer);

	void WriteFile(const char *filename);
	void WriteLL(const char *filename);

	void CreateRegisters();
	void StoreRegisters();

	bool CanRead(Register reg);
	bool CanWrite(Register reg);

	llvm::Type *RegType(Register reg);
	llvm::Value *RegGEP(Register reg);
	llvm::Value *Read(Register reg);
	llvm::Value *Write(Register reg, llvm::Value *val);


	// Native
	std::unique_ptr<llvm::LLVMContext> nativeContext;
	std::unique_ptr<llvm::Module> module;
	std::unique_ptr<llvm::IRBuilder<>> irBuilder;
	std::unique_ptr<llvm::TargetMachine> nativeTarget;
	std::unique_ptr<llvm::Function> function;

	// Arm
	std::unique_ptr<llvm::LLVMContext> armContext;
	const llvm::Target *armTarget;
	std::unique_ptr<llvm::MCSubtargetInfo> armSubtarget;
	std::unique_ptr<const llvm::MCDisassembler> armDisassembler;
	std::unique_ptr<const llvm::MCRegisterInfo> armRegisterInfo;
	std::unique_ptr<const llvm::MCAsmInfo> armAsmInfo;
	std::unique_ptr<const llvm::MCInstrInfo> armInstrInfo;
	std::unique_ptr<const llvm::MCInstPrinter> armInstPrinter;
	std::unique_ptr<llvm::MCContext> armMCContext;
	llvm::FunctionType *codeBlockFunctionSignature;
	llvm::Triple nativeTriple;

	Decoder *decoder;

	llvm::Argument *registersArgument, *flagsArgument;
	llvm::BasicBlock *outOfCodeblock, *inToCodeBlock;

	size_t switchArraySize;
	llvm::Value *switchArray;
	llvm::PointerType *switchArrayMemberType;
	llvm::Constant *switchArrayNull;
private:
	std::map<u32, CodeBlock *> blocks;
	//std::vector<std::pair<llvm::BasicBlock *, u32>> blocks;
	llvm::Value *registers;
	void TranslateBlocks();
};