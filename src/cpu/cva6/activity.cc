#include "cpu/cva6/activity.hh"

#include <sstream>

#include "cpu/cva6/trace.hh"

namespace gem5
{

namespace cva6
{

void
CVA6ActivityRecorder::cva6Trace() const
{
    std::ostringstream stages;
    unsigned int num_stages = getNumStages();

    unsigned int stage_index = 0;
    while (stage_index < num_stages) {
        stages << (getStageActive(stage_index) ? '1' : 'E');

        stage_index++;
        if (stage_index != num_stages)
            stages << ',';
    }

    cva6::cva6Trace("activity=%d stages=%s\n", getActivityCount(),
            stages.str());
}

} // namespace cva6
} // namespace gem5
