#include <memory>
#include <llvm/IR/IRBuilder.h>

namespace llvm
{
    class TargetMachine;
    class Module;
}

class ModuleGen;

/*
 * Holds alls the basic llvm structures
 */
class CodeGen
{
public:
	CodeGen(const char *output_object_filename, const char *output_debug_filename);
	~CodeGen();

    void Run();
    void IntializeLLVM();
    void GenerateModule();
    void GenerateDebugFiles();
    bool Verify();
    void OptimizeAndGenerate();
private:
	const char *output_object_filename;
	const char *output_debug_filename;

    std::unique_ptr<ModuleGen> moduleGenerator;

    std::unique_ptr<llvm::Triple> triple;
    std::unique_ptr<llvm::TargetMachine> target_machine;
    std::unique_ptr<llvm::Module> module;
};