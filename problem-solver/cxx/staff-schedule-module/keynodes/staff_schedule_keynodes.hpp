#pragma once

#include <sc-memory/sc_keynodes.hpp>

class StaffScheduleKeynodes : public ScKeynodes
{
public:
  static inline ScKeynode const action_build_staff_schedule{
      "action_build_staff_schedule", ScType::ConstNodeClass};

  static inline ScKeynode const concept_employee{
      "concept_employee", ScType::ConstNodeClass};
  static inline ScKeynode const concept_shift{
      "concept_shift", ScType::ConstNodeClass};
  static inline ScKeynode const concept_shift_type{
      "concept_shift_type", ScType::ConstNodeClass};
  static inline ScKeynode const concept_week_schedule{
      "concept_week_schedule", ScType::ConstNodeClass};
  static inline ScKeynode const concept_restaurant{
      "concept_restaurant", ScType::ConstNodeClass};
  static inline ScKeynode const concept_employee_slot{
      "concept_employee_slot", ScType::ConstNodeClass};
  static inline ScKeynode const concept_staffing_issue{
      "concept_staffing_issue", ScType::ConstNodeClass};
  static inline ScKeynode const concept_cook{
      "concept_cook", ScType::ConstNodeClass};
  static inline ScKeynode const concept_waiter{
      "concept_waiter", ScType::ConstNodeClass};
  static inline ScKeynode const concept_cleaner{
      "concept_cleaner", ScType::ConstNodeClass};
  static inline ScKeynode const concept_admin{
      "concept_admin", ScType::ConstNodeClass};

  static inline ScKeynode const nrel_has_role{
      "nrel_has_role", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_has_employee{
      "nrel_has_employee", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_employee_slot{
      "nrel_employee_slot", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_slot_can_work{
      "nrel_slot_can_work", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_missing_role{
      "nrel_missing_role", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_missing_count{
      "nrel_missing_count", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_missing_shift{
      "nrel_missing_shift", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_available_shift_type{
      "nrel_available_shift_type", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_shift_type{
      "nrel_shift_type", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_shift_day{
      "nrel_shift_day", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_assigned_employee{
      "nrel_assigned_employee", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_reserve_employee{
      "nrel_reserve_employee", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_can_work{
      "nrel_can_work", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_employee_schedule{
      "nrel_employee_schedule", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_shift_count{
      "nrel_shift_count", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_max_shifts_per_week{
      "nrel_max_shifts_per_week", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_all_shifts_staffed{
      "nrel_all_shifts_staffed", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_main_idtf{
      "nrel_main_idtf", ScType::ConstNodeNonRole};
};
