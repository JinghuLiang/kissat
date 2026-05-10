#ifndef _vsids_h_INCLUDED
#define _vsids_h_INCLUDED

#include "../../util/data-structures/heap.h"

struct kissat;

typedef struct vsids_info vsids_info;
struct vsids_info{
    heap   scores;
    double scinc;
};

void kissat_vsids_init (struct kissat *);
void kissat_vsids_bump_analyzed (struct kissat *);
void kissat_vsids_refill_all_variables (struct kissat *);
void kissat_vsids_rescale_scores (struct kissat *);
void kissat_vsids_bump_variable (struct kissat *, unsigned idx);
void kissat_vsids_bump_score_increment (struct kissat *);

#define VSIDS_MAX_SCORE 1e150

#endif
