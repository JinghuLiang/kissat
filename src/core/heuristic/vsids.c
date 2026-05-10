#include "vsids.h"
#include "../../util/inline/inlineheap.h"
#include "../main/internal.h"
#include "../../util/runtime/logging.h"
#include "../../util/io/print.h"
#include "heuristic.h"


#define RADIX_SORT_BUMP_LIMIT 32

void kissat_vsids_init (struct kissat * solver){
    solver->heuristic->vsids_info.scinc = 1.0;
    return;
}

void kissat_vsids_rescale_scores (kissat *solver) {
  INC (rescaled);
  heap *scores = &solver->heuristic->vsids_info.scores;
  const double max_score = kissat_max_score_on_heap (scores);
  kissat_phase (solver, "rescale", GET (rescaled),
                "maximum score %g increment %g", max_score, solver->heuristic->vsids_info.scinc);
  const double rescale = MAX (max_score, solver->heuristic->vsids_info.scinc);
  assert (rescale > 0);
  const double factor = 1.0 / rescale;
  kissat_rescale_heap (solver, scores, factor);
  solver->heuristic->vsids_info.scinc *= factor;
  kissat_phase (solver, "rescale", GET (rescaled), "rescaled by factor %g",
                factor);
}

void kissat_vsids_bump_score_increment (kissat *solver) {
  const double old_scinc = solver->heuristic->vsids_info.scinc;
  const double decay = GET_OPTION (decay) * 1e-3;
  assert (0 <= decay), assert (decay <= 0.5);
  const double factor = 1.0 / (1.0 - decay);
  const double new_scinc = old_scinc * factor;
  LOG ("new score increment %g = %g * %g", new_scinc, factor, old_scinc);
  solver->heuristic->vsids_info.scinc = new_scinc;
  if (new_scinc > VSIDS_MAX_SCORE)
    kissat_vsids_rescale_scores (solver);
}

static inline void bump_analyzed_variable_score (kissat *solver,
                                                 unsigned idx) {
  heap *scores = &solver->heuristic->vsids_info.scores;
  const double old_score = kissat_get_heap_score (scores, idx);
  const double inc = solver->heuristic->vsids_info.scinc;
  const double new_score = old_score + inc;
  LOG ("new score[%u] = %g = %g + %g", idx, new_score, old_score, inc);
  kissat_update_heap (solver, scores, idx, new_score);
  if (new_score > VSIDS_MAX_SCORE)
    kissat_vsids_rescale_scores (solver);
}

void kissat_vsids_bump_variable (kissat *solver, unsigned idx) {
  bump_analyzed_variable_score (solver, idx);
}

static void bump_analyzed_variable_scores (kissat *solver) {
  flags *flags = solver->flags;

  for (all_stack (unsigned, idx, solver->analyzed))
    if (flags[idx].active)
      bump_analyzed_variable_score (solver, idx);

  kissat_vsids_bump_score_increment (solver);
}

void kissat_vsids_bump_analyzed (kissat *solver) {
  START (bump);
  const size_t bumped = SIZE_STACK (solver->analyzed);
  bump_analyzed_variable_scores (solver);
  ADD (literals_bumped, bumped);
  STOP (bump);
}

void kissat_vsids_refill_all_variables (kissat *solver) {
  heap *scores = &solver->heuristic->vsids_info.scores;
  for (all_variables (idx))
    if (ACTIVE (idx) && !kissat_heap_contains (scores, idx))
      kissat_push_heap (solver, scores, idx);
}
