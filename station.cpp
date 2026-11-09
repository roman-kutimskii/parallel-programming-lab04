#include "station.h"

#include <stdexcept>
#include <utility>

namespace bip = boost::interprocess;

namespace
{

constexpr const char* MemoryName = "station";
constexpr const char* ObjectName = "Station";
constexpr std::size_t MemorySize = 64 * 1024;

} // namespace

SharedStation::SharedStation(bip::managed_shared_memory memory)
    : m_memory(std::move(memory))
    , m_station(m_memory.find<Station>(ObjectName).first)
{
    if (m_station == nullptr)
        throw std::runtime_error("в общей памяти нет вокзала");
}

SharedStation SharedStation::Create()
{
    Remove();
    bip::managed_shared_memory memory(bip::create_only, MemoryName, MemorySize);
    memory.construct<Station>(ObjectName)();  // () — все поля нулями: места свободны
    return SharedStation(std::move(memory));
}

SharedStation SharedStation::Open()
{
    return SharedStation(bip::managed_shared_memory(bip::open_only, MemoryName));
}

void SharedStation::Remove()
{
    bip::shared_memory_object::remove(MemoryName);
}
