#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "table.h"
#include "tree.h"
#include "frame.h"
/*Lab5: Your implementation here.*/

//varibales
const int F_wordSize = 8;
const int F_formalRegNum = 6;
const int F_regNum = 16;

struct F_frame_ {
  T_stm shift; // instructions required to implement the “view shift”
  int size;
  F_accessList formals; // the locations of all the formals
  Temp_label label; // the label at which the function’s machine code is to begin
};

/* stack frame */

F_accessList F_AccessList(F_access head, F_accessList tail) {
  F_accessList a = checked_malloc(sizeof(*a));
  a->head = head;
  a->tail = tail;
  return a;
}

static F_access InFrame(int offset) {
  F_access a = checked_malloc(sizeof(*a));
  a->kind = inFrame;
  a->u.offset = offset;
  return a;
}

static F_access InReg(Temp_temp reg) {
  F_access a = checked_malloc(sizeof(*a));
  a->kind = inReg;
  a->u.reg = reg;
  return a;
}

static F_accessList makeFormalAccessList(F_frame f, U_boolList formals) {
  F_accessList head = NULL;
  F_accessList tail = NULL;
  for (; formals; formals = formals->tail) {
    F_access a = F_allocLocal(f, formals->head);
    if (head == NULL) {
      head = tail = F_AccessList(a, NULL);
    } else {
      tail->tail = F_AccessList(a, NULL);
      tail = tail->tail;
    }
  }
  return head;
}

static T_stm makeShiftStm(F_frame f) {
// instructions must be produced to implement the “view shift.”
// 将escape变量传入frame，将non-escape传入temp register(use T_Move(dstExp, srcExp))
  T_stm shiftStm = NULL;
  T_exp dstExp, srcExp;
  F_accessList formals = f->formals;
  for(int ith = 1; formals; formals = formals->tail, ith++ ){
    F_access access = formals->head;

    // trans ith formal into IR exp
    // escape => T_Mem(fp + offset), non-escape => T_Temp(reg)
    dstExp = F_Exp(access, T_Temp(F_FP()));

    if (ith <= F_formalRegNum) {
      srcExp = T_Temp(F_ithParam(ith));
    } else {
      // spare 1 word for return address, 1 word for saved %rbp
      srcExp = T_Mem(T_Binop(T_plus,T_Temp(F_FP()),T_Const((ith - F_formalRegNum + 1) * F_wordSize)));
    }
    if (shiftStm)
      shiftStm = T_Seq(shiftStm, T_Move(dstExp, srcExp));
    else shiftStm = T_Move(dstExp, srcExp);
  }

  return shiftStm;
}

F_frame F_newFrame(Temp_label name, U_boolList formals) {
  F_frame f = checked_malloc(sizeof(*f));
  f->label = name;
  f->size = 0;
  f->formals = makeFormalAccessList(f, formals);
  f->shift = makeShiftStm(f);
  return f;
}

Temp_label F_name(F_frame f) {
  return f->label;
}

F_accessList F_formals(F_frame f) {
  return f->formals;
}

F_access F_allocLocal(F_frame f, bool escape) {
  if (escape) {
    f->size += F_wordSize;
    return InFrame(-f->size);
  } else {
    // should move temp to register
    return InReg(Temp_newtemp());
  }
}

/* IR */
T_exp F_Exp(F_access acc, T_exp framePtr) {
  // 将一个`F_access`转换为一个Tree表达式（`T_exp`）
  if (acc->kind == inFrame) {
    return T_Mem(T_Binop(T_plus, framePtr, T_Const(acc->u.offset)));
  } else {
    return T_Temp(acc->u.reg);
  }
}

T_exp F_externalCall(string s, T_expList args) {
  return T_Call(T_Name(Temp_namedlabel(s)), args);
}

/* fragment */

F_frag F_StringFrag(Temp_label label, string str) {
  F_frag f = checked_malloc(sizeof(*f));
  f->kind = F_stringFrag;
  f->u.stringg.label = label;
  f->u.stringg.str = str;
  return f;
}

F_frag F_ProcFrag(T_stm body, F_frame frame) {
  F_frag f = checked_malloc(sizeof(*f));
  f->kind = F_procFrag;
  f->u.proc.body = body;
  f->u.proc.frame = frame;
  return f;
}

F_fragList F_FragList(F_frag head, F_fragList tail) {
  F_fragList fl = checked_malloc(sizeof(*fl));
  fl->head = head;
  fl->tail = tail;
  return fl;
}

/* lab 6 */

/* registers */

static Temp_temp rax = NULL;
static Temp_temp rbx = NULL;
static Temp_temp rcx = NULL;
static Temp_temp rdx = NULL;
static Temp_temp rsi = NULL;
static Temp_temp rdi = NULL;
static Temp_temp rbp = NULL;
static Temp_temp rsp = NULL;
static Temp_temp r8 = NULL;
static Temp_temp r9 = NULL;
static Temp_temp r10 = NULL;
static Temp_temp r11 = NULL;
static Temp_temp r12 = NULL;
static Temp_temp r13 = NULL;
static Temp_temp r14 = NULL;
static Temp_temp r15 = NULL;

Temp_temp F_rax() {
  if (!rax) rax = Temp_newtemp();
  return rax;
}

Temp_temp F_rbx() {
  if (!rbx) rbx = Temp_newtemp();
  return rbx;
}

Temp_temp F_rcx() {
  if (!rcx) rcx = Temp_newtemp();
  return rcx;
}

Temp_temp F_rdx() {
  if (!rdx) rdx = Temp_newtemp();
  return rdx;
}

Temp_temp F_rsi() {
  if (!rsi) rsi = Temp_newtemp();
  return rsi;
}

Temp_temp F_rdi() {
  if (!rdi) rdi = Temp_newtemp();
  return rdi;
}

Temp_temp F_rbp() {
  if (!rbp) rbp = Temp_newtemp();
  return rbp;
}

Temp_temp F_rsp() {
  if (!rsp) rsp = Temp_newtemp();
  return rsp;
}

Temp_temp F_r8() {
  if (!r8) r8 = Temp_newtemp();
  return r8;
}

Temp_temp F_r9() {
  if (!r9) r9 = Temp_newtemp();
  return r9;
}

Temp_temp F_r10() {
  if (!r10) r10 = Temp_newtemp();
  return r10;
}

Temp_temp F_r11() {
  if (!r11) r11 = Temp_newtemp();
  return r11;
}

Temp_temp F_r12() {
  if (!r12) r12 = Temp_newtemp();
  return r12;
}

Temp_temp F_r13() {
  if (!r13) r13 = Temp_newtemp();
  return r13;
}

Temp_temp F_r14() {
  if (!r14) r14 = Temp_newtemp();
  return r14;
}

Temp_temp F_r15() {
  if (!r15) r15 = Temp_newtemp();
  return r15;
}

Temp_temp F_SP() { return F_rsp(); }

Temp_temp F_FP() { return F_rbp(); }

Temp_temp F_RV() { return F_rax(); }

Temp_temp F_ithParam(int ith) {
  switch (ith) {
    case 1:
      return F_rdi();
    case 2:
      return F_rsi();
    case 3:
      return F_rdx();
    case 4:
      return F_rcx();
    case 5:
      return F_r8();
    case 6:
      return F_r9();
    default:
      assert(0);
  }
}

Temp_tempList F_pair() {
  return Temp_TempList(F_rax(),Temp_TempList(F_rdx(), NULL));
}

static Temp_tempList allReg = NULL;

Temp_tempList F_registers() {
  if (!allReg)
    allReg = Temp_TempList(F_rax(),
    Temp_TempList(F_rbx(),
    Temp_TempList(F_rcx(),
    Temp_TempList(F_rdx(),
    Temp_TempList(F_rsi(),
    Temp_TempList(F_rdi(),
    Temp_TempList(F_rbp(),
    Temp_TempList(F_rsp(),
    Temp_TempList(F_r8(),
    Temp_TempList(F_r9(),
    Temp_TempList(F_r10(),
    Temp_TempList(F_r11(),
    Temp_TempList(F_r12(),
    Temp_TempList(F_r13(),
    Temp_TempList(F_r14(),
    Temp_TempList(F_r15(),NULL))))))))))))))));
  return allReg;
}

static Temp_tempList calleeSavedReg = NULL;

Temp_tempList F_calleeSavedReg() {
  if (!calleeSavedReg)
    calleeSavedReg =
      Temp_TempList(F_rbx(),
        Temp_TempList(F_r12(),
          Temp_TempList(F_r13(),
            Temp_TempList(F_r14(),
              Temp_TempList(F_r15(),NULL)))));
  return calleeSavedReg;
}

static Temp_tempList callerSavedReg = NULL;

Temp_tempList F_callerSavedReg() {
  if (!callerSavedReg)
    callerSavedReg =
      Temp_TempList(F_rax(),
        Temp_TempList(F_rcx(),
          Temp_TempList(F_rdx(),
            Temp_TempList(F_rdi(),
              Temp_TempList(F_rsi(),
               Temp_TempList(F_r8(),
                 Temp_TempList(F_r9(),
                   Temp_TempList(F_r10(),
                     Temp_TempList(F_r11(),NULL)))))))));
  return callerSavedReg;
}

static Temp_tempList paramReg = NULL;

Temp_map tempMap = NULL;

Temp_map F_tempMap(void) {
  if (!tempMap) {
    tempMap = Temp_empty();
    Temp_enter(tempMap, F_rax(), "\%rax");
    Temp_enter(tempMap, F_rbx(), "\%rbx");
    Temp_enter(tempMap, F_rcx(), "\%rcx");
    Temp_enter(tempMap, F_rdx(), "\%rdx");
    Temp_enter(tempMap, F_rbp(), "\%rbp");
    Temp_enter(tempMap, F_rsp(), "\%rsp");
    Temp_enter(tempMap, F_rdi(), "\%rdi");
    Temp_enter(tempMap, F_rsi(), "\%rsi");
    Temp_enter(tempMap, F_r8(), "\%r8");
    Temp_enter(tempMap, F_r9(), "\%r9");
    Temp_enter(tempMap, F_r10(), "\%r10");
    Temp_enter(tempMap, F_r11(), "\%r11");
    Temp_enter(tempMap, F_r12(), "\%r12");
    Temp_enter(tempMap, F_r13(), "\%r13");
    Temp_enter(tempMap, F_r14(), "\%r14");
    Temp_enter(tempMap, F_r15(), "\%r15");
  }
  return tempMap;
}

T_stm F_procEntryExit1(F_frame frame, T_stm body) {
  // (5)(8)保存和恢复callee-saved register
  Temp_tempList temps = F_calleeSavedReg();
  T_stm save = NULL, restore = NULL;
  for (; temps; temps = temps->tail) {
    T_stm s = T_Move(T_Temp(Temp_newtemp()), T_Temp(temps->head));
    if (save == NULL) save = s;
    else save = T_Seq(save, s);

    T_stm r = T_Move(s->u.MOVE.src, s->u.MOVE.dst);
    if (restore == NULL) restore = r;
    else restore = T_Seq(restore, r);
  }

  // (4)frame->shift: 将escape变量传入frame，将non-escape传入temp register
  // (5)save (6)(7)body: 函数体 + 将返回值传至专用于返回结果的寄存器 (8)restore
  return T_Seq(frame->shift, T_Seq(save, T_Seq(body, restore)));
}

static Temp_tempList returnSink = NULL;

AS_instrList F_procEntryExit2(AS_instrList body) {
  if (!returnSink)
    returnSink = Temp_TempList(F_SP(), Temp_TempList(F_FP(), F_calleeSavedReg()));
  // 在结尾添加活跃的寄存器
  return AS_splice(body, AS_InstrList(AS_Oper("", NULL, returnSink, NULL), NULL));
}

AS_proc F_procEntryExit3(F_frame frame, AS_instrList body) {
  char prolog[1024];
  string procName = S_name(F_name(frame));
  sprintf(prolog, "\t.global %s\n", procName);
  sprintf(prolog, "%s%s:\n", prolog, procName);
  sprintf(prolog, "%s\tpushq %%rbp\n", prolog);
  sprintf(prolog, "%s\tmovq %%rsp, %%rbp\n", prolog);
  sprintf(prolog, "%s\tsubq $%d, %%rsp\n", prolog, frame->size);

  char epilog[256];
  sprintf(epilog, "\tleave\n");
  sprintf(epilog, "%s\tret\n", epilog);

  return AS_Proc(String(prolog), body, String(epilog));
}

static Temp_labelList predefine = NULL;

Temp_labelList F_preDefineFuncs() {
  if (!predefine) {
    predefine = Temp_LabelList(Temp_namedlabel("flush"),
                Temp_LabelList(Temp_namedlabel("exit"),
                Temp_LabelList(Temp_namedlabel("not"),
                Temp_LabelList(Temp_namedlabel("chr"),
                Temp_LabelList(Temp_namedlabel("getchar"),
                Temp_LabelList(Temp_namedlabel("print"),
                Temp_LabelList(Temp_namedlabel("ord"),
                Temp_LabelList(Temp_namedlabel("size"),
                Temp_LabelList(Temp_namedlabel("concat"),
                Temp_LabelList(Temp_namedlabel("substring"),
                Temp_LabelList(Temp_namedlabel("printi"),NULL)))))))))));
  }
  return predefine;
}
