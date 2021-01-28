#include <stdio.h>
#include "util.h"
#include "table.h"
#include "symbol.h"
#include "absyn.h"
#include "temp.h"
#include "tree.h"
#include "printtree.h"
#include "frame.h"
#include "translate.h"
#include "string.h"

//LAB5: you can modify anything you want.

/* const */
static Tr_level outermost = NULL;
static F_fragList fragList = NULL;


/* frame */
struct Tr_level_ {
  F_frame frame;
  Tr_level parent;
};

Tr_access Tr_Access(Tr_level l, F_access a) {
  Tr_access ta = checked_malloc(sizeof(*ta));
  ta->level = l;
  ta->access = a;
  return ta;
}

Tr_accessList Tr_AccessList(Tr_access head, Tr_accessList tail) {
  Tr_accessList a = checked_malloc(sizeof(*a));
  a->head = head;
  a->tail = tail;
  return a;
}

Tr_level Tr_outermost(void) {
  if (!outermost)
    outermost = Tr_newLevel(NULL, Temp_namedlabel("tigermain"), NULL);
  return outermost;
}

Tr_level Tr_newLevel(Tr_level parent, Temp_label name, U_boolList formals) {
  // 创建新的嵌套层，调用 `F_newFrame()`
  Tr_level l = checked_malloc(sizeof(*l));
  l->parent = parent;
  l->frame = F_newFrame(name, U_BoolList(TRUE, formals)); // add static link
  return l;
}


Tr_accessList Tr_formals(Tr_level level) {
  // 调用`F_formals()`, 并转化为 `Tr_access`
  Tr_accessList dummy = Tr_AccessList(NULL, NULL), tail = dummy;

  F_accessList f = F_formals(level->frame)->tail; // ignore the static link
  for (; f; f = f->tail) {
    Tr_access a = Tr_Access(level, f->head);
    tail->tail = Tr_AccessList(a, NULL);
    tail = tail->tail;
  }
  return dummy->tail;
}

Tr_access Tr_allocLocal(Tr_level level, bool escape) {
  // 调用`F_formals()`, 并转化为 `Tr_access`
  return Tr_Access(level, F_allocLocal(level->frame, escape));
}


/* IR */
/* struct */
typedef struct patchList_ *patchList;
struct patchList_ {
  Temp_label *head;
  patchList tail;
};

struct Cx {
  patchList trues, falses;
  T_stm stm;
};

struct Tr_exp_ {
  enum {
    Tr_ex, Tr_nx, Tr_cx
  } kind;
  union {
    T_exp ex;
    T_stm nx;
    struct Cx cx;
  } u;
};

/* function implements */
Tr_expList Tr_ExpList(Tr_exp head, Tr_expList tail) {
  Tr_expList l = checked_malloc(sizeof(*l));
  l->head = head;
  l->tail = tail;
  return l;
}

static patchList PatchList(Temp_label *head, patchList tail) {
  patchList list = (patchList) checked_malloc(sizeof(struct patchList_));
  list->head = head;
  list->tail = tail;
  return list;
}

void doPatch(patchList tList, Temp_label label) {
  for (; tList; tList = tList->tail)
    *(tList->head) = label;
}

patchList joinPatch(patchList first, patchList second) {
  if (!first) return second;
  for (; first->tail; first = first->tail);
  first->tail = second;
  return first;
}

/* translation helper */
static Tr_exp Tr_Ex(T_exp ex) {
  Tr_exp e = checked_malloc(sizeof(*e));
  e->kind = Tr_ex;
  e->u.ex = ex;
  return e;
}

static Tr_exp Tr_Nx(T_stm nx) {
  Tr_exp e = checked_malloc(sizeof(*e));
  e->kind = Tr_nx;
  e->u.nx = nx;
  return e;
}

static Tr_exp Tr_Cx(patchList trues, patchList falses, T_stm stm) {
  Tr_exp e = checked_malloc(sizeof(*e));
  e->kind = Tr_cx;
  e->u.cx.trues = trues;
  e->u.cx.falses = falses;
  e->u.cx.stm = stm;
  return e;
}

static T_exp unEx(Tr_exp e) {
  switch (e->kind) {
    case Tr_ex:
      return e->u.ex;
    case Tr_cx: {
      Temp_temp r = Temp_newtemp();
      Temp_label t = Temp_newlabel(), f = Temp_newlabel();
      doPatch(e->u.cx.trues, t);
      doPatch(e->u.cx.falses, f);
      // T_Temp(r) <- 1 if stm is true else T_Temp(r) <- 0
      return T_Eseq(T_Move(T_Temp(r), T_Const(1)),
                    T_Eseq(e->u.cx.stm,
                           T_Eseq(T_Label(f),
                                  T_Eseq(T_Move(T_Temp(r), T_Const(0)),
                                         T_Eseq(T_Label(t),
                                                T_Temp(r))))));
    }
    case Tr_nx:
      return T_Eseq(e->u.nx, T_Const(0));
  }
  assert(0);
}

static T_stm unNx(Tr_exp e) {
  switch (e->kind) {
    case Tr_ex:
      return T_Exp(e->u.ex);
    case Tr_cx:
      return e->u.cx.stm; // just do the stm
    case Tr_nx:
      return e->u.nx;
  }
  assert(0);
}

static struct Cx unCx(Tr_exp e) {
  switch (e->kind) {
    case Tr_ex: {
      struct Cx cx;
      /* treat direct jump specially */
      if (e->u.ex->kind == T_CONST) {
        cx.stm = T_Jump(T_Name(NULL), Temp_LabelList(NULL, NULL));
        patchList pl = PatchList(&(cx.stm->u.JUMP.exp->u.NAME), PatchList(&(cx.stm->u.JUMP.jumps->head), NULL));
        if (e->u.ex->u.CONST == 0) {
          cx.falses = pl;
          return cx;
        } else if (e->u.ex->u.CONST == 1) {
          cx.trues = pl;
          return cx;
        }
      }
      cx.stm = T_Cjump(T_ne, e->u.ex, T_Const(0), NULL, NULL);
      cx.trues = PatchList(&(cx.stm->u.CJUMP.true), NULL);
      cx.falses = PatchList(&(cx.stm->u.CJUMP.false), NULL);
      return cx;
    }
    case Tr_cx:
      return e->u.cx;
    case Tr_nx:
      assert(0); // a right compiler will never trans a nx to cx
  }
  assert(0);
}

static T_exp staticLink(Tr_level callee, Tr_level caller) {
  T_exp fp = T_Temp(F_FP());
  Tr_level l = caller;
  while (l && l != callee->parent) {
    F_access staticLink = F_formals(l->frame)->head;
    fp = F_Exp(staticLink, fp);
    l = l->parent;
  }
  return fp;
}

static T_binOp Tr_binOp(A_oper op) {
  switch (op) {
    case A_plusOp:
      return T_plus;
    case A_minusOp:
      return T_minus;
    case A_timesOp :
      return T_mul;
    case A_divideOp:
      return T_div;
    default:
      assert(0);
  }
}

static T_relOp Tr_relOp(A_oper op) {
  switch (op) {
    case A_eqOp:
      return T_eq;
    case A_neqOp:
      return T_ne;
    case A_leOp:
      return T_le;
    case A_ltOp:
      return T_lt;
    case A_geOp:
      return T_ge;
    case A_gtOp:
      return T_gt;
    default:
      assert(0);
  }
}

/* main translation */
Tr_exp Tr_simpleVar(Tr_access a, Tr_level level) {
  T_exp fp = T_Temp(F_FP()); // 当前的fp
  while (level && level != a->level) {
    F_access sl = F_formals(level->frame)->head;
    fp = F_Exp(sl, fp); // 不停地把fp替换成新的IR tree（MEM（fp，offset））
    level = level->parent;
  }
  return Tr_Ex(F_Exp(a->access, fp));
}

Tr_exp Tr_fieldVar(Tr_exp b, int off) {
  return Tr_Ex(T_Mem(T_Binop(T_plus, unEx(b), T_Binop(T_mul, T_Const(off), T_Const(F_wordSize)))));
}

Tr_exp Tr_subscriptVar(Tr_exp b, Tr_exp i) {
  return Tr_Ex(T_Mem(T_Binop(T_plus, unEx(b), T_Binop(T_mul, unEx(i), T_Const(F_wordSize)))));
}

Tr_exp Tr_nilExp() {
  return Tr_Nx(T_Exp(T_Const(0)));
}

Tr_exp Tr_intExp(int i) {
  return Tr_Ex(T_Const(i));
}

Tr_exp Tr_stringExp(string s) {
  Temp_label l = Temp_newlabel();
  int len = strlen(s);
  char *buf = checked_malloc(len + sizeof(int));
  (*(int *) buf) = len;
  strncpy(buf + 4, s, len);
  F_frag f = F_StringFrag(l, buf);
  fragList = F_FragList(f, fragList);
  return Tr_Ex(T_Name(l));
}

Tr_exp Tr_binExp(A_oper op, Tr_exp left, Tr_exp right) {
  return Tr_Ex(T_Binop(Tr_binOp(op), unEx(left), unEx(right)));
}

Tr_exp Tr_relExp(A_oper op, Tr_exp left, Tr_exp right) {
  T_stm stm = T_Cjump(Tr_relOp(op), unEx(left), unEx(right), NULL, NULL);
  patchList trues = PatchList(&(stm->u.CJUMP.true), NULL);
  patchList falses = PatchList(&(stm->u.CJUMP.false), NULL);
  return Tr_Cx(trues, falses, stm);
}

Tr_exp Tr_arrayExp(Tr_exp size, Tr_exp init) {
  return Tr_Ex(F_externalCall("initArray", T_ExpList(unEx(size), T_ExpList(unEx(init), NULL))));
}

Tr_exp Tr_recordExp(Tr_expList fields) {
  Temp_temp r = Temp_newtemp(); // the temp to store the pointer of record

  // calculate space
  int count = 0;
  for(Tr_expList tmp = fields; tmp; tmp = tmp->tail)
    count++;

  // allocate a count * F_wordSize size space
  T_stm alloc = T_Move(T_Temp(r), F_externalCall("malloc", T_ExpList(T_Const(count * F_wordSize), NULL)));

  // Move the result of ei to offset i * F_wordSize
  T_stm move = T_Move(T_Mem(T_Binop(T_plus, T_Temp(r), T_Const(0))), unEx(fields->head));
  fields = fields->tail;
  for (int i = 1; fields; fields = fields->tail, i++) {
    move = T_Seq(move, T_Move(T_Mem(T_Binop(T_plus, T_Temp(r), T_Const(i * F_wordSize))), unEx(fields->head)));
  }
  return Tr_Ex(T_Eseq(T_Seq(alloc, move), T_Temp(r)));
}

Tr_exp Tr_seqExp(Tr_expList exps) {
  if(!exps) return Tr_nilExp();
  T_exp l = unEx(exps->head);
  exps = exps->tail;
  for (; exps; exps = exps->tail) {
    l = T_Eseq(unNx(Tr_Ex(l)), unEx(exps->head));
  }
  return Tr_Ex(l);
}

Tr_exp Tr_assignExp(Tr_exp lvalue, Tr_exp value) {
  return Tr_Nx(T_Move(unEx(lvalue), unEx(value)));
}

Tr_exp Tr_callExp(Temp_label label, Tr_level callee, Tr_level caller, Tr_expList args) {
  /* make formal list */
  T_expList arglist = NULL;
  T_expList tail = NULL;
  for (; args; args = args->tail) {
    T_exp t = unEx(args->head);
    if (arglist == NULL) {
      arglist = tail = T_ExpList(t, NULL);
    } else {
      tail->tail = T_ExpList(t, NULL);
      tail = tail->tail;
    }
  }

  for (Temp_labelList l = F_preDefineFuncs() ; l; l= l->tail) {
    if (l->head == label)
      return Tr_Ex(F_externalCall(Temp_labelstring(label), arglist));
  }

  /* get static link */
  T_exp sl = staticLink(callee, caller);
  return Tr_Ex(T_Call(T_Name(label), T_ExpList(sl, arglist)));
}

Tr_exp Tr_ifExp(Tr_exp test, Tr_exp then, Tr_exp elsee) {
  Temp_label t = Temp_newlabel(), f = Temp_newlabel(), done = Temp_newlabel();
  Temp_temp r = Temp_newtemp();

  struct Cx c = unCx(test);
  doPatch(c.trues, t);
  doPatch(c.falses, f);

  /* the simplest version */
  /* TODO:treat specially when then or else is Cx, P177 */
  if (elsee == NULL) {
    T_stm nx = T_Seq(c.stm, T_Seq(T_Label(t), T_Seq(unNx(then), T_Label(f))));
    return Tr_Nx(nx);
  } else {
    T_exp ex = T_Eseq(T_Seq(c.stm,
                              T_Seq(T_Label(t),
                                    T_Seq(T_Move(T_Temp(r), unEx(then)), // move then to r
                                          T_Seq(T_Jump(T_Name(done), Temp_LabelList(done, NULL)),
                                                T_Seq(T_Label(f),
                                                      T_Seq(T_Move(T_Temp(r), unEx(elsee)), // move else to r
                                                            T_Seq(T_Jump(T_Name(done), Temp_LabelList(done, NULL)),
                                                                  T_Label(done)))))))), T_Temp(r));
    return Tr_Ex(ex);
  }
}

Tr_exp Tr_whileExp(Tr_exp test, Tr_exp body, Temp_label done) {
  Temp_label t = Temp_newlabel(), c = Temp_newlabel();

  struct Cx cx = unCx(test);
  doPatch(cx.trues, c); doPatch(cx.falses, done);

  T_stm goto_test = T_Jump(T_Name(t), Temp_LabelList(t, NULL));
  T_stm nx = T_Seq(T_Label(t), // test:
                   T_Seq(cx.stm, // if not (condition) goto done else goto continue
                         T_Seq(T_Label(c), // continue:
                               T_Seq(unNx(body), // body
                                     T_Seq(goto_test, // goto test
                                           T_Label(done)))))); // done:
  return Tr_Nx(nx);
}

Tr_exp Tr_forExp(Tr_access i, Tr_exp lo, Tr_exp hi, Tr_exp body, Temp_label done) {
  Temp_label t = Temp_newlabel(), b = Temp_newlabel();
  T_exp iexp = F_Exp(i->access, T_Temp(F_FP()));

  T_stm goto_test = T_Jump(T_Name(t), Temp_LabelList(t, NULL));
  T_stm i_plus_1 = T_Move(iexp, unEx(Tr_binExp(A_plusOp, Tr_Ex(iexp), Tr_Ex(T_Const(1)))));
  T_stm nx = T_Seq(T_Move(iexp, unEx(lo)), // i = lo
                   T_Seq(T_Label(t), // test:
                         T_Seq(T_Cjump(T_le, iexp, unEx(hi), b, done), // if i <= hi goto body else done
                               T_Seq(T_Label(b), // body:
                                     T_Seq(unNx(body), // body
                                           T_Seq(i_plus_1, // i = i + 1
                                                 T_Seq(goto_test, // goto test
                                                       T_Label(done))))))));
  return Tr_Nx(nx);
}

Tr_exp Tr_breakExp(Temp_label done) {
  return Tr_Nx(T_Jump(T_Name(done), Temp_LabelList(done, NULL)));
}

Tr_exp Tr_stringEqualExp(Tr_exp left, Tr_exp right) {
  return Tr_Ex(F_externalCall("stringEqual", T_ExpList(unEx(left), T_ExpList(unEx(right), NULL))));
}

void Tr_print(Tr_exp e) {
  printStmList(stdout, T_StmList(unNx(e), NULL));
}

/* fragment */
void Tr_procEntryExit(Tr_level level, Tr_exp body, Tr_accessList formals) {
  // (6)(7) body + 将返回值传至专用于返回结果的寄存器
  Temp_temp t = Temp_newtemp();
  T_stm stm = T_Seq(T_Move(T_Temp(t), unEx(body)), T_Move(T_Temp(F_RV()), T_Temp(t)));
  // (4)将escape变量传入frame，将non-escape传入temp register
  // (5)(8)保存和恢复callee-saved register
  stm = F_procEntryExit1(level->frame, stm);
  F_frag f = F_ProcFrag(stm, level->frame);
  fragList = F_FragList(f, fragList); // add (4-8) stm to fragList as a ProcFrag
}

F_fragList Tr_getResult(void) {
  return fragList;
}