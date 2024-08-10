/**
 * @file
 *
 *  ActivityRecoder from cpu/activity.h wrapped to provide evaluate and
 *  cva6Trace.
 */

#ifndef __CPU_CVA6_ACTIVITY_HH__
#define __CPU_CVA6_ACTIVITY_HH__

#include "cpu/activity.hh"

namespace gem5
{

namespace cva6
{

/** ActivityRecorder with a Ticked interface */
class CVA6ActivityRecorder : public ActivityRecorder
{
  public:
    /** Ticked interface */
    void evaluate() { advance(); }
    void cva6Trace() const;

  public:
    CVA6ActivityRecorder(const std::string &name, int num_stages,
        int longest_latency) :
        ActivityRecorder(name, num_stages, longest_latency, 0)
    { }
};

} // namespace cva6
} // namespace gem5

#endif /* __CPU_CVA6_ACTIVITY_HH__ */
