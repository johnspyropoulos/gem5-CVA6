#include "cpu/cva6/stats.hh"

namespace gem5
{

namespace cva6
{

CVA6Stats::CVA6Stats(BaseCPU *base_cpu)
    : statistics::Group(base_cpu),
    ADD_STAT(quiesceCycles, statistics::units::Cycle::get(),
             "Total number of cycles that CPU has spent quiesced or waiting "
             "for an interrupt")
{
    quiesceCycles.prereq(quiesceCycles);
}

} // namespace cva6
} // namespace gem5
