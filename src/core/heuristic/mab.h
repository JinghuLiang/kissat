#ifndef _mab_H_INCLUDE
#define _mab_H_INCLUDE

#include "../../util/data-structures/stack.h"
#include <stdint.h>
#include <stdbool.h>


struct kissat;
typedef struct mab_info mab_info;
typedef struct heurisitc_mab_info heuristic_mab_info;

struct heurisitc_mab_info{
    double      acc_reward;
    double      score;
    unsigned    selected_round;
    unsigned    selected_round_weight;

};

struct mab_info{
    unsigned heuristic_count;
    unsigned current_heuristic_index;
    unsigned next_heuristic_index;

    uint64_t conflict_when_restart_begin;
    uint64_t decision_when_restart_begin;
    uint64_t weight_of_round;
    uint64_t mab_decisions;
    uint64_t mab_conflicts;

    unsigned ended_round;
    unsigned ended_round_weight;

    double momentum;
    double momentum_windows[10];
    double momentum_avg;

    heuristic_mab_info* heuristic_info;
};

void kissat_mab_reset(struct kissat* );
void kissat_mab_init(struct kissat* );
void kissat_mab_release(struct kissat* );

bool kissat_mab_switching_RR(struct kissat* );

//void kissat_mab_switch_UCB(struct kissat* );
bool kissat_mab_switching_DA(struct kissat* );
bool kissat_mab_switching_DA2(struct kissat* );

void kissat_mab_switch(struct kissat*);

#endif
