#include "../../util/inline/inlinequeue.h"
#include "../main/internal.h"
#include "../../util/runtime/logging.h"
#include "../../util/data-structures/rank.h"
#include "../../util/misc/sort.h"
#include "heuristic.h"

#define RANK(A) ((A).rank)
#define SMALLER(A, B) (RANK (A) < RANK (B))

#define RADIX_SORT_BUMP_LIMIT 32

static void sort_bump (kissat *solver) {
  const size_t size = SIZE_STACK (solver->analyzed);
  if (size < RADIX_SORT_BUMP_LIMIT) {
    LOG ("quick sorting %zu analyzed variables", size);
    SORT_STACK (datarank, solver->ranks, SMALLER);
  } else {
    LOG ("radix sorting %zu analyzed variables", size);
    RADIX_STACK (datarank, unsigned, solver->ranks, RANK);
  }
}

static void move_analyzed_variables_to_front_of_queue (kissat *solver) {
  assert (EMPTY_STACK (solver->ranks));
  const links *const links = solver->heuristic->links;
  for (all_stack (unsigned, idx, solver->analyzed)) {
    // clang-format off
    const datarank rank = { .data = idx, .rank = links[idx].stamp };
    // clang-format on
    PUSH_STACK (solver->ranks, rank);
  }

  sort_bump (solver);

  flags *flags = solver->flags;
  unsigned idx;

  for (all_stack (datarank, rank, solver->ranks))
    if (flags[idx = rank.data].active)
      kissat_move_to_front (solver, &solver->heuristic->queue,solver->heuristic->links, idx);

  CLEAR_STACK (solver->ranks);
}

void kissat_vmtf_bump_analyzed (kissat *solver) {
  START (bump);
  const size_t bumped = SIZE_STACK (solver->analyzed);
  move_analyzed_variables_to_front_of_queue (solver);
  ADD (literals_bumped, bumped);
  STOP (bump);
}
