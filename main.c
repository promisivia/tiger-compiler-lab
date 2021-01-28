/*
 * main.c
 */

#include <stdio.h>
#include "util.h"
#include "symbol.h"
#include "types.h"
#include "absyn.h"
#include "errormsg.h"
#include "temp.h" /* needed by translate.h */
#include "tree.h" /* needed by frame.h */
#include "assem.h"
#include "frame.h" /* needed by translate.h and printfrags prototype */
#include "translate.h"
#include "env.h"
#include "semant.h" /* function prototype for transProg */
#include "canon.h"
#include "prabsyn.h"
#include "printtree.h"
#include "escape.h"
#include "parse.h"
#include "codegen.h"
#include "regalloc.h"
//---------------------
#include "graph.h"
#include "flowgraph.h"
#include "liveness.h"

extern bool anyErrors;

//------debug-----------
void showInstrInfo(FILE *out, void *inst) {
  AS_print(out, (AS_instr) inst, Temp_layerMap(F_tempMap(), Temp_name()));
}

void showTempInfo(FILE *out, void *t) {
  fprintf(out, "%s\n", Temp_look(Temp_layerMap(F_tempMap(), Temp_name()), (Temp_temp) t));
}
//----------------------

/*Mark: Lab6: complete the function doProc
 * 1. initialize the F_tempMap
 * 2. initialize the register lists (for register allocation)
 * 3. do register allocation
 * 4. output (print) the assembly code of each function
 
 * Uncommenting the following printf can help you debugging.*/

/* print the assembly language instructions to filename.s */
static void doProc(FILE *out, FILE *logg, F_frame frame, T_stm body) {
  AS_proc proc;
  T_stmList stmList;
  AS_instrList iList;
  struct C_block blo;

//  fprintf(logg, "doProc for function %s:\n", S_name(F_name(frame)));

  stmList = C_linearize(body);  /* 8 */
  blo = C_basicBlocks(stmList);
  stmList = C_traceSchedule(blo);

//  printStmList(logg, stmList);
//  fprintf(logg,"-------====trace=====-----\n");


  iList = F_codegen(frame, stmList);  /* 9 */
  iList = F_procEntryExit2(iList);

//  AS_printInstrList(logg, iList, Temp_layerMap(F_tempMap(), Temp_name()));
  struct RA_result ra = RA_regAlloc(frame, iList);  /* 11 */

  //Part of TA's implementation. Just for reference.
  AS_rewrite(ra.il, Temp_layerMap(F_tempMap(), ra.coloring));
  proc = F_procEntryExit3(frame, ra.il);

  fprintf(out, "%s", proc->prolog);
  AS_printInstrList(out, proc->body, ra.coloring);  // Assem tree to machine language
  fprintf(out, "%s", proc->epilog);
}

void doStr(FILE *out, Temp_label label, string str) {
  fprintf(out, "\t.section .rodata\n");
  fprintf(out, ".%s:\n", S_name(label));

  int length = *(int *) str;
  fprintf(out, "\t.int %d\n", length);
  //it may contains zeros in the middle of string. To keep this work, we need to print all the charactors instead of using fprintf(str)
  fprintf(out, "\t.string \"");
  int i = 0;
  str = str + 4;
  char c;
  for (; i < length; i++) {
    c = str[i];
    if (c == '\n') {
      fprintf(out, "\\n");
    } else if (c == '\t') {
      fprintf(out, "\\t");
    } else {
      fprintf(out, "%c", c);
    }
  }
  fprintf(out, "\"\n");

  //fprintf(out, ".string \"%s\"\n", str);
}

int main(int argc, string *argv) {
  A_exp absyn_root;
//  S_table base_env, base_tenv;
  F_fragList frags;
  char outfile[100];
  char logfile[100];
  FILE *out = stdout;
  FILE *logg = stdout;

  if (argc == 2) {
    absyn_root = parse(argv[1]);
    if (!absyn_root)
      return 1;

#if 0
    pr_exp(out, absyn_root, 0); /* print absyn data structure */
   fprintf(out, "\n");
#endif

    //Lab 6: escape analysis
    //If you have implemented escape analysis, uncomment this
    Esc_findEscape(absyn_root); /* set varDec's escape field */

    frags = SEM_transProg(absyn_root);
    if (anyErrors) return 1; /* don't continue */

    /* convert the filename */
    sprintf(outfile, "%s.s", argv[1]);
    sprintf(logfile, "%s.log", argv[1]);
    out = fopen(outfile, "w");
    logg = fopen(logfile, "w");

    /* Chapter 8, 9, 10, 11 & 12 */
    for (; frags; frags = frags->tail)
      if (frags->head->kind == F_procFrag) {
        doProc(out, logg, frags->head->u.proc.frame, frags->head->u.proc.body);
      } else if (frags->head->kind == F_stringFrag)
        doStr(out, frags->head->u.stringg.label, frags->head->u.stringg.str);

    fclose(out);
    fclose(logg);
  }
  EM_error(0, "usage: tiger file.tig");
  return 1;
}
