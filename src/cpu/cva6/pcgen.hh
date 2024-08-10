#ifndef __CPU_CVA6_PCGEN_HH__
#define __CPU_CVA6_PCGEN_HH__

#include "base/named.hh"
#include "cpu/cva6/buffers.hh"
#include "cpu/cva6/cpu.hh"
#include "cpu/cva6/pipe_data.hh"
#include "cpu/pred/bpred_unit.hh"
#include "params/BaseCVA6CPU.hh"

namespace gem5
{

namespace cva6
{

class PCGen : public Named
{
  protected:
    /** Pointer to the containing CPU */
    CVA6CPU &cpu;

    /** Branch predictor passed from Python configuration */
    branch_prediction::BPredUnit &branchPredictor;
};

} // namespace cva6

} // namespace gem5

#endif // __CPU_CVA6_PCGEN_HH__
