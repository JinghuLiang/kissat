#include "restart.h"
#include "../cdcl/assign.h"
#include "../../util/inline/inlineframes.h"
#include "../cdcl/backtrack.h"
#include "../heuristic/decide.h"
#include "../main/internal.h"
#include "../../kitten/kimits.h"
#include "../../util/runtime/logging.h"
#include "../../util/io/print.h"
#include "../cdcl/reluctant.h"
#include "../misc/report.h"
#include "../propagate/propsearch.h"
#include "heuristic.h"
#include "mab.h"
#include <stdlib.h>

static bool restart_trace_enabled(void){
  static int cached = -1;
  if (cached < 0) cached = getenv("MAB_TRACE") ? 1 : 0;
  return cached;
}

bool kissat_restarting (kissat *solver) {
  assert (solver->unassigned);
  if (!GET_OPTION (restart))
    return false;
  if (!solver->level)
      return false;
  if (CONFLICTS < solver->limits.restart.conflicts)
    return false;
  if (solver->stable)
    return kissat_reluctant_triggered (&solver->reluctant);
  const double fast = AVERAGE (fast_glue);
  const double slow = AVERAGE (slow_glue);
  const double margin = (100.0 + GET_OPTION (restartmargin)) / 100.0;
  const double limit = margin * slow;
  kissat_extremely_verbose (solver,
                            "restart glue limit %g = "
                            "%.02f * %g (slow glue) %c %g (fast glue)",
                            limit, margin, slow,
                            (limit > fast    ? '>'
                             : limit == fast ? '='
                                             : '<'),
                            fast);
  return (limit <= fast);
}

void kissat_update_focused_restart_limit (kissat *solver) {
  assert (!solver->stable);
  limits *limits = &solver->limits;
  uint64_t restarts = solver->statistics.restarts;
  uint64_t delta = GET_OPTION (restartint);
  if (restarts)
    delta += kissat_logn (restarts) - 1;
  limits->restart.conflicts = CONFLICTS + delta;
  kissat_extremely_verbose (solver,
                            "focused restart limit at %" PRIu64
                            " after %" PRIu64 " conflicts ",
                            limits->restart.conflicts, delta);
}

// unwarp heuristic operation
static unsigned reuse_stable_trail (kissat *solver) {
  const heap *const scores = solver->heuristic->scores;
  const unsigned next_idx = kissat_next_decision_variable (solver);
  const double limit = kissat_get_heap_score (scores, next_idx);
  unsigned level = solver->level, res = 0;
  while (res < level) {
    frame *f = &FRAME (res + 1);
    const unsigned idx = IDX (f->decision);
    const double score = kissat_get_heap_score (scores, idx);
    if (score <= limit)
      break;
    res++;
  }
  return res;
}

static unsigned reuse_focused_trail (kissat *solver) {
  const links *const links = solver->heuristic->links;
  const unsigned next_idx = kissat_next_decision_variable (solver);
  const unsigned limit = links[next_idx].stamp;
  LOG ("next decision variable stamp %u", limit);
  unsigned level = solver->level, res = 0;
  while (res < level) {
    frame *f = &FRAME (res + 1);
    const unsigned idx = IDX (f->decision);
    const unsigned score = links[idx].stamp;
    if (score <= limit)
      break;
    res++;
  }
  return res;
}

static unsigned reuse_trail (kissat *solver) {
  assert (solver->level);
  assert (!EMPTY_STACK (solver->trail));

  if (!GET_OPTION (restartreusetrail))
    return 0;

  unsigned res;

  if (solver->stable)
    res = reuse_stable_trail (solver);
  else
    res = reuse_focused_trail (solver);

  LOG ("matching trail level %u", res);

  if (res) {
    INC (restarts_reused_trails);
    ADD (restarts_reused_levels, res);
    LOG ("restart reuses trail at decision level %u", res);
  } else
    LOG ("restarts does not reuse the trail");

  return res;
}
void kissat_restart (kissat *solver) {
  START (restart);
  solver->last_full_restart_conflicts = CONFLICTS;
  INC (restarts);
  ADD (restarts_levels, solver->level);
  bool switched = false;
  unsigned old_heur = solver->heuristic->mab_info.current_heuristic_index;
  if (solver->stable){
    INC (stable_restarts);
    if(GET_OPTION(mab_enable)){
        switched = kissat_mab_switching_DA(solver);
    }
  }else{
    INC (focused_restarts);
  }
  unsigned level = switched? 0:reuse_trail (solver);
  if (restart_trace_enabled() && solver->stable && GET_OPTION(mab_enable)) {
    unsigned new_heur = solver->heuristic->mab_info.next_heuristic_index;
    printf("c TRACE_RST pre old=%u new=%u switched=%u level=%u stable_conf=%" PRIu64
           " stable_dec=%" PRIu64 "\n",
           old_heur,
           new_heur,
           switched,
           level,
           solver->statistics.stable_conflicts,
           solver->statistics.stable_decisions);
  }
  kissat_extremely_verbose (solver,
                            "restarting after %" PRIu64 " conflicts"
                            " (limit %" PRIu64 ")",
                            CONFLICTS, solver->limits.restart.conflicts);
  LOG ("restarting to level %u", level);
  kissat_backtrack_in_consistent_state (solver, level);
  if (!solver->stable){
    kissat_update_focused_restart_limit (solver);
  }else{
      if(GET_OPTION(mab_enable)){
          kissat_mab_switch(solver);
          if (restart_trace_enabled()) {
            printf("c TRACE_RST post cur=%u stable_h=%d\n",
                   solver->heuristic->mab_info.current_heuristic_index,
                   solver->heuristic->stable_heuristic);
          }
      }
  }
  REPORT (1, 'R');
  STOP (restart);
}
