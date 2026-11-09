#pragma once

#include <chrono>
#include <string>
#include <vector>

struct OfficeResult
{
    int id = 0;
    bool hung = false;    // не закончила за отведённое время и была убита
    int exitCode = 0;     // код возврата, если закончила сама
    std::string failure;  // пусто, если закончила с кодом 0; иначе — что случилось, по-русски
};

// Запускает count копий этой же программы с аргументами «--office --id K <extraArgs>», K = 1..count,
// и ждёт их. Кто не закончил за timeout (считая от запуска), того убивает.
std::vector<OfficeResult> RunOffices(int count, const std::vector<std::string>& extraArgs,
                                     std::chrono::milliseconds timeout);
