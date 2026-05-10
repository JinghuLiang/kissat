#include "chb.h"
#include "heuristic.h"

void kissat_chb_init(struct kissat * solver){
    chb_info* chb = &solver->heuristic->chb_info;

    chb->learning_rate   = 0.4;
    chb->beta = 1.0 - chb->learning_rate;
    chb->fix_learn_rate  = 0.06;
    chb->non_conflict_mul = 0.9;
}

void kissat_chb_bump_assigned(struct kissat * solver, bool conflict){
    chb_info* chb = &solver->heuristic->chb_info;
    assigned* assigneds = solver->assigned;
    int i = SIZE_STACK(solver->trail) - 1;
    unsigned lit = i >= 0 ? PEEK_STACK(solver->trail, i) : 0;
    while (i >= 0 && LEVEL(lit) == solver->level) {
        lit = PEEK_STACK(solver->trail, i);
        unsigned idx = IDX(lit);
        assigned* a = assigneds + idx;

        uint64_t last_conflict = a->last_conflict_index;
        double multiplier = conflict ? 1.0 : chb->non_conflict_mul;
        double reward = multiplier / (CONFLICTS - last_conflict + 1);
        double old_score = kissat_get_heap_score(&chb->chb_score, idx);
        double new_socre = chb->learning_rate * reward + (1.0 - chb->learning_rate) * old_score;
        kissat_update_heap(solver, &chb->chb_score, idx, new_socre);
        i--;
    }

    if(conflict){
        if(chb->learning_rate > chb->fix_learn_rate){
            chb->learning_rate -= 1e-6;
            chb->beta = 1.0 - chb->learning_rate;
        }
    }
}


void kissat_chb_bump_analyzed (struct kissat *solver ){
    const size_t bumped = SIZE_STACK (solver->analyzed);
    for (all_stack (unsigned, idx, solver->analyzed)){
        assigned* assigned = solver->assigned + idx;
        if (ACTIVE(idx)){
            assigned->last_conflict_index = CONFLICTS;
        }
    }
    ADD (literals_bumped, bumped);
    STOP (bump);
}

void kissat_chb_refill_all_variables (kissat *solver) {
  heap *scores = &solver->heuristic->chb_info.chb_score;
  for (all_variables (idx))
    if (ACTIVE (idx) && !kissat_heap_contains (scores, idx))
      kissat_push_heap (solver, scores, idx);
}
