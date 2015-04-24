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
	llvm::BasicBlock *NewInstructionBasicBlock(const char *str = "");

	llvm::Value *Read(Register reg);
	llvm::Value *Write(Register reg, llvm::Value *value);

	void BeginCond(Condition cond);
	void EndCond();

	u32 pc;
	std::unique_ptr<llvm::BasicBlock> loadBlock;
	std::unique_ptr<llvm::BasicBlock> basicBlock;
	llvm::BasicBlock *lastBlock;
	llvm::BasicBlock *condPassed;
	llvm::BasicBlock *condNotPassed;
	std::list<CodeBlock *> prevs;
	std::list<CodeBlock *> nexts;
	bool inConditionalBlock = false;

	std::array<llvm::Value *, (size_t)Register::Count> registers;
	std::array<llvm::PHINode *, (size_t)Register::Count> registersPhi;
	//std::array<llvm::Value *, (size_t)Register::Count> conditionalStores;
	u32 jumpAddress = 0;
	bool disabled;
private:
	void TerminateAt(u32 pc);
	void Spill(int reg);
	Codegen *codegen;
};