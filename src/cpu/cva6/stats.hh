/**
 * @file
 *
 * The stats for CVA6CPU separated from the CPU definition.
 */

#ifndef __CPU_CVA6_STATS_HH__
#define __CPU_CVA6_STATS_HH__

#include "base/statistics.hh"
#include "cpu/base.hh"
#include "sim/ticked_object.hh"

namespace gem5
{

namespace cva6
{

/** Currently unused stats class. */
struct CVA6Stats : public statistics::Group
{
    CVA6Stats(BaseCPU *parent);

    /** Number of cycles in quiescent state */
    statistics::Scalar quiesceCycles;

};

} // namespace cva6
} // namespace gem5

#endif /* __CPU_CVA6_STATS_HH__ */
