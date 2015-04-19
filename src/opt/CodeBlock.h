#include <memory>
#include "common/common_types.h"

#include <llvm/IR/Function.h>
#include <llvm/MC/MCInst.h>

class CodeBlock
{
public:
	CodeBlock(class Codegen *codegen);
	~CodeBlock();

	u32 Run(u32 start, u32 end, bool *generated);

	size_t instructionCount;
	std::unique_ptr<llvm::Function> function;

	u32 pc;

private:
	bool AddInstruction(u32 pc);
	llvm::Value *GetRegisterAddress(llvm::MCOperand &operand);
	llvm::Value *GetRegisterAddress(u32 num);
	llvm::Value *ReadRegister(llvm::MCOperand &operand);
	llvm::Value *WriteRegister(llvm::MCOperand &operand, llvm::Value *value);
	llvm::Value *ReadRegister(u32 num);
	llvm::Value *WriteRegister(u32 num, llvm::Value *value);
	llvm::Value *CreateInt32(u32 num);

#define OPCODE(name) bool Parse##name(llvm::MCInst &instruction, u32 pc)

	OPCODE(MOVr);

#undef OPCODE

	Codegen *codegen;
	llvm::BasicBlock *basicBlock;
	llvm::Value *registers;
};