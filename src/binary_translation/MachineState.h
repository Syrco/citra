
enum class Register;
class ModuleGen;

namespace llvm
{
    class Value;
    class GlobalVariable;
}

/*
Contains all the machine state:
    Registers, Flags, Memory
*/
class MachineState
{
public:
    MachineState(ModuleGen *module);

    void GenerateGlobals();
    llvm::Value *ReadRegiser(Register reg);
    llvm::Value *WriteRegiser(Register reg, llvm::Value *value);

private:
    // Returns the address of a register or a flag
    llvm::Value *GetRegisterPtr(Register reg);

    ModuleGen *module;

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
};