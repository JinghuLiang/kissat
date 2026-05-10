#ifndef _inlinequeue_h_INCLUDED
#define _inlinequeue_h_INCLUDED

#include "../../core/main/internal.h"
#include "../runtime/logging.h"
#include "assert.h"
#include "../data-structures/queue.h"

static inline void kissat_update_queue (kissat* solver, queue* queue, const links *links,
                                        unsigned idx) {
  assert (!DISCONNECTED (idx));
  const unsigned stamp = links[idx].stamp;
  LOG ("queue updated to %s stamped %u", LOGVAR (idx), stamp);
  queue->search.idx = idx;
  queue->search.stamp = stamp;
}

static inline void kissat_enqueue_links (kissat *solver, unsigned i,
                                         links *links, queue *queue) {
  struct links *p = links + i;
  assert (DISCONNECTED (p->prev));
  assert (DISCONNECTED (p->next));
  const unsigned j = p->prev = queue->last;
  queue->last = i;
  if (DISCONNECTED (j))
    queue->first = i;
  else {
    struct links *l = links + j;
    assert (DISCONNECTED (l->next));
    l->next = i;
  }
  if (queue->stamp == UINT_MAX) {
    kissat_reassign_queue_stamps (solver);
    assert (p->stamp == queue->stamp);
  } else
    p->stamp = ++queue->stamp;
}

static inline void kissat_dequeue_links (unsigned i, links *links,
                                         queue *queue) {
  struct links *l = links + i;
  const unsigned j = l->prev, k = l->next;
  l->prev = l->next = DISCONNECT;
  if (DISCONNECTED (j)) {
    assert (queue->first == i);
    queue->first = k;
  } else {
    struct links *p = links + j;
    assert (p->next == i);
    p->next = k;
  }
  if (DISCONNECTED (k)) {
    assert (queue->last == i);
    queue->last = j;
  } else {
    struct links *n = links + k;
    assert (n->prev == i);
    n->prev = j;
  }
}

static inline void kissat_enqueue (kissat *solver,queue* queue,links * link, unsigned idx) {
  assert (idx < solver->vars);
  links* l = link + idx;
  l->prev = l->next = DISCONNECT;
  kissat_enqueue_links (solver, idx, link, queue);
  LOG ("enqueued %s stamped %u", LOGVAR (idx), l->stamp);
  if (!VALUE (LIT (idx)))
    kissat_update_queue (solver, queue, link, idx);
  kissat_check_queue (solver);
}

static inline void kissat_dequeue (kissat *solver,queue* queue,links* link, unsigned idx) {
  assert (idx < solver->vars);
  LOG ("dequeued %s", LOGVAR (idx));
  if (queue->search.idx == idx) {
    struct links *l = link + idx;
    unsigned search = l->next;
    if (search == DISCONNECT)
      search = l->prev;
    if (search == DISCONNECT) {
      queue->search.idx = DISCONNECT;
      queue->search.stamp = 0;
    } else
      kissat_update_queue (solver, queue, link, search);
  }
  kissat_dequeue_links (idx, link, queue);
  kissat_check_queue (solver);
}

static inline void kissat_move_to_front (kissat *solver,queue* queue,links* links, unsigned idx) {
  if (idx == queue->last) {
    assert (DISCONNECTED (links[idx].next));
    return;
  }
  assert (idx < solver->vars);
  const value tmp = VALUE (LIT (idx));
  if (tmp && queue->search.idx == idx) {
    unsigned prev = links[idx].prev;
    if (!DISCONNECTED (prev))
      kissat_update_queue (solver, queue, links, prev);
    else {
      unsigned next = links[idx].next;
      assert (!DISCONNECTED (next));
      kissat_update_queue (solver, queue, links, next);
    }
  }
  kissat_dequeue_links (idx, links, queue);
  kissat_enqueue_links (solver, idx, links, queue);
  LOG ("moved-to-front %s stamped %u", LOGVAR (idx), links[idx].stamp);
  if (!tmp)
    kissat_update_queue (solver, queue, links, idx);
  kissat_check_queue (solver);
}

#endif
