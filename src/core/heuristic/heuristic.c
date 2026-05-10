#include "../main/internal.h"
#include "vsids.h"
#include "heuristic.h"

#define DEALLOC_GENERIC(NAME, ELEMENTS_PER_BLOCK) \
  do { \
    const size_t block_size = ELEMENTS_PER_BLOCK * sizeof *solver->NAME; \
    kissat_dealloc (solver, solver->NAME, solver->size, block_size); \
    solver->NAME = 0; \
  } while (0)
#define DEALLOC_VARIABLE_INDEXED(NAME) DEALLOC_GENERIC (NAME, 1)

void kissat_heuristic_init(struct kissat *solver){
   // init vmtf
    kissat_init_queue(solver);

    if(!GET_OPTION(mab_enable)){
        solver->heuristic->stable_heuristic = VSIDS;//VSIDS;
        solver->heuristic->scores = &solver->heuristic->vsids_info.scores;
        // init vsidsa
        //kissat_vsids_init(solver);
        //kissat_chb_init(solver);
        solver->heuristic->scores_list_size = 1;
        solver->heuristic->heuristics[0] = VSIDS;
        solver->heuristic->scores_list = kissat_malloc(solver,
            sizeof(void*)*solver->heuristic->scores_list_size);
        solver->heuristic->scores_list[0] = &solver->heuristic->vsids_info.scores;
    }else{
        solver->heuristic->stable_heuristic = INT_MAX;//VSIDS;
        if(GET_OPTION(mab_vsids)){
            kissat_vsids_init(solver);
            ++solver->heuristic->scores_list_size;
            if(solver->heuristic->stable_heuristic == INT_MAX ){
                solver->heuristic->stable_heuristic = VSIDS;
                solver->heuristic->scores = &solver->heuristic->vsids_info.scores;
            }
        }
        if(GET_OPTION(mab_chb)){
            kissat_chb_init(solver);
            ++solver->heuristic->scores_list_size;
            if(solver->heuristic->stable_heuristic == INT_MAX ){
                solver->heuristic->stable_heuristic = CHB;
                solver->heuristic->scores = &solver->heuristic->chb_info.chb_score;
            }
        }

        solver->heuristic->scores_list = kissat_malloc(solver,
            sizeof(void*)*solver->heuristic->scores_list_size);
        unsigned current_index = 0;
        if(GET_OPTION(mab_vsids)){
            solver->heuristic->heuristics[current_index] = VSIDS;
            solver->heuristic->scores_list[current_index++] = &solver->heuristic->vsids_info.scores;
        }if(GET_OPTION(mab_chb)){
            solver->heuristic->heuristics[current_index] = CHB;
            solver->heuristic->scores_list[current_index++] = &solver->heuristic->chb_info.chb_score;
        }
    }
    return;
}

void kissat_heuristic_release(struct kissat *solver){
    for(unsigned i=0;i<solver->heuristic->scores_list_size;++i){
        kissat_release_heap (solver, solver->heuristic->scores_list[i]);
    }
    kissat_dealloc(solver,solver->heuristic->scores_list,solver->heuristic->scores_list_size,sizeof(void*));
    DEALLOC_VARIABLE_INDEXED (heuristic->links);
}
