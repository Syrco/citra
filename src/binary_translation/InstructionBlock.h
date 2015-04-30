#include <memory>
#include <string>
#include <common/common_types.h>

namespace llvm
{
    class Value;
    class BasicBlock;
}

class ModuleGen;
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

    u32 Address();
    ModuleGen *Module() { return module; }

    llvm::BasicBlock *GetEntryBasicBlock() { return entry_basic_block; }
private:
    // Textual representation of the address
    // Used to generate names
    std::string address_string;

    ModuleGen *module;
    std::unique_ptr<Instruction> instruction;

    // The block at the entry to instruction
    llvm::BasicBlock *entry_basic_block;
};