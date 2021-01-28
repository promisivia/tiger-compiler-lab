#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "util.h"
#include "errormsg.h"
#include "symbol.h"
#include "absyn.h"
#include "prabsyn.h"
#include "types.h"
#include "env.h"
#include "semant.h"
#include "helper.h"
#include "translate.h"

/*Lab5: Your implementation of lab5.*/

struct expty {
  Tr_exp exp;
  Ty_ty ty;
};

//In Lab4, the first argument exp should always be **NULL**.
struct expty expTy(Tr_exp exp, Ty_ty ty) {
  struct expty e;

  e.exp = exp;
  e.ty = ty;

  return e;
}

Ty_ty actual_ty(Ty_ty ty) {
  if (ty == NULL) return Ty_Void();
  if (ty->kind == Ty_name) {
    return actual_ty(ty->u.name.ty);
  } else {
    return ty;
  }
}

bool tyeq(Ty_ty t1, Ty_ty t2) {
  Ty_ty ty1 = actual_ty(t1);
  Ty_ty ty2 = actual_ty(t2);

  if (ty1->kind == Ty_int || ty1->kind == Ty_string)
    return ty1->kind == ty2->kind;

  if (ty1->kind == Ty_nil && ty2->kind == Ty_nil)
    return FALSE;

  if (ty1->kind == Ty_nil)
    return ty2->kind == Ty_record || ty2->kind == Ty_array;

  if (ty2->kind == Ty_nil)
    return ty1->kind == Ty_record || ty1->kind == Ty_array;

  return ty1 == ty2;
}

struct expty transVar(S_table venv, S_table tenv, A_var v, Tr_level l, Temp_label breakk) {
  switch (v->kind) {
    case A_simpleVar: {
      E_enventry x = S_look(venv, get_simplevar_sym(v));
      if (x && x->kind == E_varEntry) {
        return expTy(Tr_simpleVar(get_var_access(x), l), actual_ty(get_varentry_type(x)));
      } else {
        EM_error(v->pos, "undefined variable %s", S_name(get_simplevar_sym(v)));
        return expTy(Tr_nilExp(), Ty_Int());
      }
    }
    case A_fieldVar: {
      struct expty e = transVar(venv, tenv, get_fieldvar_var(v), l, breakk);
      if (get_expty_kind(e) == Ty_record) {
        Ty_fieldList f;
        int i = 0;
        for (f = get_record_fieldlist(e); f; f = f->tail, i++) {
          if (f->head->name == get_fieldvar_sym(v)) {
            return expTy(Tr_fieldVar(e.exp, i), actual_ty(f->head->ty));
          }
        }
        EM_error(get_fieldvar_var(v)->pos, "field %s doesn't exist", S_name(get_fieldvar_sym(v)));
        return expTy(Tr_nilExp(), Ty_Int());
      } else {
        EM_error(get_fieldvar_var(v)->pos, "not a record type");
        return expTy(Tr_nilExp(), Ty_Int());
      }
    }
    case A_subscriptVar: {
      struct expty ee = transExp(venv, tenv, get_subvar_exp(v), l, breakk);
      if (get_expty_kind(ee) != Ty_int) {
        EM_error(get_subvar_exp(v)->pos, "interger subscript required");
        return expTy(Tr_nilExp(), Ty_Int());
      }
      struct expty ev = transVar(venv, tenv, get_subvar_var(v), l, breakk);
      if (get_expty_kind(ev) == Ty_array) {
        return expTy(Tr_subscriptVar(ev.exp, ee.exp), actual_ty(get_array(ev)));
      } else {
        EM_error(get_subvar_var(v)->pos, "array type required");
        return expTy(Tr_nilExp(), Ty_Int());
      }
    }
  }
}

struct expty transExp(S_table venv, S_table tenv, A_exp a, Tr_level l, Temp_label breakk) {
  switch (a->kind) {
    case A_varExp: {
      return transVar(venv, tenv, a->u.var, l, breakk);
    }
    case A_nilExp: {
      return expTy(Tr_nilExp(), Ty_Nil());
    }
    case A_intExp: {
      return expTy(Tr_intExp(a->u.intt), Ty_Int());
    }
    case A_stringExp: {
      return expTy(Tr_stringExp(a->u.stringg), Ty_String());
    }
    case A_callExp: {
      S_symbol func = get_callexp_func(a);
      A_expList args = get_callexp_args(a);

      //check if func exist
      E_enventry e = S_look(venv, func);
      if (!e || e->kind != E_funEntry) {
        EM_error(a->pos, "undefined function %s", S_name(func));
        return expTy(Tr_nilExp(), Ty_Void());
      }

      //check if formals and args match
      Ty_tyList formals = get_func_tylist(e);
      Tr_expList param = NULL, tail = NULL;
      for (; formals && args; formals = formals->tail, args = args->tail) {
        struct expty exp = transExp(venv, tenv, args->head, l, breakk);
        if (!tyeq(exp.ty, formals->head)) {
          EM_error(a->pos, "para type mismatch");
          return expTy(Tr_nilExp(), Ty_Void());
        }
        //param = Tr_ExpList(exp.exp, param);
        if (param == NULL) {
          param = tail = Tr_ExpList(exp.exp, NULL);
        } else {
          tail->tail = Tr_ExpList(exp.exp, NULL);
          tail = tail->tail;
        }
      }
      if (formals) {
        EM_error(a->pos, "too many params in function %s", S_name(get_callexp_func(a)));
        return expTy(Tr_nilExp(), Ty_Void());
      }
      if (args) {
        EM_error(a->pos, "too many params in function %s", S_name(func));
        return expTy(Tr_nilExp(), Ty_Void());
      }
      return expTy(Tr_callExp(get_func_label(e), get_func_level(e), l, param), actual_ty(get_func_res(e)));
    }
    case A_opExp: {
      A_oper oper = get_opexp_oper(a);
      struct expty left = transExp(venv, tenv, get_opexp_left(a), l, breakk);
      struct expty right = transExp(venv, tenv, get_opexp_right(a), l, breakk);
      if (oper == A_plusOp || oper == A_minusOp || oper == A_timesOp || oper == A_divideOp) {
        if (left.ty->kind != Ty_int) {
          EM_error(get_opexp_leftpos(a), "integer required");
        }
        if (right.ty->kind != Ty_int) {
          EM_error(get_opexp_rightpos(a), "integer required");
        }
        return expTy(Tr_binExp(oper, left.exp, right.exp), Ty_Int());
      }
      if (oper == A_eqOp || oper == A_neqOp || oper == A_ltOp || oper == A_leOp || oper == A_gtOp || oper == A_geOp) {
        if (!tyeq(left.ty, right.ty)) {
          EM_error(get_opexp_leftpos(a), "same type required");
        }
        // for string compare
        if (right.ty->kind == Ty_string && oper == A_eqOp) {
          return expTy(Tr_stringEqualExp(left.exp, right.exp), Ty_Int());
        }
        return expTy(Tr_relExp(oper, left.exp, right.exp), Ty_Int());
      }
    }
    case A_arrayExp: {
      Ty_ty type = actual_ty(S_look(tenv, get_arrayexp_typ(a)));
      if (type == NULL) {
        EM_error(a->pos, "undefined type %s", S_name(get_arrayexp_typ(a)));
        return expTy(NULL, Ty_Int());
      }
      if (type->kind != Ty_array) {
        EM_error(a->pos, "type %s must be array", S_name(a->u.array.typ));
        return expTy(NULL, type);
      }
      struct expty size = transExp(venv, tenv, get_arrayexp_size(a), l, breakk);
      struct expty init = transExp(venv, tenv, get_arrayexp_init(a), l, breakk);
      if (!tyeq(init.ty, actual_ty(type)->u.array)) {
        EM_error(a->pos, "type mismatch");
        return expTy(Tr_nilExp(), Ty_Int());
      }
      return expTy(Tr_arrayExp(size.exp, init.exp), actual_ty(type));
    }
    case A_recordExp: {
      // check if type exist
      Ty_ty type = actual_ty(S_look(tenv, get_recordexp_typ(a)));
      if (type == NULL) {
        EM_error(a->pos, "undefined type %s", S_name(get_recordexp_typ(a)));
        return expTy(Tr_nilExp(), Ty_Record(NULL));
      }
      // check whether field match
      Tr_expList head = NULL, tail = NULL;
      Ty_fieldList record = get_ty_record(type);
      A_efieldList fields = get_recordexp_fields(a);

      for (; record && fields; record = record->tail, fields = fields->tail) {
        struct expty tmp = transExp(venv, tenv, fields->head->exp, l, breakk);
        // check name match
        if (S_name(record->head->name) != S_name(fields->head->name)) {
          EM_error(a->pos, "type mismatch");
          return expTy(Tr_nilExp(), type);
        }
        // construct
        if (head == NULL) {
          head = tail = Tr_ExpList(tmp.exp, NULL);
        } else {
          tail->tail = Tr_ExpList(tmp.exp, NULL);
          tail = tail->tail;
        }
      }
      // record and fields length mismatch
      if (record || fields) {
        EM_error(a->pos, "type mismatch");
        return expTy(Tr_nilExp(), type);
      }
      return expTy(Tr_recordExp(head), type);
    }
    case A_seqExp: {
      A_expList exps = get_seqexp_seq(a);
      struct expty last = expTy(Tr_nilExp(), Ty_Void());

      Tr_expList seq = NULL, tail = NULL;
      for (; exps; exps = exps->tail) {
        last = transExp(venv, tenv, exps->head, l, breakk);
        if (seq == NULL) {
          seq = tail = Tr_ExpList(last.exp, NULL);
        } else {
          tail->tail = Tr_ExpList(last.exp, NULL);
          tail = tail->tail;
        }
      }
      return expTy(Tr_seqExp(seq), last.ty);
    }
    case A_assignExp: {
      struct expty val = transExp(venv, tenv, get_assexp_exp(a), l, breakk);
      struct expty lvalue = transVar(venv, tenv, get_assexp_var(a), l, breakk);

      // check whether type match
      if (!tyeq(val.ty, lvalue.ty)) {
        EM_error(a->pos, "unmatched assign exp");
      }
      // check whether this is a loop variable
      if (get_assexp_var(a)->kind == A_simpleVar) {
        E_enventry x = S_look(venv, get_simplevar_sym(get_assexp_var(a)));
        if (x && x->readonly == TRUE)
          EM_error(a->pos, "loop variable can't be assigned");
      }
      return expTy(Tr_assignExp(lvalue.exp, val.exp), Ty_Void());
    }
    case A_ifExp: {
      struct expty test = transExp(venv, tenv, get_ifexp_test(a), l, breakk);
      if (test.ty->kind != Ty_int)
        EM_error(a->u.iff.test->pos, "expression must be integer");

      struct expty then = transExp(venv, tenv, get_ifexp_then(a), l, breakk);
      if (get_ifexp_else(a) == NULL) {
        if (get_expty_kind(then) != Ty_void) {
          EM_error(a->pos, "if-then exp's body must produce no value");
        }
        return expTy(Tr_ifExp(test.exp, then.exp, NULL), Ty_Void());
      } else {
        struct expty ee = transExp(venv, tenv, get_ifexp_else(a), l, breakk);
        if (!tyeq(ee.ty, then.ty)) {
          EM_error(a->pos, "then exp and else exp type mismatch");
        }
        return expTy(Tr_ifExp(test.exp, then.exp, ee.exp), then.ty);
      }
    }
    case A_whileExp: {
      struct expty test = transExp(venv, tenv, get_whileexp_test(a), l, breakk);
      if (test.ty->kind != Ty_int) {
        EM_error(a->u.whilee.test->pos, "expression must be integer");
      }

      Temp_label done = Temp_newlabel();
      struct expty body = transExp(venv, tenv, get_whileexp_body(a), l, done);
      if (get_expty_kind(body) != Ty_void) {
        EM_error(a->pos, "while body must produce no value");
      }
      return expTy(Tr_whileExp(test.exp, body.exp, done), Ty_Void());
    }
    case A_forExp: {
      struct expty lo = transExp(venv, tenv, get_forexp_lo(a), l, breakk);
      struct expty hi = transExp(venv, tenv, get_forexp_hi(a), l, breakk);

      if (get_expty_kind(lo) != Ty_int) {
        EM_error(get_forexp_lo(a)->pos, "for exp's range type is not integer");
      }
      if (get_expty_kind(hi) != Ty_int) {
        EM_error(get_forexp_hi(a)->pos, "for exp's range type is not integer");
      }
      Tr_access loopVar = Tr_allocLocal(l, TRUE);
      S_beginScope(venv);
      S_enter(venv, get_forexp_var(a), E_ROVarEntry(loopVar, Ty_Int()));
      Temp_label done = Temp_newlabel();
      struct expty body = transExp(venv, tenv, get_forexp_body(a), l, done);
      if (body.ty->kind != Ty_void)
        EM_error(a->u.forr.body->pos, "for body must produce no value");

      S_endScope(venv);
      return expTy(Tr_forExp(loopVar, lo.exp, hi.exp, body.exp, done), Ty_Void());
    }
    case A_breakExp: {
      if (!breakk) {
        return expTy(Tr_nilExp(), Ty_Void());
      }
      return expTy(Tr_breakExp(breakk), Ty_Void());
    }
    case A_letExp: {
      Tr_expList exps = NULL;
      Tr_expList tail = NULL;
      S_beginScope(venv);
      S_beginScope(tenv);
      for (A_decList decs = get_letexp_decs(a); decs; decs = decs->tail) {
        Tr_exp exp = transDec(venv, tenv, decs->head, l, breakk);
        if (exps == NULL) {
          exps = tail = Tr_ExpList(exp, NULL);
        } else {
          tail->tail = Tr_ExpList(exp, NULL);
          tail = tail->tail;
        }
      }
      struct expty e = transExp(venv, tenv, get_letexp_body(a), l, breakk);
      tail->tail = Tr_ExpList(e.exp, NULL);
      S_endScope(tenv);
      S_endScope(venv);
      return expTy(Tr_seqExp(exps), e.ty);
    }
  }
}

Ty_tyList makeFormalTyList(S_table tenv, A_fieldList fieldList) {
  A_fieldList f;
  Ty_tyList th = NULL, tl = NULL;
  Ty_ty t;
  for (f = fieldList; f; f = f->tail) {
    t = S_look(tenv, f->head->typ);
    if (t == NULL) {
      EM_error(f->head->pos, "undefined type %s", f->head->typ);
      t = Ty_Int();
    }
    if (th == NULL) {
      th = tl = Ty_TyList(t, NULL);
    } else {
      tl->tail = Ty_TyList(t, NULL);
      tl = tl->tail;
    }
  }
  return th;
}

U_boolList makeFormalBoolList(A_fieldList fieldList) {
  U_boolList head = NULL, tail = NULL;
  for (A_fieldList f = fieldList; f; f = f->tail) {
    bool escape = f->head->escape;
    if (head == NULL) {
      head = tail = U_BoolList(escape, NULL);
    } else {
      tail->tail = U_BoolList(escape, NULL);
      tail = tail->tail;
    }
  }
  return head;
}

Tr_exp transDec(S_table venv, S_table tenv, A_dec d, Tr_level l, Temp_label breakk) {
  switch (d->kind) {
    case A_varDec: {
      struct expty e = transExp(venv, tenv, get_vardec_init(d), l, breakk);
      Tr_access a = Tr_allocLocal(l, d->u.var.escape);
      if (get_vardec_typ(d)) {
        /* case: var x : type-id := exp */
        Ty_ty t = S_look(tenv, get_vardec_typ(d));
        if (t == NULL) EM_error(d->pos, "undefined type %s", S_name(get_vardec_typ(d)));
        if (!tyeq(t, e.ty)) EM_error(d->pos, "type mismatch");
        S_enter(venv, get_vardec_var(d), E_VarEntry(a, t));
      }
      else {
        /* case: var x := exp */
        if (get_expty_kind(e) == Ty_nil) {
          EM_error(d->pos, "init should not be nil without type specified");
          S_enter(venv, get_vardec_var(d), E_VarEntry(a, Ty_Void()));
        } else {
          S_enter(venv, get_vardec_var(d), E_VarEntry(a, e.ty));
        }
      }
      return Tr_assignExp(Tr_simpleVar(a, l), e.exp);
    }
    case A_typeDec: {
      // find duplicated type declare
      for (A_nametyList list = get_typedec_list(d); list; list = list->tail) {
        S_symbol name = list->head->name;
        if (S_look(tenv, name))
           EM_error(d->pos, "two types have the same name");
        S_enter(tenv, name, Ty_Name(name, NULL));
      }
      for (A_nametyList t = get_typedec_list(d); t; t = t->tail) {
        Ty_ty ty = S_look(tenv, t->head->name);
        ty->u.name.ty = transTy(tenv, t->head->ty);
      }
      /* Check illegal cycle */
      for (A_nametyList t = get_typedec_list(d); t; t = t->tail) {
        Ty_ty ty = S_look(tenv, t->head->name);
        Ty_ty tmp = ty;
        while (tmp->kind == Ty_name) {
          tmp = tmp->u.name.ty;
          if (tmp == ty) {
            EM_error(d->pos, "illegal type cycle");
            return Tr_nilExp();
          }
        }
      }
      return Tr_nilExp();
    }
    case A_functionDec: {
      for (A_fundecList funcs = get_funcdec_list(d); funcs; funcs = funcs->tail) {
        A_fundec func = funcs->head;
        if (S_look(venv, func->name)) {
          EM_error(d->pos, "two functions have the same name");
          continue;
        }
        Ty_ty resultTy;
        if (func->result) {
          resultTy = S_look(tenv, func->result);
          if (!resultTy) {
            EM_error(func->pos, "undefined result type %s", S_name(func->result));
            resultTy = Ty_Void();
          }
        } else resultTy = Ty_Void();

        Ty_tyList formalTys = makeFormalTyList(tenv, func->params);
        U_boolList formalEscapes = makeFormalBoolList(func->params);
        Temp_label newLabel = Temp_namedlabel(S_name(func->name));
        Tr_level newLevel = Tr_newLevel(l, newLabel, formalEscapes);
        S_enter(venv, func->name, E_FunEntry(newLevel, newLabel, formalTys, resultTy));
      }

      // function bodies
      for (A_fundecList funcs = get_funcdec_list(d); funcs; funcs = funcs->tail) {
        A_fundec func = funcs->head;
        E_enventry x = S_look(venv, func->name);
        S_beginScope(venv);

        A_fieldList args;
        Ty_tyList t;
        Tr_accessList al = Tr_formals(get_func_level(x));
        for (args = func->params, t = get_func_tylist(x); args; args = args->tail, t = t->tail, al = al->tail) {
          S_enter(venv, args->head->name, E_VarEntry(al->head, t->head));
        }

        // check return type
        struct expty body = transExp(venv, tenv, func->body, get_func_level(x), breakk);
        if (!tyeq(body.ty, x->u.fun.result)) {
          if (x->u.fun.result->kind == Ty_void) {
            EM_error(func->pos, "procedure returns value");
          } else {
            EM_error(func->pos, "function body return type mismatch: %s", S_name(func->name));
          }
        }

        Tr_procEntryExit(get_func_level(x), body.exp, al);
        S_endScope(venv);
      }
      return Tr_nilExp();
    }
  }
}

Ty_ty transTy(S_table tenv, A_ty a) {
  switch (a->kind) {
    case A_nameTy: {
      Ty_ty t = S_look(tenv, get_ty_name(a)); // Don't try to get actual ty
      if (t != NULL) return t;
      EM_error(a->pos, "undefined type %s", S_name(get_ty_name(a)));
      return Ty_Int();
    }
    case A_recordTy: {
      Ty_fieldList head = NULL, tail = NULL;
      for (A_fieldList fields = get_ty_record(a); fields; fields = fields->tail) {
        A_field field = fields->head;
        Ty_ty t = S_look(tenv, field->typ);
        if (t == NULL) {
          EM_error(field->pos, "undefined type %s", S_name(fields->head->typ));
          t = Ty_Int();
        }
        if (head == NULL) {
          head = tail = Ty_FieldList(Ty_Field(fields->head->name, t), NULL);
        } else {
          tail->tail = Ty_FieldList(Ty_Field(fields->head->name, t), NULL);
          tail = tail->tail;
        }
      }
      return Ty_Record(head);
    }
    case A_arrayTy: {
      Ty_ty t = S_look(tenv, get_ty_array(a));
      if (t != NULL) {
        return Ty_Array(t);
      } else {
        EM_error(a->pos, "undefined type %s", S_name(get_ty_array(a)));
        return Ty_Array(Ty_Int());
      }
    }
  }
}

F_fragList SEM_transProg(A_exp exp) {
  Tr_level outermost = Tr_outermost();
  struct expty main = transExp(E_base_venv(), E_base_tenv(), exp, outermost, NULL);
  Tr_procEntryExit(outermost, main.exp, NULL); // do (4 - 8)
  return Tr_getResult();
}

