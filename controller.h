#ifndef _CONTROLLER_H_
#define _CONTROLLER_H_

#include "controller.h"

#include "module.h"

#define TEMP_FILTER_INIT(tfilter)

module_state
controller_state_func(struct module_instance * this_module);

void
controller_module_init(struct module_instance * this_module);

#endif // _CONTROLLER_H_
