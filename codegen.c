#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "symbol.h"
#include "absyn.h"
#include "temp.h"
#include "errormsg.h"
#include "tree.h"
#include "assem.h"
#include "frame.h"
#include "codegen.h"
#include "table.h"
#include "string.h"

static AS_instrList iList = NULL, last = NULL;
//static F_frame frame;

static void emit(AS_instr inst) {
  if (last != NULL)
    last = last->tail = AS_InstrList(inst, NULL);
  else last = iList = AS_InstrList(inst, NULL);
}

static Temp_tempList L(Temp_temp h, Temp_tempList t) {
  return Temp_TempList(h, t);
}

static Temp_tempList munchCallerSave() {
  Temp_tempList callerSaves = F_callerSavedReg();
  Temp_tempList sides = NULL, lastSide = NULL;
  for (; callerSaves; callerSaves = callerSaves->tail) {
    Temp_temp side = Temp_newtemp();
    emit(AS_Move("movq `s0, `d0\n", L(side, NULL), L(callerSaves->head, NULL)));
    if (sides == NULL) {
      lastSide = sides = L(side, NULL);
    } else {
      lastSide->tail = L(side, NULL);
      lastSide = lastSide->tail;
    }
  }
  return sides;
}

static Temp_temp munchExp(T_exp e);

static void munchStm(T_stm s);

static Temp_tempList munchArgs(T_expList args, int *stack_size) {
  int arg_num = 0;
  Temp_tempList para_list = Temp_TempList(NULL, NULL), dummy = para_list;
  while (args) {
    Temp_temp cur = munchExp(args->head);
    if (arg_num >= F_formalRegNum) {
      emit(AS_Oper("\tpushq `s0", L(F_SP(), NULL), L(cur, L(F_SP(), NULL)), NULL));
      *stack_size += F_wordSize;
    } else {
      Temp_temp para = F_ithParam(arg_num + 1);
      para_list->tail = L(para, NULL);
      para_list = para_list->tail;
      emit(AS_Move("\tmovq `s0, `d0", Temp_TempList(para, NULL), Temp_TempList(cur, NULL)));
      arg_num++;
    }
    args = args->tail;
  }
  return dummy->tail;
}

static Temp_temp munchExp(T_exp e) {
  char *buf = checked_malloc(128 * sizeof(char));
  switch (e->kind) {
    case T_MEM: {
      Temp_temp r = Temp_newtemp();
      T_exp mem = e->u.MEM;
      if (mem->kind == T_BINOP) {
        if (mem->u.BINOP.op == T_plus && mem->u.BINOP.right->kind == T_CONST) {
          /* MEM(BINOP(PLUS,e1,CONST(i))) */
          T_exp e1 = mem->u.BINOP.left;
          int i = mem->u.BINOP.right->u.CONST;
          sprintf(buf, "movq %d(`s0), `d0\n", i);
          emit(AS_Oper(buf, L(r, NULL), L(munchExp(e1), NULL), NULL));
          return r;
        } else if (mem->u.BINOP.op == T_plus && mem->u.BINOP.left->kind == T_CONST) {
          /* MEM(BINOP(PLUS,CONST(i),e1)) */
          T_exp e1 = mem->u.BINOP.right;
          int i = mem->u.BINOP.left->u.CONST;
          sprintf(buf, "movq %d(`s0), `d0\n", i);
          emit(AS_Oper(buf, L(r, NULL), L(munchExp(e1), NULL), NULL));
          return r;
        } else {
          /* MEM(e1) */
          emit(AS_Oper("movq (`s0), `d0\n", L(r, NULL), L(munchExp(mem), NULL), NULL));
          return r;
        }
      } else if (mem->kind == T_CONST) {
        /* MEM(CONST(i)) */
        sprintf(buf, "movq %d, `d0\n", mem->u.CONST);
        emit(AS_Oper(buf, L(r, NULL), NULL, NULL));
        return r;
      } else {
        /* MEM(e1) */
        emit(AS_Oper("movq (`s0), `d0\n", L(r, NULL), L(munchExp(mem), NULL), NULL));
        return r;
      }
    }
    case T_BINOP: {
      Temp_temp r = Temp_newtemp();
      Temp_temp left = munchExp(e->u.BINOP.left), right = munchExp(e->u.BINOP.right);
      switch (e->u.BINOP.op) {
        case T_plus: {
          emit(AS_Move("\tmovq `s0, `d0", L(r, NULL), L(left, NULL)));
          emit(AS_Oper("\taddq `s0, `d0", L(r, NULL), L(right, L(r, NULL)), NULL));
          return r;
        }
        case T_minus: {
          emit(AS_Move("\tmovq `s0, `d0", L(r, NULL), L(left, NULL)));
          emit(AS_Oper("\tsubq `s0, `d0", L(r, NULL), L(right, L(r, NULL)), NULL));
          return r;
        }
        case T_mul: {
          emit(AS_Move("\tmovq `s0, `d0", F_pair(), L(left, NULL)));
          emit(AS_Oper("\timul `s0, `d0", F_pair(), L(right, F_pair()), NULL));
          emit(AS_Move("\tmovq `s0, `d0", L(r, NULL), F_pair()));
          return r;
        }
        case T_div: {
          emit(AS_Move("\tmovq `s0, `d0", F_pair(), L(left, NULL)));
          emit(AS_Oper("\tcltd", F_pair(), F_pair(), NULL));
          emit(AS_Oper("\tidiv `s0", F_pair(), L(right, F_pair()), NULL));
          emit(AS_Move("\tmovq `s0, `d0", L(r, NULL), F_pair()));
          return r;
        }
      }
    }
    case T_CONST: {
      Temp_temp r = Temp_newtemp();
      sprintf(buf, "\tmovq $%d, `d0", e->u.CONST);
      emit(AS_Oper(String(buf), L(r, NULL), NULL, NULL));
      return r;
    }
    case T_TEMP: {
      return e->u.TEMP;
    }
    case T_ESEQ: {
      munchStm(e->u.ESEQ.stm);
      return munchExp(e->u.ESEQ.exp);
    }
    case T_NAME: {
      Temp_temp r = Temp_newtemp();
      sprintf(buf, "\tleaq .%s(%rip), `d0", Temp_labelstring(e->u.NAME));
      emit(AS_Oper(String(buf), L(r, NULL), NULL, NULL));
      return r;
    }
    case T_CALL: {
      assert(e->u.CALL.fun->kind == T_NAME);
      Temp_temp r = F_RV();
      int stack_size = 0;

      // save caller saved register
      // munchCallerSave();

      // pass args, which may increase the stack
      Temp_tempList calluse = munchArgs(e->u.CALL.args, &stack_size);

      // call func
      Temp_tempList calldefs = L(r, F_callerSavedReg());
      sprintf(buf, "\tcall %s", Temp_labelstring(e->u.CALL.fun->u.NAME));
      emit(AS_Oper(String(buf), calldefs, calluse, NULL));

      // move the stack pointer
      if (stack_size > 0) {
        char buf[100];
        sprintf(buf, "\taddq $%d, `d0", stack_size);
        emit(AS_Oper(String(buf), L(F_SP(), NULL), L(F_SP(), NULL), NULL));
      }
      return r;
    }
  }
}

void munchStm(T_stm s) {
  char buf[100];
  switch (s->kind) {
    case T_MOVE: {
      T_exp dst = s->u.MOVE.dst, src = s->u.MOVE.src;
      if (dst->kind == T_MEM) {
        if (dst->u.MEM->kind == T_BINOP
            && dst->u.MEM->u.BINOP.op == T_plus
            && dst->u.MEM->u.BINOP.right->kind == T_CONST) {
          if (src->kind == T_CONST) {
            /* MOVE(MEM(BINOP(PLUS,e1,CONST(i))),CONST(j)) */
            T_exp e1 = dst->u.MEM->u.BINOP.left;
            int i = dst->u.MEM->u.BINOP.right->u.CONST;
            int j = src->u.CONST;
            sprintf(buf, "\tmovq $%d, %d(`s0)", j, i);
            emit(AS_Oper(String(buf), NULL, L(munchExp(e1), NULL), NULL));
          } else {
            /* MOVE(MEM(BINOP(PLUS,e1,CONST(i))),e2) */
            T_exp e1 = dst->u.MEM->u.BINOP.left, e2 = src;
            int i = dst->u.MEM->u.BINOP.right->u.CONST;
            sprintf(buf, "\tmovq `s1, %d(`s0)", i);
            emit(AS_Oper(String(buf), NULL, L(munchExp(e1), L(munchExp(e2), NULL)), NULL));
          }
        } else if (dst->u.MEM->kind == T_BINOP
                   && dst->u.MEM->u.BINOP.op == T_plus
                   && dst->u.MEM->u.BINOP.left->kind == T_CONST) {
          if (src->kind == T_CONST) {
            /* MOVE(MEM(BINOP(PLUS,CONST(i),e1)),CONST(j)) */
            T_exp e1 = dst->u.MEM->u.BINOP.right, e2 = src;
            int i = dst->u.MEM->u.BINOP.left->u.CONST;
            int j = src->u.CONST;
            sprintf(buf, "\tmovq $%d, %d(`s0)", j, i);
            emit(AS_Oper(String(buf), NULL, L(munchExp(e1), NULL), NULL));
          } else {
            /* MOVE(MEM(BINOP(PLUS,CONST(i),e1)),e2) */
            T_exp e1 = dst->u.MEM->u.BINOP.right, e2 = src;
            int i = dst->u.MEM->u.BINOP.left->u.CONST;
            sprintf(buf, "\tmovq `s1, %d(`s0)", i);
            emit(AS_Oper(String(buf), NULL, L(munchExp(e1), L(munchExp(e2), NULL)), NULL));
          }
        } else if (src->kind == T_MEM) {
          /* MOVE(MEM(e1), MEM(e2)) */
          T_exp e1 = dst->u.MEM, e2 = src->u.MEM;
          Temp_temp r = Temp_newtemp();
          emit(AS_Oper(String("\tmovq (`s0), `d0"), L(r, NULL), L(munchExp(e2), NULL), NULL));
          emit(AS_Oper(String("\tmovq `s0, (`s1)"), NULL, L(r, L(munchExp(e1), NULL)), NULL));
        } else if (dst->u.MEM->kind == T_CONST) {
          /* MOVE(MEM(CONST(i)), e2) */
          T_exp e2 = src;
          int i = dst->u.MEM->u.CONST;
          sprintf(buf, "\tmovq `s0, %d", i);
          emit(AS_Oper(String(buf), NULL, L(munchExp(e2), NULL), NULL));
        } else {
          /* MOVE(MEM(e1), e2) */
          T_exp e1 = dst->u.MEM, e2 = src;
          sprintf(buf, "\tmovq `s1, (`s0)");
          emit(AS_Oper(String(buf), NULL, L(munchExp(e1), L(munchExp(e2), NULL)), NULL));
        }
      } else if (dst->kind == T_TEMP) {
        emit(AS_Move("\tmovq `s0, `d0", L(munchExp(dst), NULL), L(munchExp(src), NULL)));
      }
      break;
    }
    case T_EXP:{
      munchExp(s->u.EXP);
      break;
    }
    case T_JUMP: {
      if (s->u.JUMP.exp->kind == T_NAME) {
        /* JUMP(NAME(label)) */
        emit(AS_Oper("\tjmp .`j0", NULL, NULL, AS_Targets(s->u.JUMP.jumps)));
      } else {
        /* JUMP(e) e.g. switch(i) see in 7.1*/
        Temp_temp e = munchExp(s->u.JUMP.exp);
        emit(AS_Oper("\tjmp *`s0", NULL, L(e, NULL), AS_Targets(s->u.JUMP.jumps)));
      }
      break;
    }
    case T_CJUMP: {
      /* CJUMP(op, left, right, t, f )*/
      Temp_temp left = munchExp(s->u.CJUMP.left);
      Temp_temp right = munchExp(s->u.CJUMP.right);
      sprintf(buf, "\tcmpq `s1, `s0");
      emit(AS_Oper(String(buf), NULL, L(left, L(right, NULL)), NULL));
      char *op = "";
      switch (s->u.CJUMP.op) {
        case T_eq: {
          op = "je";
          break;
        }
        case T_ne: {
          op = "jne";
          break;
        }
        case T_lt: {
          op = "jl";
          break;
        }
        case T_gt: {
          op = "jg";
          break;
        }
        case T_le: {
          op = "jle";
          break;
        }
        case T_ge: {
          op = "jge";
          break;
        }
      }
      sprintf(buf, "\t%s .`j0", op);
      emit(AS_Oper(String(buf), NULL, NULL, AS_Targets(Temp_LabelList(s->u.CJUMP.true, NULL))));
      break;
    }
    case T_LABEL: {
      /* LABEL(label) */
      Temp_label label = s->u.LABEL;
      sprintf(buf, ".%s", Temp_labelstring(label));
      emit(AS_Label(String(buf),label));
      break;
    }
    default:
      assert(0);
  }
}

AS_instrList F_codegen(F_frame f, T_stmList stmList) {
  AS_instrList list;
  // 使用Maximal Munch算法实现IR树到汇编指令的在转换
  for (T_stmList sl = stmList; sl; sl = sl->tail)
    munchStm(sl->head);
  // 返回汇编指令链表（AS_instrList）
  list = iList;
  iList = last = NULL;
  return list;
}
