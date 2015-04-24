#include <memory>
#include <common/common_types.h>
#include <core/arm/dyncom/arm_dyncom.h>

class Jit
{
public:
	struct Private;

	Jit(ARMul_State* state);

	void BeforeFindBB(struct ARMul_State* cpu);
	bool CanRun(u32 pc);
	void Run();

private:
	std::unique_ptr<Private> priv;
};