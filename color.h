/*
 * color.h - Data structures and function prototypes for coloring algorithm
 *             to determine register allocation.
 */

#ifndef COLOR_H
#define COLOR_H

struct COL_result {
  Temp_map coloring;
  Temp_tempList spills;
};

struct COL_result COL_color(G_graph ig, Live_moveList moves, G_table temp_to_moves, G_table cost);

#endif
