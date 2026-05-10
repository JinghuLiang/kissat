#ifndef _chb_h_INCLUDED
#define _chb_h_INCLUDED
#include "../../util/data-structures/heap.h"

struct kissat;

typedef struct chb_info chb_info;
struct chb_info{
    heap chb_score;

    double learning_rate;
    double beta;
    double fix_learn_rate;

    double non_conflict_mul;
};

void kissat_chb_init(struct kissat* solver);

void kissat_chb_bump_assigned (struct kissat *, bool conflict);
void kissat_chb_bump_analyzed (struct kissat *);

void kissat_chb_refill_all_variables(struct kissat *);

#endif
