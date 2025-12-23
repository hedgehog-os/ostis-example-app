#include <sc-memory/test/sc_test.hpp>

#include <sc-memory/sc_memory.hpp>
#include <sc-memory/sc_iterator.hpp>

#include "agent/build_staff_schedule_agent.hpp"
#include "keynodes/staff_schedule_keynodes.hpp"

using AgentTest = ScMemoryTest;

namespace
{
void AddRelation(ScMemoryContext & ctx, ScAddr const & src, ScAddr const & trg, ScAddr const & rel)
{
  ScAddr arc = ctx.GenerateConnector(ScType::ConstCommonArc, src, trg);
  ctx.GenerateConnector(ScType::ConstPermPosArc, rel, arc);
}

ScAddr CreateShiftType(ScMemoryContext & ctx)
{
  ScAddr shiftType = ctx.GenerateNode(ScType::ConstNode);
  ctx.GenerateConnector(
      ScType::ConstPermPosArc,
      StaffScheduleKeynodes::concept_shift_type,
      shiftType);
  return shiftType;
}

ScAddr CreateEmployeeWithMax(
    ScMemoryContext & ctx,
    ScAddr const & role,
    ScAddr const & availableShiftType,
    std::string const & maxShifts)
{
  ScAddr employee = ctx.GenerateNode(ScType::ConstNode);
  ctx.GenerateConnector(
      ScType::ConstPermPosArc,
      StaffScheduleKeynodes::concept_employee,
      employee);

  AddRelation(ctx, employee, role, StaffScheduleKeynodes::nrel_has_role);
  AddRelation(ctx, employee, availableShiftType, StaffScheduleKeynodes::nrel_available_shift_type);

  ScAddr maxLink = ctx.GenerateLink();
  ctx.SetLinkContent(maxLink, maxShifts);
  AddRelation(ctx, employee, maxLink, StaffScheduleKeynodes::nrel_max_shifts_per_week);

  return employee;
}

ScAddr CreateEmployee(ScMemoryContext & ctx, ScAddr const & role, ScAddr const & availableShiftType)
{
  return CreateEmployeeWithMax(ctx, role, availableShiftType, "5");
}

ScAddr CreateShift(ScMemoryContext & ctx, ScAddr const & shiftType)
{
  ScAddr shift = ctx.GenerateNode(ScType::ConstNode);
  ctx.GenerateConnector(
      ScType::ConstPermPosArc,
      StaffScheduleKeynodes::concept_shift,
      shift);
  AddRelation(ctx, shift, shiftType, StaffScheduleKeynodes::nrel_shift_type);
  return shift;
}

size_t GetShiftCount(ScMemoryContext & ctx, ScAddr const & employee)
{
  ScIterator5Ptr it = ctx.CreateIterator5(
      employee,
      ScType::ConstCommonArc,
      ScType::ConstNodeLink,
      ScType::ConstPermPosArc,
      StaffScheduleKeynodes::nrel_shift_count);
  if (!it->Next())
    return 0;

  std::string value;
  if (!ctx.GetLinkContent(it->Get(2), value))
    return 0;

  try
  {
    return static_cast<size_t>(std::stoul(value));
  }
  catch (std::exception const &)
  {
    return 0;
  }
}

bool HasCanWork(ScMemoryContext & ctx, ScAddr const & employee, ScAddr const & shift)
{
  ScIterator5Ptr it = ctx.CreateIterator5(
      employee,
      ScType::ConstCommonArc,
      shift,
      ScType::ConstPermPosArc,
      StaffScheduleKeynodes::nrel_can_work);
  return it->Next();
}

ScAddr CreateRestaurant(ScMemoryContext & ctx)
{
  ScAddr restaurant = ctx.GenerateNode(ScType::ConstNode);
  ctx.GenerateConnector(
      ScType::ConstPermPosArc,
      StaffScheduleKeynodes::concept_restaurant,
      restaurant);
  return restaurant;
}

void AddEmployeeToRestaurant(ScMemoryContext & ctx, ScAddr const & restaurant, ScAddr const & employee)
{
  AddRelation(ctx, restaurant, employee, StaffScheduleKeynodes::nrel_has_employee);
}

std::string GetAllShiftsStaffed(ScMemoryContext & ctx)
{
  ScIterator5Ptr it = ctx.CreateIterator5(
      ScType::ConstNode,
      ScType::ConstCommonArc,
      ScType::ConstNodeLink,
      ScType::ConstPermPosArc,
      StaffScheduleKeynodes::nrel_all_shifts_staffed);
  if (!it->Next())
    return "";

  std::string value;
  if (!ctx.GetLinkContent(it->Get(2), value))
    return "";
  return value;
}
}

TEST_F(AgentTest, BuildStaffScheduleAgentBasic)
{
  m_ctx->SubscribeAgent<BuildStaffScheduleAgent>();

  ScAddr restaurant = CreateRestaurant(*m_ctx);
  ScAddr dayShiftType = CreateShiftType(*m_ctx);

  ScAddr shift = CreateShift(*m_ctx, dayShiftType);

  ScAddr cook = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_cook, dayShiftType);
  ScAddr waiter1 = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_waiter, dayShiftType);
  ScAddr waiter2 = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_waiter, dayShiftType);
  ScAddr waiter3 = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_waiter, dayShiftType);
  ScAddr cleaner = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_cleaner, dayShiftType);
  ScAddr admin = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_admin, dayShiftType);

  AddEmployeeToRestaurant(*m_ctx, restaurant, cook);
  AddEmployeeToRestaurant(*m_ctx, restaurant, waiter1);
  AddEmployeeToRestaurant(*m_ctx, restaurant, waiter2);
  AddEmployeeToRestaurant(*m_ctx, restaurant, waiter3);
  AddEmployeeToRestaurant(*m_ctx, restaurant, cleaner);
  AddEmployeeToRestaurant(*m_ctx, restaurant, admin);

  ScAction action = m_ctx->GenerateAction(
      StaffScheduleKeynodes::action_build_staff_schedule);
  action.SetArguments(restaurant);

  EXPECT_TRUE(action.InitiateAndWait());
  EXPECT_TRUE(action.IsFinishedSuccessfully());

  ScIterator5Ptr itAssigned = m_ctx->CreateIterator5(
      shift,
      ScType::ConstCommonArc,
      ScType::ConstNode,
      ScType::ConstPermPosArc,
      StaffScheduleKeynodes::nrel_assigned_employee);

  size_t cooks = 0;
  size_t waiters = 0;
  size_t cleaners = 0;
  size_t admins = 0;

  while (itAssigned->Next())
  {
    ScAddr employee = itAssigned->Get(2);
    ScIterator5Ptr itRole = m_ctx->CreateIterator5(
        employee,
        ScType::ConstCommonArc,
        ScType::ConstNode,
        ScType::ConstPermPosArc,
        StaffScheduleKeynodes::nrel_has_role);
    if (!itRole->Next())
      continue;

    ScAddr role = itRole->Get(2);
    if (role == StaffScheduleKeynodes::concept_cook)
      cooks++;
    else if (role == StaffScheduleKeynodes::concept_waiter)
      waiters++;
    else if (role == StaffScheduleKeynodes::concept_cleaner)
      cleaners++;
    else if (role == StaffScheduleKeynodes::concept_admin)
      admins++;
  }

  EXPECT_EQ(cooks, 1u);
  EXPECT_EQ(waiters, 2u);
  EXPECT_EQ(cleaners, 1u);
  EXPECT_EQ(admins, 1u);

  ScIterator5Ptr itReserve = m_ctx->CreateIterator5(
      shift,
      ScType::ConstCommonArc,
      ScType::ConstNode,
      ScType::ConstPermPosArc,
      StaffScheduleKeynodes::nrel_reserve_employee);
  EXPECT_TRUE(itReserve->Next());

  ScIterator5Ptr itCanWork = m_ctx->CreateIterator5(
      ScType::ConstNode,
      ScType::ConstCommonArc,
      shift,
      ScType::ConstPermPosArc,
      StaffScheduleKeynodes::nrel_can_work);
  EXPECT_TRUE(itCanWork->Next());

  ScIterator5Ptr itSchedule = m_ctx->CreateIterator5(
      ScType::ConstNode,
      ScType::ConstCommonArc,
      ScType::ConstNode,
      ScType::ConstPermPosArc,
      StaffScheduleKeynodes::nrel_employee_schedule);
  EXPECT_TRUE(itSchedule->Next());

  ScIterator5Ptr itShiftCount = m_ctx->CreateIterator5(
      ScType::ConstNode,
      ScType::ConstCommonArc,
      ScType::ConstNodeLink,
      ScType::ConstPermPosArc,
      StaffScheduleKeynodes::nrel_shift_count);
  EXPECT_TRUE(itShiftCount->Next());

  EXPECT_EQ(GetAllShiftsStaffed(*m_ctx), "true");

  m_ctx->UnsubscribeAgent<BuildStaffScheduleAgent>();
}

TEST_F(AgentTest, BuildStaffScheduleAgentRespectsShiftType)
{
  m_ctx->SubscribeAgent<BuildStaffScheduleAgent>();

  ScAddr restaurant = CreateRestaurant(*m_ctx);
  ScAddr dayType = CreateShiftType(*m_ctx);
  ScAddr nightType = CreateShiftType(*m_ctx);

  ScAddr dayShift = CreateShift(*m_ctx, dayType);
  ScAddr nightShift = CreateShift(*m_ctx, nightType);

  ScAddr cookDay = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_cook, dayType);
  ScAddr cookNight = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_cook, nightType);
  ScAddr waiterDay1 = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_waiter, dayType);
  ScAddr waiterNight1 = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_waiter, nightType);
  ScAddr waiterDay2 = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_waiter, dayType);
  ScAddr waiterNight2 = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_waiter, nightType);
  ScAddr cleanerDay = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_cleaner, dayType);
  ScAddr cleanerNight = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_cleaner, nightType);
  ScAddr admin = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_admin, dayType);

  AddEmployeeToRestaurant(*m_ctx, restaurant, cookDay);
  AddEmployeeToRestaurant(*m_ctx, restaurant, cookNight);
  AddEmployeeToRestaurant(*m_ctx, restaurant, waiterDay1);
  AddEmployeeToRestaurant(*m_ctx, restaurant, waiterNight1);
  AddEmployeeToRestaurant(*m_ctx, restaurant, waiterDay2);
  AddEmployeeToRestaurant(*m_ctx, restaurant, waiterNight2);
  AddEmployeeToRestaurant(*m_ctx, restaurant, cleanerDay);
  AddEmployeeToRestaurant(*m_ctx, restaurant, cleanerNight);
  AddEmployeeToRestaurant(*m_ctx, restaurant, admin);

  ScAction action = m_ctx->GenerateAction(
      StaffScheduleKeynodes::action_build_staff_schedule);
  action.SetArguments(restaurant);

  EXPECT_TRUE(action.InitiateAndWait());
  EXPECT_TRUE(action.IsFinishedSuccessfully());

  size_t dayAdmins = 0;
  ScIterator5Ptr itDayAdmins = m_ctx->CreateIterator5(
      dayShift,
      ScType::ConstCommonArc,
      ScType::ConstNode,
      ScType::ConstPermPosArc,
      StaffScheduleKeynodes::nrel_assigned_employee);
  while (itDayAdmins->Next())
  {
    ScAddr employee = itDayAdmins->Get(2);
    if (employee == admin)
      dayAdmins++;
  }

  size_t nightAdmins = 0;
  ScIterator5Ptr itNightAdmins = m_ctx->CreateIterator5(
      nightShift,
      ScType::ConstCommonArc,
      ScType::ConstNode,
      ScType::ConstPermPosArc,
      StaffScheduleKeynodes::nrel_assigned_employee);
  while (itNightAdmins->Next())
  {
    ScAddr employee = itNightAdmins->Get(2);
    if (employee == admin)
      nightAdmins++;
  }

  EXPECT_EQ(dayAdmins, 1u);
  EXPECT_EQ(nightAdmins, 0u);

  m_ctx->UnsubscribeAgent<BuildStaffScheduleAgent>();
}

TEST_F(AgentTest, BuildStaffScheduleAgentRespectsMaxShifts)
{
  m_ctx->SubscribeAgent<BuildStaffScheduleAgent>();

  ScAddr restaurant = CreateRestaurant(*m_ctx);
  ScAddr dayType = CreateShiftType(*m_ctx);

  CreateShift(*m_ctx, dayType);
  CreateShift(*m_ctx, dayType);

  ScAddr cookLimited = CreateEmployeeWithMax(
      *m_ctx, StaffScheduleKeynodes::concept_cook, dayType, "1");
  ScAddr cook = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_cook, dayType);
  ScAddr waiter1 = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_waiter, dayType);
  ScAddr waiter2 = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_waiter, dayType);
  ScAddr cleaner = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_cleaner, dayType);
  ScAddr admin = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_admin, dayType);

  AddEmployeeToRestaurant(*m_ctx, restaurant, cookLimited);
  AddEmployeeToRestaurant(*m_ctx, restaurant, cook);
  AddEmployeeToRestaurant(*m_ctx, restaurant, waiter1);
  AddEmployeeToRestaurant(*m_ctx, restaurant, waiter2);
  AddEmployeeToRestaurant(*m_ctx, restaurant, cleaner);
  AddEmployeeToRestaurant(*m_ctx, restaurant, admin);

  ScAction action = m_ctx->GenerateAction(
      StaffScheduleKeynodes::action_build_staff_schedule);
  action.SetArguments(restaurant);

  EXPECT_TRUE(action.InitiateAndWait());
  EXPECT_TRUE(action.IsFinishedSuccessfully());

  EXPECT_LE(GetShiftCount(*m_ctx, cookLimited), 1u);

  m_ctx->UnsubscribeAgent<BuildStaffScheduleAgent>();
}

TEST_F(AgentTest, BuildStaffScheduleAgentCanWorkRespectsShiftType)
{
  m_ctx->SubscribeAgent<BuildStaffScheduleAgent>();

  ScAddr restaurant = CreateRestaurant(*m_ctx);
  ScAddr dayType = CreateShiftType(*m_ctx);
  ScAddr nightType = CreateShiftType(*m_ctx);

  ScAddr dayShift = CreateShift(*m_ctx, dayType);
  ScAddr nightShift = CreateShift(*m_ctx, nightType);

  ScAddr admin = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_admin, dayType);
  ScAddr cook = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_cook, dayType);
  ScAddr waiter1 = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_waiter, dayType);
  ScAddr waiter2 = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_waiter, dayType);
  ScAddr cleaner = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_cleaner, dayType);

  AddEmployeeToRestaurant(*m_ctx, restaurant, admin);
  AddEmployeeToRestaurant(*m_ctx, restaurant, cook);
  AddEmployeeToRestaurant(*m_ctx, restaurant, waiter1);
  AddEmployeeToRestaurant(*m_ctx, restaurant, waiter2);
  AddEmployeeToRestaurant(*m_ctx, restaurant, cleaner);

  ScAction action = m_ctx->GenerateAction(
      StaffScheduleKeynodes::action_build_staff_schedule);
  action.SetArguments(restaurant);

  EXPECT_TRUE(action.InitiateAndWait());
  EXPECT_TRUE(action.IsFinishedSuccessfully());

  EXPECT_TRUE(HasCanWork(*m_ctx, admin, dayShift));
  EXPECT_FALSE(HasCanWork(*m_ctx, admin, nightShift));

  m_ctx->UnsubscribeAgent<BuildStaffScheduleAgent>();
}

TEST_F(AgentTest, BuildStaffScheduleAgentNoShifts)
{
  m_ctx->SubscribeAgent<BuildStaffScheduleAgent>();

  ScAddr restaurant = CreateRestaurant(*m_ctx);
  ScAddr dayType = CreateShiftType(*m_ctx);
  ScAddr cook = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_cook, dayType);
  ScAddr waiter1 = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_waiter, dayType);
  ScAddr waiter2 = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_waiter, dayType);
  ScAddr cleaner = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_cleaner, dayType);
  ScAddr admin = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_admin, dayType);

  AddEmployeeToRestaurant(*m_ctx, restaurant, cook);
  AddEmployeeToRestaurant(*m_ctx, restaurant, waiter1);
  AddEmployeeToRestaurant(*m_ctx, restaurant, waiter2);
  AddEmployeeToRestaurant(*m_ctx, restaurant, cleaner);
  AddEmployeeToRestaurant(*m_ctx, restaurant, admin);

  ScAction action = m_ctx->GenerateAction(
      StaffScheduleKeynodes::action_build_staff_schedule);
  action.SetArguments(restaurant);

  EXPECT_TRUE(action.InitiateAndWait());
  EXPECT_TRUE(action.IsFinishedSuccessfully());

  m_ctx->UnsubscribeAgent<BuildStaffScheduleAgent>();
}

TEST_F(AgentTest, BuildStaffScheduleAgentNotEnoughStaff)
{
  m_ctx->SubscribeAgent<BuildStaffScheduleAgent>();

  ScAddr restaurant = CreateRestaurant(*m_ctx);
  ScAddr dayType = CreateShiftType(*m_ctx);
  ScAddr shift = CreateShift(*m_ctx, dayType);

  ScAddr cook = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_cook, dayType);
  ScAddr waiter = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_waiter, dayType);
  ScAddr cleaner = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_cleaner, dayType);
  ScAddr admin = CreateEmployee(*m_ctx, StaffScheduleKeynodes::concept_admin, dayType);

  AddEmployeeToRestaurant(*m_ctx, restaurant, cook);
  AddEmployeeToRestaurant(*m_ctx, restaurant, waiter);
  AddEmployeeToRestaurant(*m_ctx, restaurant, cleaner);
  AddEmployeeToRestaurant(*m_ctx, restaurant, admin);

  ScAction action = m_ctx->GenerateAction(
      StaffScheduleKeynodes::action_build_staff_schedule);
  action.SetArguments(restaurant);

  EXPECT_TRUE(action.InitiateAndWait());
  EXPECT_TRUE(action.IsFinishedSuccessfully());

  ScIterator5Ptr itAssigned = m_ctx->CreateIterator5(
      shift,
      ScType::ConstCommonArc,
      ScType::ConstNode,
      ScType::ConstPermPosArc,
      StaffScheduleKeynodes::nrel_assigned_employee);

  size_t assignedCount = 0;
  while (itAssigned->Next())
  {
    assignedCount++;
  }

  EXPECT_LE(assignedCount, 5u);

  EXPECT_EQ(GetAllShiftsStaffed(*m_ctx), "false");

  m_ctx->UnsubscribeAgent<BuildStaffScheduleAgent>();
}
