#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "pigletvm.h"

#define STACK_MAX 256
#define MEMORY_SIZE 65536

#define NEXT_OP()                               \
    (*vm.ip++)
#define NEXT_ARG()                                      \
    ((void)(vm.ip += 2), (vm.ip[-2] << 8) + vm.ip[-1])
#define PEEK_ARG()                              \
    ((vm.ip[0] << 8) + vm.ip[1])
#define POP()                                   \
    (*(--vm.stack_top))
#define PUSH(val)                               \
    (*vm.stack_top = (val), vm.stack_top++)
#define PEEK()                                  \
    (*(vm.stack_top - 1))
#define TOS_PTR()                               \
    (vm.stack_top - 1)


static struct {
    uint8_t *ip;

        /* Fixed-size stack */
    uint64_t stack[STACK_MAX];
    uint64_t *stack_top;

    /* Operational memory */
    uint64_t memory[MEMORY_SIZE];

    /* A single register containing the result */
    uint64_t result;
} vm;

static void vm_reset(void)
{
    puts("Reset vm state");
    vm = (typeof(vm)) { NULL };
    vm.stack_top = vm.stack;
}

interpret_result vm_interpret(uint8_t *bytecode)
{
    vm_reset();

    puts("Start interpreting");
    vm.ip = bytecode;
    for (;;) {
        uint8_t instruction = NEXT_OP();
        switch (instruction) {
        case OP_PUSHI: {
            /* get the argument, push it onto stack */
            uint16_t arg = NEXT_ARG();
            PUSH(arg);
            break;
        }
        case OP_LOADI: {
            /* get the argument, use it to get a value onto stack */
            uint16_t addr = NEXT_ARG();
            uint64_t val = vm.memory[addr];
            PUSH(val);
            break;
        }
        case OP_LOADADDI: {
            /* get the argument, add the value from the address to the top of the stack */
            uint16_t addr = NEXT_ARG();
            uint64_t val = vm.memory[addr];
            *TOS_PTR() += val;
            break;
        }
        case OP_STOREI: {
            /* get the argument, use it to get a value of the stack into a memory cell */
            uint16_t addr = NEXT_ARG();
            uint64_t val = POP();
            vm.memory[addr] = val;
            break;
        }
        case OP_LOAD: {
            /* pop an address, use it to get a value onto stack */
            uint16_t addr = POP();
            uint64_t val = vm.memory[addr];
            PUSH(val);
            break;
        }
        case OP_STORE: {
            /* pop a value, pop an adress, put a value into an address */
            uint64_t val = POP();
            uint16_t addr = POP();
            vm.memory[addr] = val;
            break;
        }
        case OP_DUP:{
            /* duplicate the top of the stack */
            PUSH(PEEK());
            break;
        }
        case OP_DISCARD: {
            /* discard the top of the stack */
            (void)POP();
            break;
        }
        case OP_ADD: {
            /* Pop 2 values, add 'em, push the result back to the stack */
            uint64_t arg_right = POP();
            *TOS_PTR() += arg_right;
            break;
        }
        case OP_ADDI: {
            /* Add immediate value to the top of the stack */
            uint16_t arg_right = NEXT_ARG();
            *TOS_PTR() += arg_right;
            break;
        }
        case OP_SUB: {
            /* Pop 2 values, subtract 'em, push the result back to the stack */
            uint64_t arg_right = POP();
            *TOS_PTR() -= arg_right;
            break;
        }
        case OP_DIV: {
            /* Pop 2 values, divide 'em, push the result back to the stack */
            uint64_t arg_right = POP();
            /* Don't forget to handle the div by zero error */
            if (arg_right == 0)
                return ERROR_DIVISION_BY_ZERO;
            *TOS_PTR() /= arg_right;
            break;
        }
        case OP_MUL: {
            /* Pop 2 values, multiply 'em, push the result back to the stack */
            uint64_t arg_right = POP();
            *TOS_PTR() *= arg_right;
            break;
        }
        case OP_JUMP:{
            /* Use arg as a jump target  */
            uint16_t target = PEEK_ARG();
            vm.ip = bytecode + target;
            break;
        }
        case OP_JUMP_IF_TRUE:{
            /* Use arg as a jump target  */
            uint16_t target = NEXT_ARG();
            if (POP())
                vm.ip = bytecode + target;
            break;
        }
        case OP_JUMP_IF_FALSE:{
            /* Use arg as a jump target  */
            uint16_t target = NEXT_ARG();
            if (!POP())
                vm.ip = bytecode + target;
            break;
        }
        case OP_EQUAL:{
            uint64_t arg_right = POP();
            *TOS_PTR() = PEEK() == arg_right;
            break;
        }
        case OP_LESS:{
            uint64_t arg_right = POP();
            *TOS_PTR() = PEEK() < arg_right;
            break;
        }
        case OP_LESS_OR_EQUAL:{
            uint64_t arg_right = POP();
            *TOS_PTR() = PEEK() <= arg_right;
            break;
        }
        case OP_GREATER:{
            uint64_t arg_right = POP();
            *TOS_PTR() = PEEK() > arg_right;
            break;
        }
        case OP_GREATER_OR_EQUAL:{
            uint64_t arg_right = POP();
            *TOS_PTR() = PEEK() >= arg_right;
            break;
        }
        case OP_GREATER_OR_EQUALI:{
            uint64_t arg_right = NEXT_ARG();
            *TOS_PTR() = PEEK() >= arg_right;
            break;
        }
        case OP_POP_RES: {
            /* Pop the top of the stack, set it as a result value */
            uint64_t res = POP();
            vm.result = res;
            break;
        }
        case OP_DONE: {
            return SUCCESS;
        }
        case OP_PRINT:{
            uint64_t arg = POP();
            printf("%" PRIu64 "\n", arg);
            break;
        }
        case OP_ABORT: {
            return ERROR_END_OF_STREAM;
        }
        default:
            return ERROR_UNKNOWN_OPCODE;
        }
    }

    return SUCCESS;
}

interpret_result vm_interpret_threaded(uint8_t *bytecode)
{
    vm_reset();
    vm.ip = bytecode;

    const void *labels[] = {
        [OP_PUSHI] = &&op_pushi,
        [OP_LOADI] = &&op_loadi,
        [OP_LOADADDI] = &&op_loadaddi,
        [OP_STORE] = &&op_store,
        [OP_STOREI] = &&op_storei,
        [OP_LOAD] = &&op_load,
        [OP_DUP] = &&op_dup,
        [OP_DISCARD] = &&op_discard,
        [OP_ADD] = &&op_add,
        [OP_ADDI] = &&op_addi,
        [OP_SUB] = &&op_sub,
        [OP_DIV] = &&op_div,
        [OP_MUL] = &&op_mul,
        [OP_JUMP] = &&op_jump,
        [OP_JUMP_IF_TRUE] = &&op_jump_if_true,
        [OP_JUMP_IF_FALSE] = &&op_jump_if_false,
        [OP_EQUAL] = &&op_equal,
        [OP_LESS] = &&op_less,
        [OP_LESS_OR_EQUAL] = &&op_less_or_equal,
        [OP_GREATER] = &&op_greater,
        [OP_GREATER_OR_EQUAL] = &&op_greater_or_equal,
        [OP_GREATER_OR_EQUALI] = &&op_greater_or_equali,
        [OP_POP_RES] = &&op_pop_res,
        [OP_DONE] = &&op_done,
        [OP_PRINT] = &&op_print,
        [OP_ABORT] = &&op_abort,
    };

    goto *labels[NEXT_OP()];

op_pushi: {
        /* get the argument, push it onto stack */
        uint16_t arg = NEXT_ARG();
        PUSH(arg);
        goto *labels[NEXT_OP()];
    }
op_loadi: {
    /* get the argument, use it to get a value onto stack */
        uint16_t addr = NEXT_ARG();
        uint64_t val = vm.memory[addr];
        PUSH(val);
        goto *labels[NEXT_OP()];
    }
op_loadaddi: {
        /* get the argument, add the value from the address to the top of the stack */
        uint16_t addr = NEXT_ARG();
        uint64_t val = vm.memory[addr];
        *TOS_PTR() += val;
        goto *labels[NEXT_OP()];
    }
op_storei: {
        /* get the argument, use it to get a value of the stack into a memory cell */
        uint16_t addr = NEXT_ARG();
        uint64_t val = POP();
        vm.memory[addr] = val;
        goto *labels[NEXT_OP()];
    }
op_load: {
        /* pop an address, use it to get a value onto stack */
        uint16_t addr = POP();
        uint64_t val = vm.memory[addr];
        PUSH(val);
        goto *labels[NEXT_OP()];
    }
op_store: {
        /* pop a value, pop an adress, put a value into an address */
        uint64_t val = POP();
        uint16_t addr = POP();
        vm.memory[addr] = val;
        goto *labels[NEXT_OP()];
    }
op_dup:{
        /* duplicate the top of the stack */
        PUSH(PEEK());
        goto *labels[NEXT_OP()];
    }
op_discard: {
        /* discard the top of the stack */
        (void)POP();
        goto *labels[NEXT_OP()];
    }
op_add: {
        /* Pop 2 values, add 'em, push the result back to the stack */
        uint64_t arg_right = POP();
        *TOS_PTR() += arg_right;
        goto *labels[NEXT_OP()];
    }
op_addi: {
        /* Add immediate value to the top of the stack */
        uint16_t arg_right = NEXT_ARG();
        *TOS_PTR() += arg_right;
        goto *labels[NEXT_OP()];
    }
op_sub: {
        /* Pop 2 values, subtract 'em, push the result back to the stack */
        uint64_t arg_right = POP();
        *TOS_PTR() -= arg_right;
        goto *labels[NEXT_OP()];
    }
op_div: {
        /* Pop 2 values, divide 'em, push the result back to the stack */
        uint64_t arg_right = POP();
        /* Don't forget to handle the div by zero error */
        if (arg_right == 0)
            return ERROR_DIVISION_BY_ZERO;
        *TOS_PTR() /= arg_right;
        goto *labels[NEXT_OP()];
    }
op_mul: {
        /* Pop 2 values, multiply 'em, push the result back to the stack */
        uint64_t arg_right = POP();
        *TOS_PTR() *= arg_right;
        goto *labels[NEXT_OP()];
    }
op_jump:{
        /* Use arg as a jump target  */
        uint16_t target = PEEK_ARG();
        vm.ip = bytecode + target;
        goto *labels[NEXT_OP()];
    }
op_jump_if_true:{
        /* Use arg as a jump target  */
        uint16_t target = NEXT_ARG();
        if (POP())
            vm.ip = bytecode + target;
        goto *labels[NEXT_OP()];
    }
op_jump_if_false:{
        /* Use arg as a jump target  */
        uint16_t target = NEXT_ARG();
        if (!POP())
            vm.ip = bytecode + target;
        goto *labels[NEXT_OP()];
    }
op_equal:{
        uint64_t arg_right = POP();
        *TOS_PTR() = PEEK() == arg_right;
        goto *labels[NEXT_OP()];
    }
op_less:{
        uint64_t arg_right = POP();
        *TOS_PTR() = PEEK() < arg_right;
        goto *labels[NEXT_OP()];
    }
op_less_or_equal:{
        uint64_t arg_right = POP();
        *TOS_PTR() = PEEK() <= arg_right;
        goto *labels[NEXT_OP()];
    }
op_greater:{
        uint64_t arg_right = POP();
        *TOS_PTR() = PEEK() > arg_right;
        goto *labels[NEXT_OP()];
    }
op_greater_or_equal:{
        uint64_t arg_right = POP();
        *TOS_PTR() = PEEK() >= arg_right;
        goto *labels[NEXT_OP()];
    }
op_greater_or_equali:{
        uint64_t arg_right = NEXT_ARG();
        *TOS_PTR() = PEEK() >= arg_right;
        goto *labels[NEXT_OP()];
    }
op_pop_res: {
        /* Pop the top of the stack, set it as a result value */
        uint64_t res = POP();
        vm.result = res;
        goto *labels[NEXT_OP()];
    }
op_done: {
        goto end;
    }
op_print:{
        uint64_t arg = POP();
        printf("%" PRIu64 "\n", arg);
        goto *labels[NEXT_OP()];
    }
op_abort: {
        return ERROR_END_OF_STREAM;
    }
end:
    return SUCCESS;
}


uint64_t vm_get_result(void)
{
    return vm.result;
}
