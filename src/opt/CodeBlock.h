#include "common/common_types.h"
#include "Instruction.h"
#include <memory>
#include <list>
#include <array>

namespace llvm
{
	class BasicBlock;
	class Value;
	class PHINode;
}

class CodeBlock
{
public:
	CodeBlock(class Codegen *codegen, u32 pc);
	~CodeBlock();

	bool AddInstruction();
	static void Link(CodeBlock *prev, CodeBlock *next);
	void Terminate();
	void JumpFailed();
	llvm::BasicBlock *NewInstructionBasicBlock(const char *str="");

	u32 pc;
	std::unique_ptr<llvm::BasicBlock> loadBlock;
	std::unique_ptr<llvm::BasicBlock> basicBlock;
	llvm::BasicBlock *lastBlock;
	std::list<CodeBlock *> prevs;
	std::list<CodeBlock *> nexts;

	llvm::Value *Read(Register reg);
	llvm::Value *Write(Register reg, llvm::Value *value);

	std::array<llvm::Value *, (size_t)Register::Count> registers;
	std::array<llvm::PHINode *, (size_t)Register::Count> registersPhi;
	u32 jumpAddress = 0;
private:
	void TerminateAt(u32 pc);
	void Spill(int reg);
	Codegen *codegen;
};