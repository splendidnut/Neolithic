/***************************************************************************
 * Neolithic Compiler - Simple C Cross-compiler for the 6502
 *
 * Copyright (c) 2020-2022 by Philip Blackman
 * -------------------------------------------------------------------------
 *
 * Licensed under the GNU General Public License v2.0
 *
 * See the "LICENSE.TXT" file for more information regarding usage and
 * redistribution of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * -------------------------------------------------------------------------
 */


//
//  Instruction generator for Optimized Code Blocks (intrinsics)
//
// Created by User on 11/8/2022.
//

#include "instrs_opt.h"
#include "data/syntax_tree.h"
#include "data/symbols.h"
#include "data/labels.h"
#include "instrs.h"


/**
 * Copy a set of initial data into a struct variable using a simple tight code loop
 *
 * ;--- LDA/STA code size = 4 bytes of code per data byte  = 4x
 * ;--- Loop code size    = 10 bytes of code + data bytes  = 10 + x
 * ;-- If there are more than 3 bytes of data, then using a loop is the optimal solution.
 *
 * @param listNode
 * @param destVar
 * @param useIndirect - indicate whether to use indirect store (using $80 as temp ptr)
 */
void ICG_CopyDataIntoStruct(ListNode listNode, SymbolRecord *destVar, bool useIndirect) {
    Label *initData = newGenericLabel(LBL_DATA);
    Label *lblInitCopy = newGenericLabel(LBL_CODE);
    Label *startOfLoop = newGenericLabel(LBL_LOOP_START);

    ICG_Jump(lblInitCopy, "jump over data");

    //----------------------------------------------------------------
    //--- build initializer list data here (and place before code)

    IL_AddCommentToCode("Initializer data");
    IL_Label(initData);

    SymbolTable *structSymTbl = GET_STRUCT_SYMBOL_TABLE(destVar->userTypeDef);
    SymbolRecord *curStructVar = structSymTbl->firstSymbol;
    List *initList = listNode.value.list;
    int index = 1;
    do {
        int varSize = getBaseVarSize(curStructVar);

        // write initializer list item using appropriate size
        ListNode initNode = initList->nodes[index];
        ICG_AsmData(initNode.value.num, (varSize > 1));

        // go to next element
        curStructVar = curStructVar->next;
        index++;
    } while ((index < initList->count) && (curStructVar != NULL));

    //----------------------------------
    //--  now do code
    /*
initCopy:
	LDY		#SIZE_OF_STRUCT
initCopyLoop:
	LDA 	.srcData,Y
	STA		[dst_struct],Y
	DEY
	BPL		initCopyLoop
     */

    IL_AddCommentToCode("Copy data to destination");
    IL_Label(lblInitCopy);
    ICG_LoadRegConst('Y', getStructVarSize(destVar));
    IL_Label(startOfLoop);
    IL_AddInstrP(LDA, ADDR_ABY, initData->name, PARAM_NORMAL);
    if (useIndirect) {
        IL_AddInstrN(STA, ADDR_IY, 0x80);   //--- store use tempvar as PTR
    } else {
        IL_AddInstrP(STA, ADDR_ABY, destVar->name, PARAM_NORMAL);
    }
    IL_AddInstrB(DEY);
    ICG_Branch(BPL, startOfLoop);
}