#pragma once

#include "station.h"

#include <string>
#include <vector>

struct CheckResult
{
    int soldByJournals = 0;  // билетов во всех журналах касс
    int occupied = 0;        // занятых мест в поездах
    std::vector<std::string> problems;

    bool Ok() const { return problems.empty(); }
};

// Сверка мест в общей памяти с журналами касс. Запускать, когда ни одна касса не работает.
// Место в порядке, если оно свободно и ни у кого нет на него билета,
// или если в поезде записана ровно та касса, у которой есть на него билет.
CheckResult CheckStation(const Station& station);
