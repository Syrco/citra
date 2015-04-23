#include <memory>
#include <common/common_types.h>

class Jit
{
public:
	struct Private;

	Jit();

	bool CanRun(u32 pc);
	void Run(struct ARMul_State* cpu);

private:
	std::unique_ptr<Private> priv;
};