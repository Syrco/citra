#include <llvm/IR/IRBuilder.h>

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
    void GenerateGlobals();
    void GenerateCanRunFunction();
    void GenerateRunFunction();
    void GenerateBlocks();
private:
    std::unique_ptr<llvm::IRBuilder<>> ir_builder;
    llvm::Module *module;

    /*
     * u32 *Registers;
     *  The registers of the cpu
     */
    llvm::GlobalVariable *registers_global;
    /*
     * u32 *Flags;
     *  The flags of the cpu
     *  Orderered N, Z, C, V
     */
    llvm::GlobalVariable *flags_global;
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
};