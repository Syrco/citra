#include <memory>
#include <string>
#include <common/common_types.h>
#include <llvm/IR/IRBuilder.h>
#include "ModuleGen.h"

namespace llvm
{
    class Value;
    class BasicBlock;
}

class Instruction;

enum class Register;

/*
 * An instruction blocks
 * Holds the entry and exit points for an instruction
 * Responsible to generate the code
 */
class InstructionBlock
{
public:
    InstructionBlock(ModuleGen *module, Instruction *instruction);
    ~InstructionBlock();

    /*
    * Generates the basic block of the instruction
    */
    void GenerateEntryBlock();

    /*
     * Generates the code for the instruction
     */
    void GenerateCode();

    /*
     * Generates code to read the register
     */
    llvm::Value *Read(Register reg);
    /*
     * Generates code to write the value
     * Returns the write instruction = written value
     */
    llvm::Value *Write(Register reg, llvm::Value *value);

    /*
     * Creates a basic block for use by instructions
     */
    llvm::BasicBlock *CreateBasicBlock(const char *name);
    /*
	 * Links two instructions, adding to prev and next lists
	 */
	static void Link(InstructionBlock *prev, InstructionBlock *next);

    u32 Address();
    ModuleGen *Module() { return module; }
    llvm::IRBuilder<> *IrBuilder() { return module->IrBuilder(); }

    llvm::BasicBlock *GetEntryBasicBlock() { return entry_basic_block; }

	bool HasColor() { return has_color; }
	void SetColor(size_t color) { this->color = color; has_color = true; }
	size_t GetColor() { return color; }

	std::list<InstructionBlock *> GetNexts() { return nexts; }
	std::list<InstructionBlock *> GetPrevs() { return prevs; }
private:
    // Textual representation of the address
    // Used to generate names
    std::string address_string;

    ModuleGen *module;
    std::unique_ptr<Instruction> instruction;

    // The block at the entry to instruction
    llvm::BasicBlock *entry_basic_block;

	bool has_color = false;
	size_t color;

	std::list<InstructionBlock *> nexts;
	std::list<InstructionBlock *> prevs;
};