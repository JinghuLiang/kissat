#ifndef _heuristic_h_INCLUDED
#define _heuristic_h_INCLUDED

#include "../main/internal.h"

#include "../../util/data-structures/queue.h"
#include "../../util/data-structures/heap.h"
#include "../../util/inline/inlineheap.h"
#include "../../util/inline/inlinequeue.h"
#include "../../util/misc/literal.h"

#include "vsids.h"
#include "vmtf.h"
#include "chb.h"

#include "mab.h"

struct kissat;
typedef struct heuristic_info heuristic_info;

enum heuristics {VMTF,VSIDS,CHB,LRB};

struct heuristic_info{
    enum heuristics stable_heuristic;
    enum heuristics heuristics[3];

    // vmtf
    links *links;
    queue queue;
    heap* scores;


    unsigned    scores_list_size;
    heap**      scores_list;

    vsids_info vsids_info;
    chb_info chb_info;

    mab_info mab_info;

    uint64_t restart_at_conflict;
};

void kissat_heuristic_init(struct kissat * solver);

void kissat_heuristic_release(struct kissat * solver);

static inline void
add_unassigned_variable_back_to_queue (kissat *solver,heuristic_info* heuristic,unsigned lit) {
  const unsigned idx = IDX (lit);
  if (heuristic->links[idx].stamp > heuristic->queue.search.stamp)
    kissat_update_queue (solver,&heuristic->queue,heuristic->links, idx);
}

static inline void
add_unassigned_variable_back_to_heap (struct kissat* solver,heuristic_info* heuristic,unsigned lit) {
  const unsigned idx = IDX (lit);
  if (!kissat_heap_contains (heuristic->scores, idx))
    kissat_push_heap (solver, heuristic->scores, idx);
}

static inline void
kissat_heuristic_after_assigned (struct kissat* solver, bool conflict){
    if(solver->stable){
        if(solver->heuristic->stable_heuristic == CHB){
            kissat_chb_bump_assigned(solver, conflict);
        }
    }
}

static inline void
kissat_heuristic_analyzed (struct kissat * solver){
    if(solver->stable){
        if(solver->heuristic->stable_heuristic == VSIDS){
            kissat_vsids_bump_analyzed(solver);
            //if(GET_OPTION(mab_enable)&&GET_OPTION(mab_lrb)){
            //    kissat_lrb_bump_analyzed(solver);
            //}
        }else if(solver->heuristic->stable_heuristic == CHB){
            kissat_vsids_bump_analyzed(solver);
            kissat_chb_bump_analyzed(solver);
        }
    }else{
        kissat_vmtf_bump_analyzed(solver);
    }
}

static inline void
kissat_heuristic_refill_all_variables (struct kissat * solver){
    if(solver->heuristic->stable_heuristic == VSIDS){
        return kissat_vsids_refill_all_variables (solver);
    }else if(solver->heuristic->stable_heuristic == CHB){
        return kissat_chb_refill_all_variables (solver);
    }
}

// retuern max
static inline bool
kissat_heuristic_rescale_scores (struct kissat * solver){
    if(GET_OPTION(mab_enable)){
        if(GET_OPTION(mab_vsids))
            kissat_vsids_rescale_scores(solver);
        return false;
    }else{
        if(solver->heuristic->stable_heuristic==VSIDS){
            kissat_vsids_rescale_scores(solver);
            return true;
        }else if(solver->heuristic->stable_heuristic==CHB){
            return true;
        }
        assert(false);
        return true;
    }
}

/*
static inline void
kissat_heuristic_bump_variable (struct kissat * solver, unsigned idx){
    return kissat_vsids_bump_variable(solver, idx);
}
*/

/*
static inline void
kissat_heuristic_bump_score_increment (struct kissat * solver){
    return kissat_vsids_bump_score_increment(solver);
}
*/

#endif
