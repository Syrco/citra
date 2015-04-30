#include <llvm/IR/IRBuilder.h>
#include <unordered_map>
#include <common/common_types.h>

enum class Register;

class InstructionBlock;
class MachineState;

namespace llvm
{
    class Module;
}

class ModuleGen
{
public:
    explicit ModuleGen(llvm::Module *module);
    ~ModuleGen();

    void Run();

    llvm::IRBuilder<> *IrBuilder() { return ir_builder.get(); }
    llvm::Module *Module() { return module; }
    MachineState *Machine() { return machine.get(); }

private:
    // Generates the declarations of all the globals of the module
    void GenerateGlobals();
    void GenerateBlockAddressArray();
    void GenerateGetBlockAddressFunction();
    void GenerateCanRunFunction();
    void GenerateRunFunction();
    // Creates InstructionBlock for each instruction
    void DecodeInstructions();
    // Generates the entry basic blocks for each instruction
    void GenerateInstructionsEntry();
    // Generates the code of each instruction
    void GenerateInstructionsCode();
    // Terminates each block
    void GenerateInstructionsTermination();
    // Adds all the basic blocks of an instruction to the run function
    void AddInstructionsToRunFunction();

    std::unique_ptr<MachineState> machine;

    std::unique_ptr<llvm::IRBuilder<>> ir_builder;
    llvm::Module *module;

    size_t block_address_array_base;
    size_t block_address_array_size;
    /*
     * i8 **BlockAddressArray;
     *  The array at [i/4 - block_address_array_base] contains the block address for the instruction at i
     *  or nullptr if it is not decoded
     */
    llvm::ArrayType *block_address_array_type;
    llvm::GlobalVariable *block_address_array;
    /*
     * i8 *GetBlockAddress(u32 pc)
     *  Returns the address of the block for the instruction at pc
     */
    llvm::Function *get_block_address_function;
    /*
     * bool CanRun()
     *  Returns whether there is a binary translation available for a PC
     */
    llvm::Function *can_run_function;
    /*
     * void Run()
     *  Runs binary translated opcodes
     */
    llvm::Function *run_function;
    llvm::BasicBlock *run_function_entry;
    llvm::BasicBlock *run_function_re_entry;

    /*
     * All the instruction blocks
     */
    std::vector<std::unique_ptr<InstructionBlock>> instruction_blocks;
    std::unordered_map<u32, InstructionBlock *> instruction_blocks_by_pc;
};