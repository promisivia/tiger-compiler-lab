#ifndef LIVENESS_H
#define LIVENESS_H

typedef struct Live_moveList_* Live_moveList;
struct Live_moveList_ {
  G_node src, dst;
  Live_moveList tail;
};
Live_moveList Live_MoveList(G_node src, G_node dst, Live_moveList tail);


struct Live_graph {
    G_graph graph;
    Live_moveList workListMoves; //有可能合并的传送指令集合
    G_table moveList; //一个node（register）对应的所有move指令
    G_table cost; // spill cost
};
Temp_temp Live_gtemp(G_node n);

struct Live_graph Live_liveness(G_graph flow);

bool Live_contain(Live_moveList ml, G_node src, G_node dst);
Live_moveList Live_add(Live_moveList ml, G_node src, G_node dst);
Live_moveList Live_erase(Live_moveList ml, G_node src, G_node dst);
Live_moveList Live_complement(Live_moveList in, Live_moveList nin);
Live_moveList Live_splice(Live_moveList a, Live_moveList b);
Live_moveList Live_union(Live_moveList a, Live_moveList b);
Live_moveList Live_intersect(Live_moveList a, Live_moveList b);

#endif
