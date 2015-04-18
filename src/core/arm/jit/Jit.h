#include <memory>
#include <common/common_types.h>

class Jit
{
public:
	struct Private;

	Jit();

	bool InterpreterTranslate(struct ARMul_State* cpu, u32 addr);

private:
	std::unique_ptr<Private> priv;
};