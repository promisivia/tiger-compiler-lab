#include <stdio.h>
#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "flowgraph.h"
#include "liveness.h"
#include "table.h"

Live_moveList Live_MoveList(G_node src, G_node dst, Live_moveList tail) {
  Live_moveList lm = (Live_moveList) checked_malloc(sizeof(*lm));
  lm->src = src;
  lm->dst = dst;
  lm->tail = tail;
  return lm;
}

Temp_temp Live_gtemp(G_node n) {
  return (Temp_temp) G_nodeInfo(n);
}

static void enterLiveMap(G_table table, G_node node, Temp_tempList temps) {
  G_enter(table, node, temps);
}

static Temp_tempList lookupLiveMap(G_table table, G_node node) {
  return (Temp_tempList) G_look(table, node);
}

static Live_moveList lookupMoveMap(G_table table, G_node node) {
  return (Live_moveList) G_look(table, node);
}

static double lookupCostMap(G_table table, G_node node) {
  double *p = G_look(table, node);
  if (p) return *p;
  return 0.0;
}

static void setCostMap(G_table table, G_node node, double cost) {
  double *p = G_look(table, node);
  if (!p) {
    p = checked_malloc(sizeof(double));
    G_enter(table, node, p);
  }
  *p = cost;
}

static void increaseCost(G_table table, G_node node) {
  double *p = G_look(table, node);
  if (!p) {
    p = checked_malloc(sizeof(double));
    G_enter(table, node, p);
  }
  *p = *p + 1;
}

static void enterMoveMap(G_table t, G_node src, G_node dst) {
  G_enter(t, src, Live_MoveList(src, dst, lookupMoveMap(t, src)));
  G_enter(t, dst, Live_MoveList(src, dst, lookupMoveMap(t, dst)));
}

static void connect(G_node defNode, G_node outNode) {
  G_addEdge(defNode, outNode);
  G_addEdge(outNode, defNode);
}

G_table liveOut(G_graph flow) {
  // get reversed node list
  G_nodeList nodes = NULL;
  for (G_nodeList nl = G_nodes(flow); nl; nl = nl->tail) {
    nodes = G_NodeList(nl->head, nodes);
  }

  G_table inTable = G_empty(), outTable = G_empty();
  bool stop = FALSE; // stop if noting change in in, out, use, def

  while (!stop) {
    stop = TRUE;
    Temp_tempList in, out, use, def;
    for (G_nodeList tmp_nodes = nodes; tmp_nodes; tmp_nodes = tmp_nodes->tail) {
      G_node node = tmp_nodes->head;
      in = G_look(inTable, node);
      out = G_look(outTable, node);
      use = FG_use(node);
      def = FG_def(node);

      for (; use; use = use->tail) {
        if (!Temp_contain(in, use->head)) {
          in = Temp_TempList(use->head, in);
          stop = FALSE;
        }
      }

      for (; out; out = out->tail) {
        if (!Temp_contain(in, out->head) && !Temp_contain(def, out->head)) {
          in = Temp_TempList(out->head, in);
          stop = FALSE;
        }
      }

      out = G_look(outTable, node);
      G_nodeList succs = G_succ(node);
      for (; succs; succs = succs->tail) {
        G_node succ = succs->head;
        Temp_tempList succ_in = (Temp_tempList) G_look(inTable, succ);
        for (; succ_in; succ_in = succ_in->tail) {
          if (!Temp_contain(out, succ_in->head)) {
            out = Temp_TempList(succ_in->head, out);
            stop = FALSE;
          }
        }
      }
      G_enter(inTable, node, (void *) in);
      G_enter(outTable, node, (void *) out);
    }
  }

  return outTable;
}

static void constructInterfereGraph(struct Live_graph *lg, G_graph flow, G_table liveout) {
  /* four struct of Live_graph*/
  G_graph graph = G_Graph();
  Live_moveList workListMoves = NULL;
  G_table moveList = G_empty(), cost = G_empty();

  G_nodeList instruList = G_nodes(flow); // use several times
  TAB_table tempToNode = TAB_empty();

  /* add node */
  Temp_tempList _uniqueTemps = NULL;
  for (G_nodeList instrus = instruList; instrus; instrus = instrus->tail) {
    G_node instru = instrus->head;
    Temp_tempList nodes = Temp_splice(FG_def(instru), FG_use(instru));
    for (; nodes; nodes = nodes->tail) {
      Temp_temp n = nodes->head;
      if (!Temp_contain(_uniqueTemps, n)) {
        TAB_enter(tempToNode, n, G_Node(graph, n));
        _uniqueTemps = Temp_TempList(n, _uniqueTemps);
      }
    }
  }
  /* add interference edge */
  for (G_nodeList instrus = instruList; instrus; instrus = instrus->tail) {
    G_node instru = instrus->head;

    Temp_tempList def = FG_def(instru);
    Temp_tempList use = FG_use(instru);
    Temp_tempList out = lookupLiveMap(liveout, instru);

    if (FG_isMove(instru)) {
      out = Temp_complement(out, use);
      AS_instr instr = G_nodeInfo(instru);
      G_node src = TAB_look(tempToNode, instr->u.MOVE.src->head);
      G_node dst = TAB_look(tempToNode, instr->u.MOVE.dst->head);
      if (src == dst) break;
      workListMoves = Live_MoveList(src, dst, workListMoves);
      enterMoveMap(moveList, src, dst);
    }

    for (; def; def = def->tail) {
      G_node defNode = (G_node) TAB_look(tempToNode, def->head);
      for (Temp_tempList outs = out; outs; outs = outs->tail) {
        G_node outNode = (G_node) TAB_look(tempToNode, outs->head);
        if (outNode == defNode) continue;
        connect(defNode, outNode);
        increaseCost(cost, defNode);
        increaseCost(cost, outNode);
      }
    }
  }
  /* set cost */
  G_nodeList nl = G_nodes(graph);
  for (; nl; nl = nl->tail) {
    G_node n = nl->head;
    setCostMap(cost, n, lookupCostMap(cost, n) / G_degree(n));
  }

  lg->graph = graph;
  lg->workListMoves = workListMoves;
  lg->moveList = moveList;
  lg->cost = cost;
}

/* 这里相当于实现bulid的伪代码 */
struct Live_graph Live_liveness(G_graph flow) {
  /* Live_graph struct */
  struct Live_graph lg;

  /* Construct liveness graph */
  G_table out = liveOut(flow);

  /* Construct interference graph */
  constructInterfereGraph(&lg, flow, out);

  return lg;
}

bool Live_contain(Live_moveList ml, G_node src, G_node dst) {
  for (; ml; ml = ml->tail) {
    if (ml->src == src && ml->dst == dst)
      return TRUE;
  }
  return FALSE;
}

Live_moveList Live_erase(Live_moveList ml, G_node src, G_node dst) {
  Live_moveList prev = NULL;
  Live_moveList origin = ml;
  for (; ml; ml = ml->tail) {
    if (ml->src == src && ml->dst == dst) {
      if (!prev)
        return ml->tail;
      prev->tail = ml->tail;
      return origin;
    }
    prev = ml;
  }
  return origin;
}

Live_moveList Live_complement(Live_moveList in, Live_moveList notin) {
  Live_moveList res = NULL;
  for (; in; in = in->tail) {
    if (!Live_contain(notin, in->src, in->dst)) {
      res = Live_MoveList(in->src, in->dst, res);
    }
  }
  return res;
}

Live_moveList Live_splice(Live_moveList a, Live_moveList b) {
  if (a == NULL) return b;
  return Live_MoveList(a->src, a->dst, Live_splice(a->tail, b));
}

Live_moveList Live_union(Live_moveList a, Live_moveList b) {
  Live_moveList s = Live_complement(b, a);
  return Live_splice(a, s);
}

Live_moveList Live_intersect(Live_moveList a, Live_moveList b) {
  Live_moveList res = NULL;
  for (; a; a = a->tail) {
    if (Live_contain(b, a->src, a->dst))
      res = Live_MoveList(a->src, a->dst, res);
  }
  return res;
}

Live_moveList Live_add(Live_moveList ml, G_node src, G_node dst) {
  if (Live_contain(ml, src, dst))
    return ml;
  return Live_MoveList(src, dst, ml);
}