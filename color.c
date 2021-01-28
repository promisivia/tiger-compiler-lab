#include <stdio.h>
#include <string.h>

#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "liveness.h"
#include "color.h"
#include "table.h"

#define INFINITE 65535
static int K;
static G_graph interferenceGraph;
static G_table degree, moveList, alias, color, spillCost;
static G_nodeList simplifyWorklist, freezeWorklist, spillWorklist, spilledNodes, coalescedNodes, coloredNodes, selectStack;
static Temp_tempList precolored;
static Live_moveList coalescedMoves, constrainedMoves, frozenMoves, worklistMoves, activeMoves;

int tempCount(Temp_tempList tl) {
  int i = 0;
  for (; tl; tl = tl->tail) i++;
  return i;
}

int nodeCount(G_nodeList nl) {
  int i = 0;
  for (; nl; nl = nl->tail) i++;
  return i;
}

int moveCount(Live_moveList ml) {
  int i = 0;
  for (; ml; ml = ml->tail) i++;
  return i;
}

int nodeNum(G_node n) {
  return Temp_getTempnum(Live_gtemp(n));
}

char *printNodes(G_nodeList nl) {
  char *buf = checked_malloc(1024);
  *buf = '\0';
  for (; nl; nl = nl->tail) {
    sprintf(buf, "%s %d,", buf, nodeNum(nl->head));
  }
  return buf;
}

void setAlias(G_node n, G_node a) {
  G_enter(alias, n, a);
}

G_node getAlias(G_node n) {
  if (G_contain(coalescedNodes, n)) {
    return getAlias((G_node) G_look(alias, n));
  }
  return n;
}

void setDegree(G_node n, int d) {
  G_enter(degree, n, (void *) d);
}

int getDegree(G_node n) {
  return (int) G_look(degree, n);
}

double getSpillCost(G_node n) {
  return *(double *) G_look(spillCost, n);
}

void assignColor(G_node n, Temp_temp t) {
  G_enter(color, n, t);
}

Temp_temp getColor(G_node n) {
  return (Temp_temp) G_look(color, n);
}

bool isPrecolored(G_node n) {
  return Temp_contain(precolored, Live_gtemp(n));
}

void enterMoveList(G_node n, Live_moveList ml) {
  G_enter(moveList, n, ml);
}

Live_moveList getMoveList(G_node n) {
  return (Live_moveList) G_look(moveList, n);
}

void pushSelectStack(G_node n) {
  selectStack = G_add(selectStack, n);
}

G_node popSelectStack() {
  assert(!(selectStack == NULL));
  G_node n = selectStack->head;
  selectStack = selectStack->tail;
  return n;
}

void addEdge(G_node u, G_node v) {
  if (G_goesTo(u, v) || G_goesTo(v, u)) {
    return;
  }
  G_addEdge(u, v);
  G_addEdge(v, u);

  if (!isPrecolored(u))
    setDegree(u, getDegree(u) + 1);

  if (!isPrecolored(v))
    setDegree(v, getDegree(v) + 1);
}

Live_moveList nodeMoves(G_node n) {
  return Live_intersect(getMoveList(n), Live_union(activeMoves, worklistMoves));
}

bool isMoveRelated(G_node n) {
  return nodeMoves(n) != NULL;
}

G_nodeList adjacent(G_node n) {
  return G_complement(G_succ(n), G_union(selectStack, coalescedNodes));
}

void init(G_graph ig, Live_moveList moves, G_table temp_to_moves, G_table cost) {
  Temp_tempList regs = F_registers();
  K = F_regNum;
  simplifyWorklist = NULL;
  freezeWorklist = NULL;
  spillWorklist = NULL;
  spilledNodes = NULL;
  coalescedNodes = NULL;
  coloredNodes = NULL;
  selectStack = NULL;

  precolored = regs;
  coalescedMoves = NULL;
  constrainedMoves = NULL;
  frozenMoves = NULL;
  worklistMoves = moves;
  activeMoves = NULL;

  degree = G_empty();
  moveList = temp_to_moves;
  alias = G_empty();
  color = G_empty();
  spillCost = cost;

  interferenceGraph = ig;

  for (G_nodeList nodes = G_nodes(ig); nodes; nodes = nodes->tail) {
    G_node n = nodes->head;
    if (!isPrecolored(n)) {
      setDegree(n, G_degree(n) / 2);
    } else {
      setDegree(n, INFINITE); // to avoid operate on pre-colored degree
    }
  }
}

void makeWorkList() {
  G_nodeList nodes = G_nodes(interferenceGraph);
  for (; nodes; nodes = nodes->tail) {
    G_node n = nodes->head;
    if (isPrecolored(n)) continue;
    if (getDegree(n) >= K) {
      spillWorklist = G_add(spillWorklist, n);
    } else if (isMoveRelated(n)) {
      freezeWorklist = G_add(freezeWorklist, n);
    } else {
      simplifyWorklist = G_add(simplifyWorklist, n);
    }
  }
}

void enableMoves(G_nodeList nl) {
  for (; nl; nl = nl->tail) {
    G_node n = nl->head;
    Live_moveList ml = nodeMoves(n);
    for (; ml; ml = ml->tail) {
      G_node src = ml->src, dst = ml->dst;
      if (Live_contain(activeMoves,  src, dst)) {
        activeMoves = Live_erase(activeMoves, src, dst);
        worklistMoves = Live_add(worklistMoves, src, dst);
      }
    }
  }
}

void decrementDegree(G_node m) {
  int d = getDegree(m);
  setDegree(m, d - 1);
  if (d == K && !isPrecolored(m)) {
    enableMoves(G_add(adjacent(m), m));
    spillWorklist = G_remove(spillWorklist, m);
    if (isMoveRelated(m)) {
      freezeWorklist = G_add(freezeWorklist, m);
    } else {
      simplifyWorklist = G_add(simplifyWorklist, m);
    }
  }
}

void simplify() {
  G_node n = simplifyWorklist->head;
  simplifyWorklist = simplifyWorklist->tail;
  pushSelectStack(n);
  G_nodeList nl = adjacent(n);
  for (; nl; nl = nl->tail) {
    G_node m = nl->head;
    decrementDegree(m);
  }
}

void addWorkList(G_node u) {
  if (!isPrecolored(u) && !isMoveRelated(u) && getDegree(u) < K) {
    freezeWorklist = G_remove(freezeWorklist, u);
    simplifyWorklist = G_add(simplifyWorklist, u);
  }
}

bool OK(G_node t, G_node v) {
  return getDegree(t) < K || isPrecolored(t) || G_goesTo(t, v);
}

static bool checkAdjacent(G_node v, G_node u) {
  G_nodeList adjs = adjacent(v);
  for (; adjs; adjs = adjs->tail) {
    G_node t = adjs->head;
    if (!OK(t, u))
      return FALSE;
  }
  return TRUE;
}

bool conservative(G_nodeList nl) {
  int k = 0;
  for (; nl; nl = nl->tail) {
    if (getDegree(nl->head) >= K)
      k = k + 1;
  }
  return k < K;
}

void combine(G_node u, G_node v) {
  if (G_contain(freezeWorklist, v))
    freezeWorklist = G_remove(freezeWorklist, v);
  else
    spillWorklist = G_remove(spillWorklist, v);

  coalescedNodes = G_add(coalescedNodes, v);
  setAlias(v, u);
  enterMoveList(u, Live_union(getMoveList(u), getMoveList(v)));
  enableMoves(G_NodeList(v, NULL));

  G_nodeList nl = adjacent(v);
  for (; nl; nl = nl->tail) {
    addEdge(nl->head, u);
    decrementDegree(nl->head);
  }

  if (getDegree(u) >= K && G_contain(freezeWorklist, u)) {
    freezeWorklist = G_remove(freezeWorklist, u);
    spillWorklist = G_add(spillWorklist, u);
  }
}

void coalesce() {
  // let m = copy(x, y) in worklistMoves
  G_node src = worklistMoves->src, dst = worklistMoves->dst;
  G_node x = getAlias(src);
  G_node y = getAlias(dst);
  G_node u, v;
  if (isPrecolored(y)) {
    u = y;
    v = x;
  } else {
    u = x;
    v = y;
  }
  worklistMoves = Live_erase(worklistMoves, src, dst);
  if (u == v) {
    coalescedMoves = Live_add(coalescedMoves, src, dst);
    addWorkList(u);
  } else if (isPrecolored(v) || G_goesTo(u, v)) {
    constrainedMoves = Live_add(constrainedMoves, src, dst);
    addWorkList(u);
    addWorkList(v);
  } else if (isPrecolored(u) && checkAdjacent(v, u)
  || !isPrecolored(u) && conservative(G_union(adjacent(u), adjacent(v)))) {
    coalescedMoves = Live_add(coalescedMoves, src, dst);
    combine(u, v);
    addWorkList(u);
  } else {
    activeMoves = Live_add(activeMoves, src, dst);
  }
}

void freezeMoves(G_node u) {
  for (Live_moveList ml = nodeMoves(u); ml; ml = ml->tail) {
    G_node src = ml->src, dst = ml->dst;
    G_node x = src;
    G_node y = dst;
    G_node v;
    if (getAlias(y) == getAlias(u))
      v = getAlias(x);
    else
      v = getAlias(y);

    activeMoves = Live_erase(activeMoves, src, dst);
    frozenMoves = Live_add(frozenMoves, src, dst);
    if (!nodeMoves(v) && getDegree(v) < K) {
      freezeWorklist = G_remove(freezeWorklist, v);
      simplifyWorklist = G_add(simplifyWorklist, v);
    }
  }
}

void freeze() {
  G_node u = freezeWorklist->head;
  freezeWorklist = G_remove(freezeWorklist, u);
  simplifyWorklist = G_add(simplifyWorklist, u);
  freezeMoves(u);
}

void selectSpill() {
  /* use heuristic algorithm to find spill node */
  G_node m = spillWorklist->head;
  double min = getSpillCost(m);
  for(G_nodeList l = spillWorklist->tail; l; l = l->tail){
    int c = getSpillCost(l->head);
    if (c < min){
      m = l->head;
      min = c;
    }
  }
  /* spill the selected node */
  spillWorklist = G_remove(spillWorklist, m);
  simplifyWorklist = G_add(simplifyWorklist, m);
  freezeMoves(m);
}

void assignColors() {
  for (G_nodeList nl = G_nodes(interferenceGraph); nl; nl = nl->tail) {
    G_node n = nl->head;
    if (isPrecolored(n)) {
      assignColor(n, Live_gtemp(n));
      coloredNodes = G_add(coloredNodes, n);
    }
  }
  while (selectStack) {
    G_node n = popSelectStack();
    if (G_contain(coloredNodes, n)) continue;
    Temp_tempList okColors = precolored;

    for (G_nodeList adj = G_adj(n); adj; adj = adj->tail) {
      G_node w = adj->head;
      if (G_contain(coloredNodes, getAlias(w)) || isPrecolored(getAlias(w))) {
        okColors = Temp_complement(okColors, Temp_TempList(getColor(getAlias(w)), NULL));
      }
    }
    if (!okColors) {
      spilledNodes = G_add(spilledNodes, n);
    } else {
      coloredNodes = G_add(coloredNodes, n);
      assignColor(n, okColors->head);
    }
  }
  G_nodeList nl = coalescedNodes;
  for (; nl; nl = nl->tail) {
    G_node n = nl->head;
    assignColor(n, getColor(getAlias(n)));
  }
}

/*
 * same as the Main func in register allocation
 * LivenessAnalysis and build work already down in liveness model
 */
struct COL_result
COL_color(G_graph ig, Live_moveList workListMoves, G_table moveList, G_table cost) {
  struct COL_result ret;
  Temp_map initial = F_tempMap();
  init(ig, workListMoves, moveList, cost);
  makeWorkList();

  do {
    if (simplifyWorklist) simplify();
    else if (worklistMoves) coalesce();
    else if (freezeWorklist) freeze();
    else if (spillWorklist) selectSpill();
  } while (simplifyWorklist || worklistMoves || freezeWorklist || spillWorklist);

  assignColors();

  // from G_table to Temp_map
  if (!spilledNodes) {
    Temp_map coloring = Temp_empty();
    G_nodeList nl = G_nodes(interferenceGraph);
    for (; nl; nl = nl->tail) {
      G_node n = nl->head;
      Temp_temp color = getColor(n);
      if (color) {
        Temp_enter(coloring, Live_gtemp(n), Temp_look(initial, color));
      }
    }
    ret.coloring = Temp_layerMap(coloring, initial);
  }

  // from G_nodeList to Temp_tempList
  Temp_tempList spills = NULL;
  for (G_nodeList nl = spilledNodes; nl; nl = nl->tail) {
    G_node n = nl->head;
    spills = Temp_TempList(Live_gtemp(n), spills);
  }
  ret.spills = spills;

  return ret;
}
