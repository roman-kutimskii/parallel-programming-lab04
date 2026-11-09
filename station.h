#pragma once

#include <boost/interprocess/managed_shared_memory.hpp>

inline constexpr int TrainCount = 5;
inline constexpr int SeatCount = 50;
inline constexpr int MaxOffices = 16;  // кассы нумеруются с 1, номер 0 означает «место свободно»

struct Train
{
    int seats[SeatCount];  // 0 — свободно, иначе номер кассы, которая продала место
};

// Всё, что лежит в общей памяти и видно сразу всем кассам.
struct Station
{
    Train trains[TrainCount];
    long long operations[MaxOffices + 1];  // operations[K] пишет только касса K, в конце работы
};

// Общая память с именем "station". Пока объект жив, процесс видит вокзал;
// сама память живёт в ОС, пока её не удалят, даже если все процессы завершились.
class SharedStation
{
public:
    static SharedStation Create();  // удалить старую память и создать новую, все места свободны
    static SharedStation Open();    // открыть существующую (её уже создал --run)
    static void Remove();

    Station& Get() { return *m_station; }

private:
    explicit SharedStation(boost::interprocess::managed_shared_memory memory);

    boost::interprocess::managed_shared_memory m_memory;
    Station* m_station;
};
