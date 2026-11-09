// Лабораторная 4: кассы вокзала — синхронизация процессов.
//
// Несколько касс (отдельных процессов) продают и возвращают билеты на одни и те же поезда.
// Места лежат в общей памяти (station.h), каждая касса ведёт свой журнал (journal.h).
// Сейчас кассы НИКАК не синхронизированы — одно место может продаться дважды.
#include "check.h"
#include "journal.h"
#include "spawn.h"
#include "station.h"

#include <charconv>
#include <chrono>
#include <format>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <string_view>

namespace
{

using Clock = std::chrono::steady_clock;

enum class Mode
{
    Run,
    Office,
    Check,
};

struct Options
{
    Mode mode = Mode::Run;
    int offices = 0;  // --run: сколько касс запустить
    int id = 0;       // --office: номер этой кассы, 1..MaxOffices
    int seconds = 0;
};

// Возвращает 0, если аргумент не является положительным целым.
int ParsePositive(std::string_view arg)
{
    int value = 0;
    const auto [end, ec] = std::from_chars(arg.data(), arg.data() + arg.size(), value);
    const bool parsed = (ec == std::errc{}) && (end == arg.data() + arg.size());
    return (parsed && value > 0) ? value : 0;
}

std::optional<Options> ParseArgs(int argc, char* argv[])
{
    Options options;
    std::optional<Mode> mode;
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];
        if (arg == "--run")
            mode = Mode::Run;
        else if (arg == "--office")
            mode = Mode::Office;
        else if (arg == "--check")
            mode = Mode::Check;
        else if (arg == "--offices" && i + 1 < argc)
            options.offices = ParsePositive(argv[++i]);
        else if (arg == "--id" && i + 1 < argc)
            options.id = ParsePositive(argv[++i]);
        else if (arg == "--seconds" && i + 1 < argc)
            options.seconds = ParsePositive(argv[++i]);
        else
            return std::nullopt;
    }
    if (!mode)
        return std::nullopt;
    options.mode = *mode;

    switch (options.mode)
    {
    case Mode::Run:
        if (options.offices < 1 || options.offices > MaxOffices || options.seconds == 0)
            return std::nullopt;
        break;
    case Mode::Office:
        if (options.id < 1 || options.id > MaxOffices || options.seconds == 0)
            return std::nullopt;
        break;
    case Mode::Check:
        break;
    }
    return options;
}

// Первое свободное место в поезде или -1, если мест нет.
int FindFreeSeat(const Train& train)
{
    for (int s = 0; s < SeatCount; ++s)
    {
        if (train.seats[s] == 0)
            return s;
    }
    return -1;
}

// ---------------------------------------------------------------------------------------------
// Касса. Работает в своём процессе: её запускает --run, по одной копии программы на кассу.
// ---------------------------------------------------------------------------------------------
int RunOffice(const Options& options)
{
    SharedStation shared = SharedStation::Open();
    Station& station = shared.Get();
    Journal journal(options.id);

    std::mt19937 rng(std::random_device{}() + static_cast<unsigned>(options.id));
    std::uniform_int_distribution<int> randomTrain(0, TrainCount - 1);
    // Чем больше у кассы билетов, тем чаще она возвращает: в среднем у каждой около TicketsPerOffice билетов,
    // поезда заполнены частично, и кассы работают всё отведённое время.
    constexpr int TicketsPerOffice = 20;
    std::uniform_int_distribution<int> sellThreshold(0, 2 * TicketsPerOffice);

    const auto deadline = Clock::now() + std::chrono::seconds(options.seconds);
    long long operations = 0;

    while (Clock::now() < deadline)
    {
        // ---- одна операция кассы: от сих... ----

        if (journal.Empty() || journal.Size() < sellThreshold(rng))
        {
            // Продажа: первое свободное место в случайном поезде.
            // Кассир сначала печатает билет (журнал), потом отмечает место в поезде.
            const int t = randomTrain(rng);
            Train& train = station.trains[t];
            const int seat = FindFreeSeat(train);
            if (seat >= 0)
            {
                journal.Add({t, seat});
                train.seats[seat] = options.id;
            }
            // Если мест нет — пассажир уходит ни с чем, это тоже операция.
        }
        else
        {
            // Возврат: случайный свой билет. Место освобождаем, только если оно записано за нами.
            const Ticket ticket = journal.RandomTicket(rng);
            int& seat = station.trains[ticket.train].seats[ticket.seat];
            if (seat == options.id)
                seat = 0;
            journal.Remove(ticket);
        }

        // ---- ...до сих ----

        ++operations;
    }

    station.operations[options.id] = operations;
    return 0;
}

// ---------------------------------------------------------------------------------------------
// Вокзал: создать общую память, запустить кассы, дождаться, сверить.
// ---------------------------------------------------------------------------------------------
int RunStation(const Options& options)
{
    SharedStation shared = SharedStation::Create();
    ClearJournals();

    const auto start = Clock::now();
    const auto results = RunOffices(options.offices, {"--seconds", std::to_string(options.seconds)},
                                    std::chrono::seconds(options.seconds + 5));
    const double seconds = std::chrono::duration<double>(Clock::now() - start).count();

    Station& station = shared.Get();
    long long operations = 0;
    bool officesOk = true;
    for (const OfficeResult& result : results)
    {
        operations += station.operations[result.id];
        if (!result.failure.empty())
        {
            std::cout << std::format("Касса {} {}\n", result.id, result.failure);
            officesOk = false;
        }
    }

    std::cout << std::format("Касс: {}, время: {:.2f} с, операций: {}, в секунду: {:.0f}\n", options.offices,
                             seconds, operations, operations / seconds);
    if (!officesOk)
        std::cout << "(операции упавших и зависших касс не посчитаны)\n";

    const CheckResult check = CheckStation(station);
    std::cout << std::format("По журналам касс продано: {}, занято мест в поездах: {} — {}\n", check.soldByJournals,
                             check.occupied, check.Ok() ? "сходится" : "ОШИБКА");

    constexpr std::size_t MaxShown = 10;
    for (std::size_t i = 0; i < check.problems.size() && i < MaxShown; ++i)
        std::cout << "  " << check.problems[i] << '\n';
    if (check.problems.size() > MaxShown)
        std::cout << std::format("  ... и ещё {} мест с ошибками\n", check.problems.size() - MaxShown);

    if (!officesOk)
        return 4;
    return check.Ok() ? 0 : 3;
}

int RunCheck()
{
    SharedStation shared = SharedStation::Open();
    const CheckResult check = CheckStation(shared.Get());
    std::cout << std::format("По журналам касс продано: {}, занято мест в поездах: {} — {}\n", check.soldByJournals,
                             check.occupied, check.Ok() ? "сходится" : "ОШИБКА");
    for (const std::string& problem : check.problems)
        std::cout << "  " << problem << '\n';
    return check.Ok() ? 0 : 3;
}

} // namespace

int main(int argc, char* argv[])
{
    const auto options = ParseArgs(argc, argv);
    if (!options)
    {
        std::cerr << std::format("Usage:\n"
                                 "  {0} --run --offices <1..{1}> --seconds <N>\n"
                                 "  {0} --office --id <1..{1}> --seconds <N>\n"
                                 "  {0} --check\n",
                                 argv[0], MaxOffices);
        return 1;
    }

    try
    {
        switch (options->mode)
        {
        case Mode::Run:
            return RunStation(*options);
        case Mode::Office:
            return RunOffice(*options);
        case Mode::Check:
            return RunCheck();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << std::format("Ошибка: {}\n", e.what());
        return 2;
    }
    return 2;
}
