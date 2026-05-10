#include "mab.h"
#include "heuristic.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static bool mab_trace_enabled(void){
    static int cached = -1;
    if (cached < 0) cached = getenv("MAB_TRACE") ? 1 : 0;
    return cached;
}

void kissat_mab_reset(kissat* solver){
    mab_info * info = &solver->heuristic->mab_info;
    for(unsigned i =0 ;i<info->heuristic_count;++i){
        info->heuristic_info[i].acc_reward = 0.0;
        info->heuristic_info[i].score = 0.0;
        info->heuristic_info[i].selected_round = 0;
        info->heuristic_info[i].selected_round_weight = 0;
    }

    info->current_heuristic_index = 0;
    info->next_heuristic_index = 0;
    if (info->heuristic_count > 0) {
        info->heuristic_info[0].selected_round = 1;
    }

    info->conflict_when_restart_begin = 0;
    info->decision_when_restart_begin = 0;
    info->weight_of_round = 1;
    info->mab_decisions = 0;
    info->mab_conflicts = 0;

    info->ended_round = 0;
    info->ended_round_weight = 0;

    info->momentum = 1.0;
    info->momentum_avg = 0;
}

void kissat_mab_init(kissat* solver){
    mab_info * info = &solver->heuristic->mab_info;
    info->heuristic_count = solver->heuristic->scores_list_size;
    if(GET(mab_restarts)==0){
        info->heuristic_info = kissat_malloc(solver,
                sizeof(heuristic_mab_info) * info->heuristic_count);
    }
    kissat_mab_reset(solver);
}

void kissat_mab_release(kissat* solver){
    kissat_dealloc(solver, solver->heuristic->mab_info.heuristic_info,
        sizeof(heuristic_mab_info), solver->heuristic->mab_info.heuristic_count );
}

bool kissat_mab_switching_RR(kissat* solver){
    mab_info* info = &solver->heuristic->mab_info;

    info->next_heuristic_index =
        (info->current_heuristic_index + 1) % (info->heuristic_count);
    return true;
}

void mab_update_DA(kissat* solver){
    mab_info* mab_info = &solver->heuristic->mab_info;
    unsigned heuristic = mab_info->current_heuristic_index;
    heuristic_mab_info* heu_info = &mab_info->heuristic_info[heuristic];

    uint64_t round_conflict =
        mab_info->mab_conflicts - mab_info->conflict_when_restart_begin;
    uint64_t round_decision =
        mab_info->mab_decisions - mab_info->decision_when_restart_begin;

    double round_score =  log2((double)round_decision/mab_info->weight_of_round)
                        / log2((double)round_conflict/mab_info->weight_of_round);

    double round_score_pen = round_score / (log2(mab_info->weight_of_round)+1);

    if (mab_trace_enabled()) {
        printf("c TRACE_MAB_UPD round=%u cur=%u weight=%" PRIu64
               " round_dec=%" PRIu64 " raw_mab_dec=%" PRIu64 " round_conf=%" PRIu64
               " reward=%g reward_w=%g mom=%g mom_avg=%g\n",
               mab_info->ended_round + 1,
               heuristic,
               mab_info->weight_of_round,
               round_decision,
               mab_info->mab_decisions,
               round_conflict,
               round_score,
               round_score_pen,
               mab_info->momentum,
               mab_info->momentum_avg);
    }

    // printf("before acc %lf weight %u \n", heu_info->acc_reward, heu_info->selected_round_weight);
    // printf("before acc cd_t %lu %lu \n", GET(stable_conflicts), GET(stable_decisions));
    // printf("before acc cd %lu %lu \n", round_decision, round_conflict);
    // printf("before acc r_p score %lf %lf \n", round_score, round_score_pen);
    heu_info->acc_reward += round_score_pen * mab_info->weight_of_round;
    if (heu_info->selected_round_weight > 0) {
        heu_info->score = heu_info->acc_reward / heu_info->selected_round_weight;
    }

    mab_info->ended_round += 1;

    //if(mab_info->ended_round > 10){
    unsigned windods_p = (mab_info->ended_round-1) % 10;
    mab_info->momentum_avg += ( (round_score - mab_info->momentum_windows[windods_p]) / 10 );
    mab_info->momentum_windows[windods_p] = round_score;
    //}else{
    //    unsigned windods_p = mab_info->ended_round-1;
    //    mab_info->momentum_windows[windods_p] = round_score;
    //    mab_info->momentum_avg += ((mab_info->momentum_avg * windods_p) + round_score)/ mab_info->ended_round;
    //}
    if(round_score > mab_info->momentum_avg){
        mab_info->momentum *= 1.1;
    }else{
        mab_info->momentum *= 0.9;
    }

    mab_info->weight_of_round = solver->reluctant.v;
    mab_info->conflict_when_restart_begin = mab_info->mab_conflicts;
    mab_info->decision_when_restart_begin = mab_info->mab_decisions;
}

unsigned mab_decide_DA(kissat* solver){
    mab_info* mab_info = &solver->heuristic->mab_info;
    unsigned stable_restarts = 0;
    for (unsigned i = 0; i < mab_info->heuristic_count; ++i) {
        stable_restarts += mab_info->heuristic_info[i].selected_round;
    }
    if(stable_restarts < mab_info->heuristic_count){
        return (mab_info->current_heuristic_index + 1) % mab_info->heuristic_count;
    }else{
        // adative exp
        double adative_exp = 4;
        double avg_score = 0;
        for(unsigned i = 0; i < mab_info->heuristic_count; ++i){
            heuristic_mab_info* heu_info = &mab_info->heuristic_info[i];
            avg_score += heu_info->score;
        }
        avg_score /= mab_info->heuristic_count;
        adative_exp *= avg_score;
        adative_exp *= mab_info->momentum;
        adative_exp /= (log2(mab_info->weight_of_round) + 1);

        double max_ucb_value = 0;
        unsigned max_ucb_heuristic = 0;
        for(unsigned i = 0; i < mab_info->heuristic_count; ++i){
            heuristic_mab_info* heu_info = &mab_info->heuristic_info[i];
            double ucb_duration_aware =
                heu_info->score +
                sqrt(  adative_exp * log(mab_info->ended_round_weight + 1) / heu_info->selected_round_weight );
            if (i == 0) {
                max_ucb_value = ucb_duration_aware;
            } else if (ucb_duration_aware > max_ucb_value) {
                max_ucb_value = ucb_duration_aware;
                max_ucb_heuristic = i;
            }
        }
        return max_ucb_heuristic;
    }
    assert(false);
    return 0;
}


bool kissat_mab_switching_DA(kissat* solver){
    mab_info* info = &solver->heuristic->mab_info;

    INC(mab_restarts);
    // update (credits CURRENT arm's reward accumulator)
    mab_update_DA(solver);
    // decide (UCB or round-robin)
    info->next_heuristic_index = mab_decide_DA(solver);
    // credit CHOSEN arm's selection count AFTER UCB, using NEW weight
    {
        unsigned chosen = info->next_heuristic_index;
        heuristic_mab_info* chosen_info = &info->heuristic_info[chosen];
        chosen_info->selected_round += 1;
        chosen_info->selected_round_weight += info->weight_of_round;
        chosen_info->score = chosen_info->acc_reward / chosen_info->selected_round_weight;
        info->ended_round_weight += info->weight_of_round;

        if (mab_trace_enabled()) {
            heuristic_mab_info* h0 = &info->heuristic_info[0];
            heuristic_mab_info* h1 = &info->heuristic_info[(info->heuristic_count > 1) ? 1 : 0];
            printf("c TRACE_MAB_DEC round=%u cur=%u next=%u switched=%u ended_w=%u"
                   " h0_sel=%u h0_selw=%u h0_score=%g"
                   " h1_sel=%u h1_selw=%u h1_score=%g\n",
                   info->ended_round,
                   info->current_heuristic_index,
                   info->next_heuristic_index,
                   info->next_heuristic_index != info->current_heuristic_index,
                   info->ended_round_weight,
                   h0->selected_round,
                   h0->selected_round_weight,
                   h0->score,
                   h1->selected_round,
                   h1->selected_round_weight,
                   h1->score);
        }
    }
    // switch
    return (info->next_heuristic_index != info->current_heuristic_index);
}


void mab_update_DA2(kissat* solver){
    mab_info* mab_info = &solver->heuristic->mab_info;
    unsigned heuristic = mab_info->current_heuristic_index;
    heuristic_mab_info* heu_info = &mab_info->heuristic_info[heuristic];

    uint64_t round_conflict =
        solver->statistics.stable_conflicts - mab_info->conflict_when_restart_begin;
    uint64_t round_decision =
        solver->statistics.stable_decisions - mab_info->decision_when_restart_begin;

    double round_score =  log2((double)round_decision/mab_info->weight_of_round)
                        / log2((double)round_conflict/mab_info->weight_of_round);

    //double round_score_pen = round_score / (log2(mab_info->weight_of_round)+1);

    heu_info->selected_round += 1;
    heu_info->selected_round_weight += mab_info->weight_of_round;
    heu_info->acc_reward += round_score * mab_info->weight_of_round;
    heu_info->score = heu_info->acc_reward / heu_info->selected_round_weight;

    mab_info->ended_round += 1;
    mab_info->ended_round_weight += mab_info->weight_of_round;

    unsigned windods_p = (mab_info->ended_round-1) % 10;
    mab_info->momentum_avg += ( (round_score - mab_info->momentum_windows[windods_p]) / 10 );
    mab_info->momentum_windows[windods_p] = round_score;
    if(round_score > mab_info->momentum_avg){
        mab_info->momentum *= 1.1;
    }else if(round_score < mab_info->momentum_avg){
        mab_info->momentum *= 0.9;
    }

    mab_info->weight_of_round = kissat_log2_ceiling_of_uint64(solver->reluctant.v) + 1;
    mab_info->conflict_when_restart_begin = solver->statistics.stable_conflicts;
    mab_info->decision_when_restart_begin = solver->statistics.stable_decisions;
}

unsigned mab_decide_DA2(kissat* solver){
    mab_info* mab_info = &solver->heuristic->mab_info;
    if(mab_info->ended_round < mab_info->heuristic_count){
        return (mab_info->ended_round % mab_info->heuristic_count);
    }else{
        // adative exp
        double adative_exp = 4;
        double avg_score = 0;
        for(unsigned i = 0; i < mab_info->heuristic_count; ++i){
            heuristic_mab_info* heu_info = &mab_info->heuristic_info[i];
            avg_score += heu_info->score;
        }
        avg_score /= mab_info->heuristic_count;
        adative_exp *= avg_score;
        adative_exp *= mab_info->momentum;
        adative_exp /= mab_info->weight_of_round;

        double max_ucb_value = 0;
        unsigned max_ucb_heuristic = UINT32_MAX;
        for(unsigned i = 0; i < mab_info->heuristic_count; ++i){
            heuristic_mab_info* heu_info = &mab_info->heuristic_info[i];
            double ucb_duration_aware =
                heu_info->score +
                sqrt(  adative_exp * log(mab_info->ended_round_weight) / heu_info->selected_round_weight );
            if(ucb_duration_aware >= max_ucb_value){
                max_ucb_value = ucb_duration_aware;
                max_ucb_heuristic = i;
            }
        }
        assert(max_ucb_heuristic != UINT32_MAX);
        return max_ucb_heuristic;
    }
    assert(false);
    return 0;
}

bool kissat_mab_switching_DA2(kissat* solver){
    mab_info* info = &solver->heuristic->mab_info;
    bool switching;
    INC(mab_restarts);
    // update
    mab_update_DA2(solver);
    // decide
    info->next_heuristic_index = mab_decide_DA2(solver);
    // switch
    return info->next_heuristic_index != info->current_heuristic_index;

}


void kissat_mab_switch(kissat* solver){
    mab_info* info = &solver->heuristic->mab_info;
    bool switching = info->current_heuristic_index != info->next_heuristic_index;
    if(switching){
        INC(mab_valid_switch);
        // change tag
        solver->heuristic->stable_heuristic =
            solver->heuristic->heuristics[info->next_heuristic_index];
        // change score
        solver->heuristic->scores =
            solver->heuristic->scores_list[info->next_heuristic_index];
        // refill
        kissat_heuristic_refill_all_variables(solver);
        // update
        info->current_heuristic_index = info->next_heuristic_index;
    }

    enum heuristics heuristic_name =
        solver->heuristic->heuristics[info->next_heuristic_index];
    if(heuristic_name == VSIDS){
        INC(mab_vsids);
    }else if(heuristic_name == CHB) {
        INC(mab_chb);
    }else if(heuristic_name == LRB) {
        INC(mab_lrb);
    }
}
