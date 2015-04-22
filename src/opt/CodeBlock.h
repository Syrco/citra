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


	u32 pc;
	llvm::BasicBlock *lastBlock = nullptr;

private:
	bool AddInstruction(u32 pc);

	Codegen *codegen;
};