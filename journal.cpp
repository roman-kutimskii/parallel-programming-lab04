#include "journal.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <format>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace
{

const fs::path SalesDir = "sales";

fs::path JournalPath(int officeId)
{
    return SalesDir / std::format("office-{}.txt", officeId);
}

} // namespace

Journal::Journal(int officeId)
    : m_file(JournalPath(officeId), std::ios::app)
{
    if (!m_file)
        throw std::runtime_error(std::format("не удалось открыть {}", JournalPath(officeId).string()));
}

void Journal::Add(Ticket ticket)
{
    m_tickets.push_back(ticket);
    m_file << "+ " << ticket.train + 1 << ' ' << ticket.seat + 1 << std::endl;  // endl — сразу на диск
}

void Journal::Remove(Ticket ticket)
{
    const auto it = std::find(m_tickets.begin(), m_tickets.end(), ticket);
    if (it == m_tickets.end())
        throw std::logic_error("возврат билета, которого нет в журнале");
    m_tickets.erase(it);
    m_file << "- " << ticket.train + 1 << ' ' << ticket.seat + 1 << std::endl;
}

Ticket Journal::RandomTicket(std::mt19937& rng) const
{
    std::uniform_int_distribution<std::size_t> pick(0, m_tickets.size() - 1);
    return m_tickets[pick(rng)];
}

void ClearJournals()
{
    fs::remove_all(SalesDir);
    fs::create_directories(SalesDir);
}

std::map<int, std::vector<Ticket>> ReadAllJournals()
{
    std::map<int, std::vector<Ticket>> result;
    if (!fs::exists(SalesDir))
        return result;

    for (const auto& entry : fs::directory_iterator(SalesDir))
    {
        int officeId = 0;
        const std::string name = entry.path().filename().string();
        if (std::sscanf(name.c_str(), "office-%d.txt", &officeId) != 1)
            continue;

        std::vector<Ticket>& tickets = result[officeId];
        std::ifstream file(entry.path());
        char op = 0;
        Ticket ticket{};
        while (file >> op >> ticket.train >> ticket.seat)
        {
            --ticket.train;
            --ticket.seat;
            if (op == '+')
            {
                tickets.push_back(ticket);
            }
            else
            {
                const auto it = std::find(tickets.begin(), tickets.end(), ticket);
                if (it != tickets.end())
                    tickets.erase(it);
            }
        }
    }
    return result;
}
