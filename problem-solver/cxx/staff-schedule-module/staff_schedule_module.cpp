#include "staff_schedule_module.h"

#include "agent/build_staff_schedule_agent.hpp"

SC_MODULE_REGISTER(StaffScheduleModule)
  ->Agent<BuildStaffScheduleAgent>();
