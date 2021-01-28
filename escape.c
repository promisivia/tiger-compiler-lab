#include <stdio.h>
#include <string.h>
#include "util.h"
#include "symbol.h"
#include "absyn.h"
#include "escape.h"
#include "table.h"
#include "env.h"
#include "helper.h"

static void traverseExp(S_table env, int depth, A_exp e);

static void traverseDec(S_table env, int depth, A_dec d);

static void traverseVar(S_table env, int depth, A_var v);

static void traverseExp(S_table env, int depth, A_exp e) {
  switch (e->kind) {
    case A_varExp:
      traverseVar(env, depth, e->u.var);
      break;
    case A_callExp: {
      A_expList args = e->u.call.args;
      for (; args; args = args->tail) {
        traverseExp(env, depth, args->head);
      }
      break;
    }
    case A_opExp:
      traverseExp(env, depth, e->u.op.left);
      traverseExp(env, depth, e->u.op.right);
      break;
    case A_recordExp: {
      A_efieldList el = get_recordexp_fields(e);
      for (; el; el = el->tail) {
        traverseExp(env, depth, el->head->exp);
      }
      return;
    }
    case A_seqExp: {
      A_expList expList = get_seqexp_seq(e);
      for (; expList; expList = expList->tail) {
        traverseExp(env, depth, expList->head);
      }
      return;
    }
    case A_assignExp: {
      traverseExp(env, depth, get_assexp_exp(e));
      traverseVar(env, depth, get_assexp_var(e));
      return;
    }
    case A_ifExp: {
      traverseExp(env, depth, get_ifexp_test(e));
      traverseExp(env, depth, get_ifexp_then(e));
      if (get_ifexp_else(e)) {
        traverseExp(env, depth, get_ifexp_else(e));
      }
      return;
    }
    case A_whileExp: {
      traverseExp(env, depth, get_whileexp_test(e));
      traverseExp(env, depth, get_whileexp_body(e));
      return;
    }
    case A_forExp: {
      traverseExp(env, depth, get_forexp_lo(e));
      traverseExp(env, depth, get_forexp_hi(e));
      S_beginScope(env);
      get_forexp_escape(e) = FALSE;
      S_enter(env, get_forexp_var(e), E_EscapeEntry(depth, &(get_forexp_escape(e))));
      traverseExp(env, depth, get_forexp_body(e));
      S_endScope(env);
      return;
    }
    case A_letExp: {
      S_beginScope(env);
      A_decList d = get_letexp_decs(e);
      for (; d; d = d->tail) {
        traverseDec(env, depth, d->head);
      }
      traverseExp(env, depth, get_letexp_body(e));
      S_endScope(env);
      return;
    }
    case A_arrayExp: {
      traverseExp(env, depth, get_arrayexp_size(e));
      traverseExp(env, depth, get_arrayexp_init(e));
      return;
    }
    default:
      return;
  }
}

static void traverseDec(S_table env, int depth, A_dec d) {
  /* 每当发现变量或函数参数声明时,将x的escape设为false
     引入一个新的表，在d深度绑定一个symbol(EscapeEntry) */
  switch (d->kind) {
    case A_functionDec: {
      A_fundecList decs = get_funcdec_list(d);
      for (; decs; decs = decs->tail) {
        A_fundec dec = decs->head;
        for (A_fieldList formals = dec->params; formals; formals = formals->tail) {
          A_field formal = formals->head;
          formal->escape = FALSE;
          S_enter(env, formal->name, E_EscapeEntry(depth, &formal->escape));
        }
        traverseExp(env, depth + 1, decs->head->body);
      }
      return;
    }
    case A_varDec: {
      get_vardec_escape(d) = FALSE;
      S_enter(env, get_vardec_var(d), E_EscapeEntry(depth, &(get_vardec_escape(d))));
      traverseExp(env, depth, get_vardec_init(d));
      return;
    }
    case A_typeDec:
      return;
  }
}

static void traverseVar(S_table env, int depth, A_var v) {
  switch (v->kind) {
    case A_simpleVar: {
      E_enventry x = S_look(env, get_simplevar_sym(v));
      if (x && depth > x->u.esc.depth) {
        *(x->u.esc.escape) = TRUE;
      }
      return;
    }
    case A_fieldVar: {
      traverseVar(env, depth, get_fieldvar_var(v));
      return;
    }
    case A_subscriptVar: {
      traverseVar(env, depth, get_subvar_var(v));
      traverseExp(env, depth, get_subvar_exp(v));
      return;
    }
  }
}

void Esc_findEscape(A_exp exp) {
  S_table env = S_empty();
  traverseExp(env, 0, exp);
}
