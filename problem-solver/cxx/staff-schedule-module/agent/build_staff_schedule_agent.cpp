#include "build_staff_schedule_agent.hpp"
#include "keynodes/staff_schedule_keynodes.hpp"

#include <sc-memory/sc_memory.hpp>
#include <sc-memory/sc_iterator.hpp>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

using namespace std;

namespace
{
struct EmployeeInfo
{
  ScAddr addr;
  ScAddr role;
  vector<ScAddr> availableShiftTypes;
  size_t assignedCount = 0;
  size_t maxShifts = 5;
  vector<ScAddr> assignedShifts;
};

struct ShiftInfo
{
  ScAddr addr;
  ScAddr shiftType;
  ScAddr day;
};

struct ShiftSlot
{
  ScAddr shift;
  ScAddr role;
};

bool HasAddr(vector<ScAddr> const & list, ScAddr const & addr)
{
  for (auto const & item : list)
  {
    if (item == addr)
      return true;
  }
  return false;
}
}

ScAddr BuildStaffScheduleAgent::GetActionClass() const
{
  return StaffScheduleKeynodes::action_build_staff_schedule;
}

ScResult BuildStaffScheduleAgent::DoProgram(ScAction & action)
{
  m_logger.Debug("BuildStaffScheduleAgent started");

  try
  {
    auto const & [restaurantAddr] = action.GetArguments<1>();
    if (!m_context.IsElement(restaurantAddr))
    {
      m_logger.Error("Restaurant not specified.");
      return action.FinishWithError();
    }

    // Собираем типы смен один раз, чтобы использовать при проверке доступности.
    vector<ScAddr> allShiftTypes;
    ScIterator3Ptr itShiftTypes = m_context.CreateIterator3(
        StaffScheduleKeynodes::concept_shift_type,
        ScType::ConstPermPosArc,
        ScType::ConstNode);
    while (itShiftTypes->Next())
    {
      allShiftTypes.push_back(itShiftTypes->Get(2));
    }

    vector<EmployeeInfo> employees;
    ScIterator5Ptr itEmployees = m_context.CreateIterator5(
        restaurantAddr,
        ScType::ConstCommonArc,
        ScType::ConstNode,
        ScType::ConstPermPosArc,
        StaffScheduleKeynodes::nrel_has_employee);

    while (itEmployees->Next())
    {
      EmployeeInfo info;
      info.addr = itEmployees->Get(2);

      // У каждого сотрудника должна быть роль; некорректные записи пропускаем.
      ScIterator5Ptr itRole = m_context.CreateIterator5(
          info.addr,
          ScType::ConstCommonArc,
          ScType::ConstNode,
          ScType::ConstPermPosArc,
          StaffScheduleKeynodes::nrel_has_role);
      if (itRole->Next())
      {
        info.role = itRole->Get(2);
      }
      else
      {
        m_logger.Warning("Employee without role skipped");
        continue;
      }

      // Если доступные типы смен не указаны, считаем, что сотрудник доступен для всех типов.
      ScIterator5Ptr itShiftType = m_context.CreateIterator5(
          info.addr,
          ScType::ConstCommonArc,
          ScType::ConstNode,
          ScType::ConstPermPosArc,
          StaffScheduleKeynodes::nrel_available_shift_type);
      while (itShiftType->Next())
      {
        info.availableShiftTypes.push_back(itShiftType->Get(2));
      }
      if (info.availableShiftTypes.empty())
      {
        info.availableShiftTypes = allShiftTypes;
      }

      // Читаем недельный лимит; если его нет или он неверный, используем 5.
      ScIterator5Ptr itMax = m_context.CreateIterator5(
          info.addr,
          ScType::ConstCommonArc,
          ScType::ConstNodeLink,
          ScType::ConstPermPosArc,
          StaffScheduleKeynodes::nrel_max_shifts_per_week);
      if (itMax->Next())
      {
        ScAddr const & linkAddr = itMax->Get(2);
        string value;
        if (m_context.GetLinkContent(linkAddr, value))
        {
          try
          {
            info.maxShifts = static_cast<size_t>(stoi(value));
          }
          catch (exception const &)
          {
            info.maxShifts = 5;
          }
        }
      }

      employees.push_back(info);
    }

    if (employees.empty())
    {
      m_logger.Error("No employees found for restaurant");
      return action.FinishWithError();
    }

    vector<ShiftInfo> shifts;
    ScIterator3Ptr itShifts = m_context.CreateIterator3(
        StaffScheduleKeynodes::concept_shift,
        ScType::ConstPermPosArc,
        ScType::ConstNode);
    while (itShifts->Next())
    {
      ShiftInfo shift;
      shift.addr = itShifts->Get(2);

      ScIterator5Ptr itType = m_context.CreateIterator5(
          shift.addr,
          ScType::ConstCommonArc,
          ScType::ConstNode,
          ScType::ConstPermPosArc,
          StaffScheduleKeynodes::nrel_shift_type);
      if (!itType->Next())
      {
        m_logger.Warning("Shift without type skipped");
        continue;
      }
      shift.shiftType = itType->Get(2);

      ScIterator5Ptr itDay = m_context.CreateIterator5(
          shift.addr,
          ScType::ConstCommonArc,
          ScType::ConstNode,
          ScType::ConstPermPosArc,
          StaffScheduleKeynodes::nrel_shift_day);
      if (itDay->Next())
      {
        shift.day = itDay->Get(2);
      }

      shifts.push_back(shift);
    }

    if (shifts.empty())
    {
      m_logger.Warning("No shifts found");
      return action.FinishSuccessfully();
    }

    // Строим двудольный граф: сотрудник -> смена, если это разрешено.
    for (auto const & shift : shifts)
    {
      for (auto const & employee : employees)
      {
        if (HasAddr(employee.availableShiftTypes, shift.shiftType))
        {
          ScAddr arc = m_context.GenerateConnector(
              ScType::ConstCommonArc,
              employee.addr,
              shift.addr);
          m_context.GenerateConnector(
              ScType::ConstPermPosArc,
              StaffScheduleKeynodes::nrel_can_work,
              arc);
        }
      }
    }

    // Расширяем граф с учётом максимальной нагрузки: создаём слоты на каждую смену сотрудника.
    vector<vector<ScAddr>> employeeSlots(employees.size());
    for (size_t i = 0; i < employees.size(); ++i)
    {
      employeeSlots[i].reserve(employees[i].maxShifts);
      for (size_t k = 0; k < employees[i].maxShifts; ++k)
      {
        ScAddr slotNode = m_context.GenerateNode(ScType::ConstNode);
        m_context.GenerateConnector(
            ScType::ConstPermPosArc,
            StaffScheduleKeynodes::concept_employee_slot,
            slotNode);

        ScAddr slotArc = m_context.GenerateConnector(
            ScType::ConstCommonArc,
            employees[i].addr,
            slotNode);
        m_context.GenerateConnector(
            ScType::ConstPermPosArc,
            StaffScheduleKeynodes::nrel_employee_slot,
            slotArc);

        for (auto const & shift : shifts)
        {
          if (!HasAddr(employees[i].availableShiftTypes, shift.shiftType))
            continue;

          ScAddr canWorkArc = m_context.GenerateConnector(
              ScType::ConstCommonArc,
              slotNode,
              shift.addr);
          m_context.GenerateConnector(
              ScType::ConstPermPosArc,
              StaffScheduleKeynodes::nrel_slot_can_work,
              canWorkArc);
        }

        employeeSlots[i].push_back(slotNode);
      }
    }

    // Требования по составу смены.
    vector<pair<ScAddr, size_t>> requirements = {
        {StaffScheduleKeynodes::concept_cook, 1},
        {StaffScheduleKeynodes::concept_waiter, 2},
        {StaffScheduleKeynodes::concept_cleaner, 1},
        {StaffScheduleKeynodes::concept_admin, 1}};

    // Формируем слоты смен по ролям (каждый слот — отдельное назначение).
    vector<ShiftSlot> slots;
    slots.reserve(shifts.size() * requirements.size());
    for (auto const & shift : shifts)
    {
      for (auto const & requirement : requirements)
      {
        for (size_t i = 0; i < requirement.second; ++i)
        {
          slots.push_back({shift.addr, requirement.first});
        }
      }
    }

    ScAddr scheduleAddr = m_context.GenerateNode(ScType::ConstNode);
    m_context.GenerateConnector(
        ScType::ConstPermPosArc,
        StaffScheduleKeynodes::concept_week_schedule,
        scheduleAddr);

    ScAddr scheduleIdtf = m_context.GenerateLink();
    m_context.SetLinkContent(scheduleIdtf, string("Weekly staff schedule"));
    m_context.GenerateConnector(
        ScType::ConstPermPosArc,
        scheduleAddr,
        scheduleIdtf);
    m_context.GenerateConnector(
        ScType::ConstPermPosArc,
        StaffScheduleKeynodes::nrel_main_idtf,
        m_context.GenerateConnector(
            ScType::ConstCommonArc,
            scheduleAddr,
            scheduleIdtf));

    for (auto const & shift : shifts)
    {
      m_context.GenerateConnector(
          ScType::ConstPermPosArc,
          scheduleAddr,
          shift.addr);
    }

    // Ограничения: не более одной роли в одной смене для сотрудника и maxShifts в неделю.
    struct Edge
    {
      int to;
      int rev;
      int cap;
    };

    auto addEdge = [](vector<vector<Edge>> & graph, int from, int to, int cap) {
      graph[from].push_back({to, static_cast<int>(graph[to].size()), cap});
      graph[to].push_back({from, static_cast<int>(graph[from].size()) - 1, 0});
    };

    size_t employeeCount = employees.size();
    size_t shiftCount = shifts.size();
    size_t slotCount = slots.size();

    size_t employeeStart = 0;
    size_t employeeShiftStart = employeeStart + employeeCount;
    size_t slotStart = employeeShiftStart + employeeCount * shiftCount;
    size_t source = slotStart + slotCount;
    size_t sink = source + 1;

    vector<vector<Edge>> graph(sink + 1);

    for (size_t i = 0; i < employeeCount; ++i)
    {
      addEdge(graph, static_cast<int>(source), static_cast<int>(employeeStart + i),
              static_cast<int>(employees[i].maxShifts));
    }

    for (size_t i = 0; i < employeeCount; ++i)
    {
      for (size_t j = 0; j < shiftCount; ++j)
      {
        addEdge(graph,
                static_cast<int>(employeeStart + i),
                static_cast<int>(employeeShiftStart + i * shiftCount + j),
                1);
      }
    }

    for (size_t slotIndex = 0; slotIndex < slotCount; ++slotIndex)
    {
      ShiftSlot const & slot = slots[slotIndex];
      size_t shiftIndex = 0;
      for (; shiftIndex < shifts.size(); ++shiftIndex)
      {
        if (shifts[shiftIndex].addr == slot.shift)
          break;
      }

      for (size_t i = 0; i < employeeCount; ++i)
      {
        if (employees[i].role != slot.role)
          continue;

        if (!HasAddr(employees[i].availableShiftTypes, shifts[shiftIndex].shiftType))
          continue;

        addEdge(graph,
                static_cast<int>(employeeShiftStart + i * shiftCount + shiftIndex),
                static_cast<int>(slotStart + slotIndex),
                1);
      }

      addEdge(graph, static_cast<int>(slotStart + slotIndex), static_cast<int>(sink), 1);
    }

    vector<int> level(graph.size(), -1);
    vector<size_t> itPtr(graph.size(), 0);

    auto bfs = [&]() -> bool {
      fill(level.begin(), level.end(), -1);
      vector<int> queue;
      queue.push_back(static_cast<int>(source));
      level[source] = 0;
      for (size_t qi = 0; qi < queue.size(); ++qi)
      {
        int v = queue[qi];
        for (auto const & edge : graph[v])
        {
          if (edge.cap > 0 && level[edge.to] == -1)
          {
            level[edge.to] = level[v] + 1;
            queue.push_back(edge.to);
          }
        }
      }
      return level[sink] != -1;
    };

    function<int(int, int)> dfs = [&](int v, int pushed) -> int {
      if (pushed == 0)
        return 0;
      if (v == static_cast<int>(sink))
        return pushed;
      for (size_t & i = itPtr[v]; i < graph[v].size(); ++i)
      {
        Edge & edge = graph[v][i];
        if (edge.cap > 0 && level[edge.to] == level[v] + 1)
        {
          int tr = dfs(edge.to, min(pushed, edge.cap));
          if (tr == 0)
            continue;
          edge.cap -= tr;
          graph[edge.to][edge.rev].cap += tr;
          return tr;
        }
      }
      return 0;
    };

    int flow = 0;
    while (bfs())
    {
      fill(itPtr.begin(), itPtr.end(), 0);
      while (int pushed = dfs(static_cast<int>(source), 1 << 30))
      {
        flow += pushed;
      }
    }

    m_logger.Info("Matched " + to_string(flow) + " of " + to_string(slotCount) + " shift slots");

    vector<vector<ScAddr>> assignedPerShift(shifts.size());
    vector<vector<ScAddr>> assignedRolePerShift(shifts.size());
    vector<ScAddr> assignedArcs;

    for (size_t slotIndex = 0; slotIndex < slotCount; ++slotIndex)
    {
      int slotNode = static_cast<int>(slotStart + slotIndex);
      for (auto const & edge : graph[slotNode])
      {
        if (edge.to >= static_cast<int>(employeeShiftStart) &&
            edge.to < static_cast<int>(slotStart) &&
            edge.cap == 1)
        {
          size_t employeeShiftIdx = static_cast<size_t>(edge.to - employeeShiftStart);
          size_t employeeIndex = employeeShiftIdx / shiftCount;
          size_t shiftIndex = employeeShiftIdx % shiftCount;

          ShiftSlot const & slot = slots[slotIndex];
          EmployeeInfo & employee = employees[employeeIndex];

          ScAddr arc = m_context.GenerateConnector(
              ScType::ConstCommonArc,
              slot.shift,
              employee.addr);
          m_context.GenerateConnector(
              ScType::ConstPermPosArc,
              StaffScheduleKeynodes::nrel_assigned_employee,
              arc);
          assignedArcs.push_back(arc);

          employee.assignedCount += 1;
          employee.assignedShifts.push_back(slot.shift);

          assignedPerShift[shiftIndex].push_back(employee.addr);
          assignedRolePerShift[shiftIndex].push_back(slot.role);
          break;
        }
      }
    }

    // Проверяем полноту укомплектования смен и сохраняем причины.
    bool allShiftsStaffed = (static_cast<size_t>(flow) == slotCount);
    vector<ScAddr> staffingIssues;
    vector<ScAddr> reserveArcs;
    for (size_t i = 0; i < shifts.size(); ++i)
    {
      for (auto const & requirement : requirements)
      {
        size_t count = 0;
        for (size_t k = 0; k < assignedRolePerShift[i].size(); ++k)
        {
          if (assignedRolePerShift[i][k] == requirement.first)
            count++;
        }
        if (count < requirement.second)
        {
          m_logger.Warning("Shift has insufficient staff for required role");
          allShiftsStaffed = false;

          size_t missing = requirement.second - count;
          ScAddr issueNode = m_context.GenerateNode(ScType::ConstNode);
          m_context.GenerateConnector(
              ScType::ConstPermPosArc,
              StaffScheduleKeynodes::concept_staffing_issue,
              issueNode);

          ScAddr shiftArc = m_context.GenerateConnector(
              ScType::ConstCommonArc,
              issueNode,
              shifts[i].addr);
          m_context.GenerateConnector(
              ScType::ConstPermPosArc,
              StaffScheduleKeynodes::nrel_missing_shift,
              shiftArc);

          ScAddr roleArc = m_context.GenerateConnector(
              ScType::ConstCommonArc,
              issueNode,
              requirement.first);
          m_context.GenerateConnector(
              ScType::ConstPermPosArc,
              StaffScheduleKeynodes::nrel_missing_role,
              roleArc);

          ScAddr countLink = m_context.GenerateLink();
          m_context.SetLinkContent(countLink, to_string(missing));
          ScAddr countArc = m_context.GenerateConnector(
              ScType::ConstCommonArc,
              issueNode,
              countLink);
          m_context.GenerateConnector(
              ScType::ConstPermPosArc,
              StaffScheduleKeynodes::nrel_missing_count,
              countArc);

          staffingIssues.push_back(issueNode);
        }
      }
    }

    ScAddr staffedLink = m_context.GenerateLink();
    m_context.SetLinkContent(staffedLink, allShiftsStaffed ? string("true") : string("false"));
    ScAddr staffedArc = m_context.GenerateConnector(
        ScType::ConstCommonArc,
        scheduleAddr,
        staffedLink);
    m_context.GenerateConnector(
        ScType::ConstPermPosArc,
        StaffScheduleKeynodes::nrel_all_shifts_staffed,
        staffedArc);

    // Добавляем резервы для каждой смены и роли.
    for (size_t i = 0; i < shifts.size(); ++i)
    {
      for (auto const & requirement : requirements)
      {
        ScAddr role = requirement.first;
        ScAddr shiftType = shifts[i].shiftType;

        for (auto & employee : employees)
        {
          if (employee.role != role)
            continue;
          if (!HasAddr(employee.availableShiftTypes, shiftType))
            continue;
          if (HasAddr(assignedPerShift[i], employee.addr))
            continue;

          ScAddr reserveArc = m_context.GenerateConnector(
              ScType::ConstCommonArc,
              shifts[i].addr,
              employee.addr);
          m_context.GenerateConnector(
              ScType::ConstPermPosArc,
              StaffScheduleKeynodes::nrel_reserve_employee,
              reserveArc);
          reserveArcs.push_back(reserveArc);
          break;
        }
      }
    }

    vector<ScAddr> employeeScheduleArcs;
    vector<ScAddr> shiftCountArcs;
    for (auto & employee : employees)
    {
      ScAddr employeeSchedule = m_context.GenerateNode(ScType::ConstNode);
      m_context.GenerateConnector(
          ScType::ConstPermPosArc,
          StaffScheduleKeynodes::concept_week_schedule,
          employeeSchedule);

      for (auto const & shiftAddr : employee.assignedShifts)
      {
        m_context.GenerateConnector(
            ScType::ConstPermPosArc,
            employeeSchedule,
            shiftAddr);
      }

      ScAddr scheduleArc = m_context.GenerateConnector(
          ScType::ConstCommonArc,
          employee.addr,
          employeeSchedule);
      m_context.GenerateConnector(
          ScType::ConstPermPosArc,
          StaffScheduleKeynodes::nrel_employee_schedule,
          scheduleArc);
      employeeScheduleArcs.push_back(scheduleArc);

      ScAddr countLink = m_context.GenerateLink();
      m_context.SetLinkContent(countLink, to_string(employee.assignedCount));
      ScAddr countArc = m_context.GenerateConnector(
          ScType::ConstCommonArc,
          employee.addr,
          countLink);
      m_context.GenerateConnector(
          ScType::ConstPermPosArc,
          StaffScheduleKeynodes::nrel_shift_count,
          countArc);
      shiftCountArcs.push_back(countArc);
    }

    ScStructure result = m_context.GenerateStructure();
    result << restaurantAddr << scheduleAddr << staffedLink;
    for (auto const & shift : shifts)
      result << shift.addr;
    for (auto const & employee : employees)
      result << employee.addr;
    for (auto const & arc : assignedArcs)
      result << arc;
    for (auto const & arc : reserveArcs)
      result << arc;
    for (auto const & arc : employeeScheduleArcs)
      result << arc;
    for (auto const & arc : shiftCountArcs)
      result << arc;
    for (auto const & issue : staffingIssues)
      result << issue;

    action.SetResult(result);

    m_logger.Info("BuildStaffScheduleAgent finished successfully");
    return action.FinishSuccessfully();
  }
  catch (exception const & e)
  {
    m_logger.Error("BuildStaffScheduleAgent error: " + string(e.what()));
    return action.FinishWithError();
  }
}
