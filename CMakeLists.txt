cmake_minimum_required(VERSION 3.15)
project(neolithic)

set(CMAKE_C_STANDARD 99)

include_directories(.)

add_executable(neolithic
        data/syntax_tree.c
        data/syntax_tree.h
        main.c
        parser/parser.c
        parser/parser.h
        data/symbols.c
        data/symbols.h
        parser/tokenize.c
        parser/tokenize.h
        codegen/gen_symbols.c
        codegen/gen_symbols.h
        codegen/gen_code.c
        codegen/gen_code.h
        parser/preprocess.c
        parser/preprocess.h
        parser/tokens.h
        parser/tokens.c
        data/labels.c
        data/labels.h
        machine/mem.c
        machine/mem.h
        cpu_arch/instrs.c
        cpu_arch/instrs.h
        codegen/eval_expr.c
        codegen/eval_expr.h
        data/func_map.c
        data/func_map.h
        cpu_arch/asm_code.c
        cpu_arch/asm_code.h
        parser/parse_asm.c
        parser/parse_asm.h
        data/instr_list.c
        data/instr_list.h
        output/write_output.c
        output/write_output.h
        common/common.c
        common/common.h
        output/output_block.c
        output/output_block.h
        optimizer/optimizer.c
        optimizer/optimizer.h
        codegen/gen_calltree.c
        codegen/gen_calltree.h
        codegen/call_stack.c
        codegen/call_stack.h
        optimizer/gen_opttree.c
        optimizer/gen_opttree.h
        common/tree_walker.c
        common/tree_walker.h
        data/type_list.c
        data/type_list.h
        codegen/gen_common.c
        codegen/gen_common.h
        cpu_arch/instrs_math.c
        cpu_arch/instrs_math.h machine/machine.h codegen/gen_alloc.c codegen/gen_alloc.h)

#--  Uncomment next line to generate .S assembler files for each .C file
#-- set_target_properties(neolithic PROPERTIES COMPILE_FLAGS "-save-temps -fverbose-asm")
