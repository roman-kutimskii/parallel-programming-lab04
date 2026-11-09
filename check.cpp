#include "check.h"
#include "journal.h"

#include <format>
#include <map>
#include <set>

namespace
{

std::string Owner(int officeId)
{
    return officeId == 0 ? "место свободно" : std::format("в поезде записана касса {}", officeId);
}

// «1 и 2», «1, 2 и 4», «1 (×3) и 2» — каждая касса один раз, повторы числом.
std::string OfficeList(const std::vector<int>& offices)
{
    std::map<int, int> counts;
    for (int id : offices)
        ++counts[id];

    std::string result;
    std::size_t i = 0;
    for (const auto& [id, count] : counts)
    {
        if (i > 0)
            result += (i + 1 == counts.size()) ? " и " : ", ";
        result += std::to_string(id);
        if (count > 1)
            result += std::format(" (×{})", count);
        ++i;
    }
    return result;
}

} // namespace

CheckResult CheckStation(const Station& station)
{
    // claims[t][s] — какие кассы считают, что продали место s в поезде t
    std::vector<std::vector<std::vector<int>>> claims(TrainCount, std::vector<std::vector<int>>(SeatCount));

    CheckResult result;
    for (const auto& [officeId, tickets] : ReadAllJournals())
    {
        for (const Ticket& ticket : tickets)
        {
            claims[ticket.train][ticket.seat].push_back(officeId);
            ++result.soldByJournals;
        }
    }

    for (int t = 0; t < TrainCount; ++t)
    {
        for (int s = 0; s < SeatCount; ++s)
        {
            const int owner = station.trains[t].seats[s];
            const std::vector<int>& who = claims[t][s];
            if (owner != 0)
                ++result.occupied;

            const bool ok = (who.empty() && owner == 0) || (who.size() == 1 && who[0] == owner);
            if (ok)
                continue;

            const std::string place = std::format("поезд {}, место {}", t + 1, s + 1);
            if (who.empty())
                result.problems.push_back(
                    std::format("{}: занято кассой {}, но билета нет ни в одном журнале", place, owner));
            else if (who.size() == 1)
                result.problems.push_back(std::format("{}: билет у кассы {}, а {}", place, who[0], Owner(owner)));
            else
            {
                const bool oneOffice = std::set<int>(who.begin(), who.end()).size() == 1;
                result.problems.push_back(std::format("{}: продано {} {}, {}", place, oneOffice ? "кассой" : "кассами",
                                                      OfficeList(who), Owner(owner)));
            }
        }
    }
    return result;
}
