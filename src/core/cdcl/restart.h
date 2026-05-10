#ifndef _restart_h_INCLUDED
#define _restart_h_INCLUDED

#include <stdbool.h>

struct kissat;
struct clause;

bool kissat_restarting (struct kissat *);
void kissat_restart (struct kissat *);

bool kissat_light_restarting (struct kissat *);
struct clause *kissat_light_restart (struct kissat *);

void kissat_update_focused_restart_limit (struct kissat *);

#endif
