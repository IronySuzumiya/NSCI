#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>

#define POOL_SIZE (1024 * 256)

enum Nadeko_C_Virtual_Machine_Commands
{
    // no operator command
    LL, LI, LC, SL, SI, SC, PUSH,
    ADD, SUB, MUL, DIV, MOD, AND, OR, NOT, XOR, SHL, SHR, INC, DEC,
    LEV, // CONTROL

    // one or two(under x64 architecture) operator command
    IMM, CALL, ENT, LEA, ADJ, JMP, JZ, JNZ,

    // syscall command
    PRTF, EXIT
};

enum Token_Classes
{
    Int = 128, Long, LongLong, Char, String, Glo, Loc, Fun, Sys, Id,
    If, Else, While, Break, Continue, Return, Enum, Struct,
    Assign, Cond, Lor, Land, Or, Xor, And, Ne, Eq, Le, Lt, Ge, Gt,
    Shr, Shl, Sub, Add, Mod, Mul, Div, Sizeof, Not, Lnot, Dec, Inc,
    Ptr, Sel, Brak
};

struct _Token
{
    int token;
    int hash;
    char *name;
    int type;
    int class;
#ifndef WIN32
    long value;
#else
    long long value;
#endif
}
Token, *Tptr;

int is_white_space(char *token)
{
    return *token == ' ' || *token == '\t'
        || *token == '\r' || *token == '\n';
}

void global_declaration(char **src, long **text, char **data)
{
    while (is_white_space(*src)) { ++src; }
}

void program(char *src, long *text, char *data)
{
    int token;
    while (token = *src++)
    {
        global_declaration(&src, &text, &data);
    }
}

int eval(long *pc, long *stack, char *data)
{
    long op;
    long eax;
    long *ebp, *esp;

    ebp = esp = stack + POOL_SIZE / sizeof(long) - 1;

    while (1)
    {
        op = *pc++;

        // Load or store
        if (op == IMM) { eax = *pc++; }
#ifndef WIN32
        else if (op == LL) {
            eax = *(long *)((long long)esp[0] << 32 | esp[1]);
            esp = esp + 2;
        }
        else if (op == LI) {
            eax = *(int *)((long long)esp[0] << 32 | esp[1]);
            esp = esp + 2;
        }
        else if (op == LC) {
            eax = *(char *)((long long)esp[0] << 32 | esp[1]);
            esp = esp + 2;
        }
        else if (op == SL) {
            *(long *)((long long)esp[0] << 32 | esp[1]) = eax;
            esp = esp + 2;
        }
        else if (op == SI) {
            *(int *)((long long)esp[0] << 32 | esp[1]) = eax;
            esp = esp + 2;
        }
        else if (op == SC) {
            *(char *)((long long)esp[0] << 32 | esp[1]) = eax;
            esp = esp + 2;
        }
#else
        else if (op == LC) { eax = *(char *)eax; }
        else if (op == LI) { eax = *(int *)eax; }
        else if (op == LL) { eax = *(long *)eax; }
        else if (op == SC) { *(char *)*esp++ = eax; }
        else if (op == SI) { *(int *)*esp++ = eax; }
        else if (op == SL) { *(long *)*esp++ = eax; }
#endif
        else if (op == PUSH) { *--esp = eax; }

        // Calculate
        else if (op == ADD) { eax = *esp++ + eax; }
        else if (op == SUB) { eax = *esp++ - eax; }
        else if (op == MUL) { eax = *esp++ * eax; }
        else if (op == DIV) { eax = *esp++ / eax; }
        else if (op == MOD) { eax = *esp++ % eax; }
        else if (op == AND) { eax = *esp++ & eax; }
        else if (op == OR) { eax = *esp++ | eax; }
        else if (op == NOT) { eax = ~eax; }
        else if (op == XOR) { eax = *esp++ ^ eax; }
        else if (op == SHL) { eax = *esp++ << eax; }
        else if (op == SHR) { eax = *esp++ >> eax; }
        else if (op == INC) { ++eax; }
        else if (op == DEC) { --eax; }
        
        // Control
#ifndef WIN32
        else if (op == CALL) {
            *--esp = (long)(pc + 2); // lower 32bits
            *--esp = (long)(((long long)(pc + 2) & 0xffffffff00000000) >> 32); // higher 32bits
            pc = (long *)(pc[0] | (long long)pc[1] << 32);
        }
        else if (op == ENT) {
            *--esp = (long)(ebp);
            ebp = esp;
            *--esp = (long)(((long long)ebp & 0xffffffff00000000) >> 32);
            esp = esp - *pc++;
        }
        else if (op == LEV) {
            esp = ebp;
            ebp = (long *)((long long)esp[-1] << 32 | esp[0]);
            esp = esp + 2;
            pc = (long *)((long long)esp[-1] << 32 | esp[0]);
            ++esp;
        }
        else if (op == JMP) {
            pc = (long *)(pc[0] | (long long)pc[1] << 32);
        }
        else if (op == JZ) {
            pc = eax == 0 ? (long *)(pc[0] | (long long)pc[1] << 32) : pc + 2;
        }
        else if (op == JNZ) {
            pc = eax == 0 ? pc + 2 : (long *)(pc[0] | (long long)pc[1] << 32);
        }
#else
        else if (op == CALL) { *--esp = (long)(pc + 1); pc = (long *)*pc; }
        else if (op == ENT) { *--esp = (long)ebp; ebp = esp; esp = esp - *pc++; }
        else if (op == LEV) { esp = ebp; ebp = (long *)*esp++; pc = (long *)*esp++; }
        else if (op == JMP) { pc = (long *)*pc; }
        else if (op == JZ) { pc = eax == 0 ? (long *)*pc : pc + 1; }
        else if (op == JNZ) { pc = eax == 0 ? pc + 1 : (long *)*pc; }
#endif
        else if (op == LEA) { eax = *(ebp - *pc++); }
        else if (op == ADJ) { esp = esp + *pc++; }
        
        // Syscall
#ifndef WIN32
        else if (op == PRTF) { printf((char *)((long long)ebp[2] | (long long)ebp[3] << 32), ebp[4]); }
#else
        else if (op == PRTF) { printf((char *)ebp[2], ebp[3]); }
#endif
        else if (op == EXIT) { printf("exit(%d);\n", (int)*pc); return (int)*pc; }
        else { printf("unknown command %ld", op); return -1; }
    }
    return 0;
}

int main(int argc, char** argv)
{
    int fd, i;                  // file

    char *src, *src_head;       // source code

    long *text, *text_head;     // text segment

    long *stack;                // stack segment

    char *data, *data_head;     // data segment

    if (argc < 2)
    {
        printf("Usage: <ExecFilename> <SourceFilename>\n");
        return -1;
    }

    --argc;
    ++argv;

    if (!(fd = open(*argv, 0x0000)))
    {
        printf("Could not open the file %s\n", *argv);
        return -1;
    }
    if (!(src = src_head = malloc(POOL_SIZE)))
    {
        printf("Could not malloc(%ld) for the source code", POOL_SIZE);
        return -1;
    }
    if (!(text = text_head = malloc(POOL_SIZE)))
    {
        printf("Could not malloc(%ld) for the text segment", POOL_SIZE);
        return -1;
    }
    if (!(stack = malloc(POOL_SIZE)))
    {
        printf("Could not malloc(%ld) for the stack segment", POOL_SIZE);
        return -1;
    }
    if (!(data = data_head = malloc(POOL_SIZE)))
    {
        printf("Could not malloc(%ld) for the data segment", POOL_SIZE);
        return -1;
    }
    if (!(i = read(fd, src, POOL_SIZE - 1)))
    {
        printf("Bad file read, returned %d", i);
        return -1;
    }
    src[i] = 0;

    memset(text, 0, POOL_SIZE);
    memset(stack, 0, POOL_SIZE);
    memset(data, 0, POOL_SIZE);

    program(src, text, data);

    *text++ = IMM;
    *text++ = 3;
    *text++ = PUSH;
    *text++ = IMM;
    *text++ = -1;
    *text++ = PUSH;
    *text++ = IMM;
    *text++ = 2;
    *text++ = SUB;
    *text++ = ADD;
    *text++ = JZ;
#ifndef WIN32
    *text++ = (long)(text + 4);
    *text++ = (long)(((long long)(text + 3) & 0xffffffff00000000) >> 32);
#else
    *text++ = (long)(text + 3);
#endif
    *text++ = EXIT;
    *text++ = 666;
    *text++ = EXIT;
    *text++ = 0;

    return eval(text_head, stack, data);
}
