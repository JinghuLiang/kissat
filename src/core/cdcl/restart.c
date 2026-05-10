#include "restart.h"
#include "assign.h"
#include "../../util/inline/inlineframes.h"
#include "backtrack.h"
#include "decide.h"
#include "../main/internal.h"
#include "../../util/kitten/kimits.h"
#include "../../util/runtime/logging.h"
#include "../../util/io/print.h"
#include "reluctant.h"
#include "../misc/report.h"
#include "../propagate/propsearch.h"
#include <stdbool.h>
#include <stdint.h>

bool kissat_restarting (kissat *solver) {
  assert (solver->unassigned);
  if (!GET_OPTION (restart))
    return false;
  if (solver->level <= 10)
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

static unsigned reuse_stable_trail (kissat *solver) {
  const heap *const scores = SCORES;
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
  const links *const links = solver->links;
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
  if (solver->stable){
    INC (stable_restarts);
  }else
    INC (focused_restarts);
  unsigned level = reuse_trail (solver);
  kissat_extremely_verbose (solver,
                            "restarting after %" PRIu64 " conflicts"
                            " (limit %" PRIu64 ")",
                            CONFLICTS, solver->limits.restart.conflicts);
  LOG ("restarting to level %u", level);
  kissat_backtrack_in_consistent_state (solver, level);
  if (!solver->stable)
    kissat_update_focused_restart_limit (solver);
  REPORT (1, 'R');
  STOP (restart);
}

bool kissat_light_restart_dynamic_condition (kissat *solver) {
    if(!GET_OPTION(lightrestart_condition))
        return true;
    static unsigned begin_round_conflict = 0;
    static unsigned begin_round_decision = 0;
    static unsigned begin_round_propagation = 0;

    unsigned round_conflicts    = solver->statistics.stable_conflicts - begin_round_conflict;
    unsigned round_decisions    = solver->statistics.stable_decisions - begin_round_decision;
    unsigned round_propagations = solver->statistics.stable_propagations - begin_round_propagation;

    begin_round_conflict = solver->statistics.stable_conflicts;
    begin_round_decision = solver->statistics.stable_decisions;
    begin_round_propagation = solver->statistics.stable_propagations;

    double global_dc_ratio = (double)solver->statistics.decisions / solver->statistics.conflicts;
    double local_dc_ratio  = (double)round_decisions / round_conflicts;
    static uint64_t check_round = 0;
    ++check_round;
    UPDATE_AVERAGE(fast_stable_dc, local_dc_ratio );
    UPDATE_AVERAGE(faster_stable_dc, local_dc_ratio );
    UPDATE_AVERAGE(slow_stable_dc, local_dc_ratio );
    static uint64_t round_count = 0;
    ++round_count;
    if(  AVERAGE(fast_stable_dc) < AVERAGE(slow_stable_dc)*0.95 || global_dc_ratio >= 2.5 ){
        return true;
    }else{
        return false;
    }
}


bool kissat_light_restarting (kissat *solver) {
  if (!GET_OPTION (lightrestart))
    return false;
  if (!solver->stable)
    return false;
  if(!solver->stable_light_restart)
      return false;

  uint64_t base = solver->last_light_restart_conflicts;
  if (base < solver->last_full_restart_conflicts)
    base = solver->last_full_restart_conflicts;

  if (CONFLICTS <= base)
    return false;

  const uint64_t delta = CONFLICTS - base;
  if (delta >= 1024){
      bool light_restart_condition = kissat_light_restart_dynamic_condition(solver);
      solver->last_light_restart_conflicts = CONFLICTS;
      if (solver->reluctant.trigger &&
          CONFLICTS >= solver->limits.restart.conflicts){
        return false;
      }else{
        return light_restart_condition;
      }
  }
  return false;
}

static clause *kissat_light_restart_fill_frame (kissat *solver, unsigned lit) {
  if (VALUE (lit))
    return 0;

  solver->level++;
  assert (solver->level != INVALID_LEVEL);
  kissat_push_frame (solver, lit);
  assert (solver->level < SIZE_STACK (solver->frames));
  kissat_assign_decision (solver, lit);

  clause *conflict = kissat_search_propagate (solver);

  return conflict;
}

clause *kissat_light_restart (kissat *solver) {
  assert (solver->stable);
  assert (GET_OPTION (lightrestart));
  assert (!solver->inconsistent);
  assert (solver->watching);

  solver->last_light_restart_conflicts = CONFLICTS;
  if (solver->level <= 10)
    return 0;

  INC (light_restarts);

  unsigneds decision_records;
  INIT_STACK (decision_records);

  frame *begin_f = BEGIN_STACK (solver->frames);
  frame *end_f = END_STACK (solver->frames);
  frame *current_f = begin_f;
  while (++current_f != end_f)
    PUSH_STACK (decision_records, current_f->decision);

  kissat_backtrack_in_consistent_state (solver, 0);
  assert (!solver->level);

  unsigned *begin_dc = BEGIN_STACK (decision_records);
  unsigned *end_dc = END_STACK (decision_records);
  clause *conflict = 0;

  while (begin_dc != end_dc && !conflict) {
    --end_dc;
    conflict = kissat_light_restart_fill_frame (solver, *end_dc);
    assert (solver->propagate <= END_ARRAY (solver->trail));
  }

  RELEASE_STACK (decision_records);
  return conflict;
}
