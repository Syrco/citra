/*
 * A register in a broad sense: R0-R15, and flags
 */
enum class Register
{
    R0, R1, R2, R3, R4, R5, R6, R7,
    R8, R9, R10, R11, R12, SP, LR, PC,
    N, Z, C, V
};

enum class Condition
{
    EQ, NE, CS, CC, MI, PL, VS, VC, HI, LS, GE, LT, GT, LE, AL, Invalid
};