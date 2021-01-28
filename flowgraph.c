#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "flowgraph.h"
#include "errormsg.h"
#include "table.h"

// 指令n定义的变量列表
Temp_tempList FG_def(G_node n) {
  AS_instr inst = (AS_instr) G_nodeInfo(n);
  if (inst->kind == I_OPER) {
    return inst->u.OPER.dst;
  } else if (inst->kind == I_MOVE) {
    return inst->u.MOVE.dst;
  }
  return NULL;
}

// 指令n使用过的变量列表
Temp_tempList FG_use(G_node n) {
  AS_instr inst = (AS_instr) G_nodeInfo(n);
  if (inst->kind == I_OPER) {
    return inst->u.OPER.src;
  } else if (inst->kind == I_MOVE) {
    return inst->u.MOVE.src;
  }
  return NULL;
}

// 判断是否是move指令，后续可以决定是否可以删除
bool FG_isMove(G_node n) {
  AS_instr inst = (AS_instr) G_nodeInfo(n);
  return inst->kind == I_MOVE;
}

//返回流图
//流图带有独立封装的辅助信息（通过table）
G_graph FG_AssemFlowGraph(AS_instrList il) {
  G_graph graph = G_Graph();
  TAB_table label2node = TAB_empty();
  G_node prev = NULL; // 如果前一条是jump，那么就记录为NULL
  G_nodeList jump_nodes = NULL;

  /* Build flow graph skeleton */
  for (; il; il = il->tail) {
    AS_instr inst = il->head;
    G_node node = G_Node(graph, (void *) inst);
    // prev is not a jump operate
    if (prev) {
      G_addEdge(prev, node);
    }
    // record LABEL to node
    if (inst->kind == I_LABEL) {
      TAB_enter(label2node, inst->u.LABEL.label, node);
    }
    // jmp operate
    if (inst->kind == I_OPER && !strncmp("jmp", inst->u.OPER.assem, 3)) {
      prev = NULL;
    } else {
      prev = node;
    }
    // record jump related node list (beside jmp)
    if (inst->kind == I_OPER && inst->u.OPER.jumps) {
      jump_nodes = G_NodeList(node, jump_nodes);
    }
  }

  /* add jump targets */
  /* 现在的edge完全没有涉及任何的jump操作 */
  for (; jump_nodes; jump_nodes = jump_nodes->tail) {
    G_node node = jump_nodes->head;
    AS_instr instr = G_nodeInfo(node);
    for (Temp_labelList target = instr->u.OPER.jumps->labels; target; target = target->tail) {
      G_node label = (G_node) TAB_look(label2node, target->head);
      if (label) {
        G_addEdge(node, label);
      } else {
        EM_error(0, "Cannot find label %s", Temp_labelstring(target->head));
      }
    }
  }

  return graph;
}