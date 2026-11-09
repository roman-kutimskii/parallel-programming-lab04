#pragma once

#include <fstream>
#include <map>
#include <random>
#include <vector>

struct Ticket
{
    int train;  // 0 .. TrainCount-1
    int seat;   // 0 .. SeatCount-1
    bool operator==(const Ticket&) const = default;
};

// Журнал кассы: её билеты в памяти и файл sales/office-K.txt.
// В файл дописывается строка на каждую продажу («+ поезд место») и возврат («- поезд место»),
// номера в файле с 1. Файл сразу сбрасывается на диск, поэтому переживает падение кассы.
// Журнал — личный файл кассы, другие кассы его не трогают.
class Journal
{
public:
    explicit Journal(int officeId);

    void Add(Ticket ticket);
    void Remove(Ticket ticket);

    bool Empty() const { return m_tickets.empty(); }
    int Size() const { return static_cast<int>(m_tickets.size()); }
    Ticket RandomTicket(std::mt19937& rng) const;  // журнал не должен быть пустым

private:
    std::vector<Ticket> m_tickets;
    std::ofstream m_file;
};

void ClearJournals();                                  // удалить sales/ и создать заново
std::map<int, std::vector<Ticket>> ReadAllJournals();  // номер кассы -> её билеты
