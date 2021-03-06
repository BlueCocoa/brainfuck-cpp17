//
//  main.cpp
//  brainfuck
//
//  Created by Ryza on 2019/8/12.
//  Copyright © 2019 Ryza. All rights reserved.
//

#include <stdio.h>
#include <array>
#include <iostream>
#include <map>
#include <optional>
#include <stack>
#include <variant>
#include <vector>

/// brainfuck virtual machine tape length
#define BRAINFUCK_VM_TAPE_LEN 30000

// https://schneide.blog/2018/01/11/c17-the-two-line-visitor-explained/
template<class... Ts> struct brainfuck_vm : Ts... { using Ts::operator()...; };
template<class... Ts> brainfuck_vm(Ts...) -> brainfuck_vm<Ts...>;

#pragma mark - brainfuck ops

struct increment_value_op {}; // +
struct decrement_value_op {}; // -

struct increment_ptr_op {};   // >
struct decrement_ptr_op {};   // <

struct print_op {};           // ,
struct read_op {};            // .

struct loop_start_op {};      // [
struct loop_end_op {};        // ]

/// brainfuck_op allowed ops in C++17 std::variant
using brainfuck_op = std::variant<
    increment_value_op,
    decrement_value_op,
    increment_ptr_op,
    decrement_ptr_op,
    print_op,
    read_op,
    loop_start_op,
    loop_end_op,
    std::monostate
>;

/// a map from char to brainfuck_op
const std::map<char, brainfuck_op> bf_op_map {
    {'+', increment_value_op{}},
    {'-', decrement_value_op{}},
    {'>', increment_ptr_op{}},
    {'<', decrement_ptr_op{}},
    {'.', print_op{}},
    {',', read_op{}},
    {'[', loop_start_op{}},
    {']', loop_end_op{}}
};

#pragma mark - brainfuck vm

/// brainfuck virtual machine status
struct brainfuck_vm_status {
    /// virtual infinity length tape
    std::map<int, char> tape;
    /// current cell of the tape
    int tape_ptr = 0;

    /// used for keeping track of all valid brainfuck_op
    std::vector<char> instruction;
    /// current brainfuck_op index
    int instruction_ptr_current = -1;
    /// keeping track of loops
    std::stack<int> instruction_loop_ptr;

    /// flag of skipping loop, e.g
    /// +-[[[------------++++++++++-.>>[>]>>>--<<<<<<--]]]++++
    ///   ^skipping from, but we need all                ^end of skipping
    ///      instructions inside.
    int jump_loop = 0;
};

#pragma mark - helper function

/**
 next brainfuck op

 @param status   the brainfuck vm status
 @param char_op  character form op
 @param via_loop due to the way I wrote, a flag is needed to avoid re-adding ops
 @return brainfuck_op
 */
brainfuck_op next_op(brainfuck_vm_status & status, char char_op, bool via_loop = false) {
    // find the brainfuck_op from bf_op_map
    if (auto op = bf_op_map.find(char_op); op != bf_op_map.end()) {
        // do not append the char_op if we're retriving the next op inside a loop_op
        if (!via_loop) {
            // save char_op to instruction
            status.instruction.emplace_back(char_op);
            // increse the ptr of current instruction
            status.instruction_ptr_current++;
        }

        // return next op
        return op->second;
    } else {
        // invaild char for brainfuck
        // monostate is returned
        return std::monostate();
    }
}

#pragma mark - brainfuck vm interpreter

/**
 run brainfuck vm

 @param status   run brainfuck vm from the given state
 @param char_op  character form op
 @param via_loop due to the way I wrote, a flag is needed to avoid re-adding ops
 */
void run_vm(brainfuck_vm_status & status, char char_op, bool via_loop = false) {
    // get the op from char_op
    brainfuck_op op = next_op(status, char_op, via_loop);

    // parttern matching
    std::visit(brainfuck_vm {
        [&](increment_value_op) {
            // skip actual action if we're skipping loop
            if (status.jump_loop == 0) {
                status.tape[status.tape_ptr]++;
            }
        },
        [&](decrement_value_op) {
            // skip actual action if we're skipping loop
            if (status.jump_loop == 0) {
                status.tape[status.tape_ptr]--;
            }
        },
        [&](increment_ptr_op) {
            // skip actual action if we're skipping loop
            if (status.jump_loop == 0) {
                status.tape_ptr++;
            }
        },
        [&](decrement_ptr_op) {
            // skip actual action if we're skipping loop
            if (status.jump_loop == 0) {
                status.tape_ptr--;
            }
        },
        [&](print_op) {
            // skip actual action if we're skipping loop
            if (status.jump_loop == 0) {
                printf("%c", status.tape[status.tape_ptr]);
                // printf("%c - %d\n", status.tape[status.tape_ptr], status.tape[status.tape_ptr]);
            }
        },
        [&](read_op) {
            // skip actual action if we're skipping loop
            if (status.jump_loop == 0) {
                status.tape[status.tape_ptr] = getchar();
            }
        },
        [&](loop_start_op) {
            // if and only if 1) `current_cell_value != 0`
            //                2) and we're not do the skipping
            // we can record the starting index of the if instruction
            // besides, if we're in condition 1)
            // the if statement should be also skipped
            if (status.tape[status.tape_ptr] != 0 && status.jump_loop == 0) {
                // push the starting instruction index of loop
                status.instruction_loop_ptr.emplace(status.instruction_ptr_current);
            } else {
                status.jump_loop++;
            }
        },
        [&](loop_end_op) {
            // decrease the jump_loop value if we encounter the `]`
            // and we were previously doing the skip
            if (status.jump_loop != 0) {
                status.jump_loop--;
            } else {
                // if we were not in skipping
                // then we need to check the loop condition, `current_cell_value != 0`
                if (status.tape[status.tape_ptr] != 0) {
                    // the instruction range of current loop
                    // loop the instruction until condition satisfies no more
                    while (status.tape[status.tape_ptr] != 0) {
                        // printf("while cond: %d\n", status.tape[status.tape_ptr]);
                        // save current instruction pointer
                        int current = status.instruction_ptr_current;
                        // start the loop right after the index of `[`
                        status.instruction_ptr_current = status.instruction_loop_ptr.top() + 1;
                        // run one op at a time
                        // until the next op is the corresponding `]`
                        while (status.instruction_ptr_current < current) {
                            run_vm(status, status.instruction[status.instruction_ptr_current], true);
                            status.instruction_ptr_current++;
                        }
                        // restore the current instruction pointer
                        status.instruction_ptr_current = current;
                    }

                    // pop current loop starting index
                    status.instruction_loop_ptr.pop();
                }
            }
        },
        [&](std::monostate) {
        }
    }, op);
}

int main(int argc, const char * argv[]) {
    // the brainfuck vm
    brainfuck_vm_status status;
    // temp variable for receving the character form op
    auto char_op = char{};
    // read char op forever until EOF
    while (std::cin >> char_op) {
        // interpret
        run_vm(status, char_op);
    }
}
