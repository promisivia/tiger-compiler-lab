#include <stdio.h>
#include <stdlib.h>
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
#include "regalloc.h"
#include "table.h"
#include "flowgraph.h"

static int count = 0;  // to avoid dead loop

struct RA_result RA_regAlloc(F_frame f, AS_instrList il) {
  count++;
  if (count > 100) exit(0);

  struct RA_result ret;
  G_graph fg;
  struct Live_graph lg;
  struct COL_result cr;

  fg = FG_AssemFlowGraph(il);
  lg = Live_liveness(fg);
  cr = COL_color(lg.graph, lg.workListMoves, lg.moveList, lg.cost);
  if (cr.spills) {  // handle spill
    il = AS_rewriteProgram(f, il, cr.spills);
    return RA_regAlloc(f, il);
  }

  ret.coloring = cr.coloring;
  ret.il = il;

  return ret;
}
