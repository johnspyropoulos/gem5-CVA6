#ifndef __CPU_CVA6_FUNC_UNIT_HH__
#define __CPU_CVA6_FUNC_UNIT_HH__

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#include "base/types.hh"
#include "cpu/cva6/buffers.hh"
#include "cpu/cva6/dyn_inst.hh"
#include "cpu/func_unit.hh"
#include "cpu/timing_expr.hh"
#include "params/CVA6FU.hh"
#include "params/CVA6FUPool.hh"
#include "params/CVA6OpClass.hh"
#include "params/CVA6OpClassSet.hh"
#include "sim/clocked_object.hh"
#include "sim/sim_object.hh"

namespace gem5
{

/** Boxing for CVA6OpClass to get around a build problem with C++11 but
 *  also allow for future additions to op class checking */
class CVA6OpClass : public SimObject
{
  public:
    OpClass opClass;

  public:
    CVA6OpClass(const CVA6OpClassParams &params) :
        SimObject(params),
        opClass(params.opClass)
    { }
};

/** Wrapper for a matchable set of op classes */
class CVA6OpClassSet : public SimObject
{
  public:
    std::vector<CVA6OpClass *> opClasses;

    /** Convenience packing of opClasses into a bit vector for easier
     *  testing */
    std::vector<bool> capabilityList;

  public:
    CVA6OpClassSet(const CVA6OpClassSetParams &params);

  public:
    /** Does this set support the given op class */
    bool provides(OpClass op_class) { return capabilityList[op_class]; }
};

/** Extra timing capability to allow individual ops to have their source
 *  register dependency latencies tweaked based on the ExtMachInst of the
 *  source instruction.
 */
class CVA6FUTiming: public SimObject
{
  public:
    /** Mask off the ExtMachInst of an instruction before comparing with
     *  match */
    uint64_t mask;
    uint64_t match;

    /** Textual description of the decode's purpose */
    std::string description;

    /** If true, instructions matching this mask/match should *not* be
     *  issued in this FU */
    bool suppress;

    /** Extra latency that the instruction should spend at the end of
     *  the pipeline */
    Cycles extraCommitLat;
    TimingExpr *extraCommitLatExpr;

    /** Extra delay that results should show in the scoreboard after
     *  leaving the pipeline.  If set to Cycles(0) for memory references,
     *  an 'unpredictable' return time will be set in the scoreboard
     *  blocking following dependent instructions from issuing */
    Cycles extraAssumedLat;

    /** Cycle offsets from the scoreboard delivery times of register values
     *  for each of this instruction's source registers (in srcRegs order).
     *  The offsets are subtracted from the scoreboard returnCycle times.
     *  For example, for an instruction type with 3 source registers,
     *  [2, 1, 2] will allow the instruction to issue upto 2 cycles early
     *  for dependencies on the 1st and 3rd register and upto 1 cycle early
     *  on the 2nd. */
    std::vector<Cycles> srcRegsRelativeLats;

    /** Extra opClasses check (after the FU one) */
    CVA6OpClassSet *opClasses;

  public:
    CVA6FUTiming(const CVA6FUTimingParams &params);

  public:
    /** Does the extra decode in this object support the given op class */
    bool provides(OpClass op_class) { return opClasses->provides(op_class); }
};

/** A functional unit that can execute any of opClasses operations with a
 *  single op(eration)Lat(ency) and issueLat(ency) associated with the unit
 *  rather than each operation (as in src/FuncUnit).
 *
 *  This is very similar to cpu/func_unit but replicated here to allow
 *  the CVA6 functional units to change without having to disturb the common
 *  definition.
 */
class CVA6FU : public SimObject
{
  public:
    CVA6OpClassSet *opClasses;

    /** Delay from issuing the operation, to it reaching the
     *  end of the associated pipeline */
    Cycles opLat;

    /** Delay after issuing an operation before the next
     *  operation can be issued */
    Cycles issueLat;

    /** FUs which this pipeline can't receive a forwarded (i.e. relative
     *  latency != 0) result from */
    std::vector<unsigned int> cantForwardFromFUIndices;

    /** Extra timing info to give timings to individual ops */
    std::vector<CVA6FUTiming *> timings;

  public:
    CVA6FU(const CVA6FUParams &params) :
        SimObject(params),
        opClasses(params.opClasses),
        opLat(params.opLat),
        issueLat(params.issueLat),
        cantForwardFromFUIndices(params.cantForwardFromFUIndices),
        timings(params.timings)
    { }
};

/** A collection of CVA6FUs */
class CVA6FUPool : public SimObject
{
  public:
    std::vector<CVA6FU *> funcUnits;

  public:
    CVA6FUPool(const CVA6FUPoolParams &params) :
        SimObject(params),
        funcUnits(params.funcUnits)
    { }
};

namespace cva6
{

/** Container class to box instructions in the FUs to make those
 *  queues have correct bubble behaviour when stepped */
class QueuedInst
{
  public:
    CVA6DynInstPtr inst;

  public:
    QueuedInst(CVA6DynInstPtr inst_ = CVA6DynInst::bubble()) :
        inst(inst_)
    { }

  public:
    /** Report and bubble interfaces */
    void reportData(std::ostream &os) const;
    bool isBubble() const { return inst->isBubble(); }

    static QueuedInst bubble()
    { return QueuedInst(CVA6DynInst::bubble()); }
};

/** Functional units have pipelines which stall when an inst gets to
 *  their ends allowing Execute::commit to pick up timing-completed insts
 *  when it feels like it */
typedef SelfStallingPipeline<QueuedInst,
    ReportTraitsAdaptor<QueuedInst> > FUPipelineBase;

/** A functional unit configured from a CVA6FU object */
class FUPipeline : public FUPipelineBase, public FuncUnit
{
  public:
    /** Functional unit description that this pipeline implements */
    const CVA6FU &description;

    /** An FUPipeline needs access to curCycle, use this timing source */
    ClockedObject &timeSource;

    /** Set of operation classes supported by this FU */
    std::bitset<Num_OpClasses> capabilityList;

    /** FUs which this pipeline can't receive a forwarded (i.e. relative
     *  latency != 0) result from */
    std::vector<bool> cantForwardFromFUIndices;

  public:
    /** When can a new instruction be inserted into the pipeline?  This is
     *  an absolute cycle time unless it is 0 in which case the an
     *  instruction can be pushed straightaway */
    Cycles nextInsertCycle;

  public:
    FUPipeline(const std::string &name, const CVA6FU &description_,
        ClockedObject &timeSource_);

  public:
    /** How many cycles must from curCycle before insertion into the
     *  pipeline is allowed */
    Cycles cyclesBeforeInsert();

    /** Can an instruction be inserted now? */
    bool canInsert() const;

    /** Find the extra timing information for this instruction.  Returns
     *  NULL if no decode info. is found */
    CVA6FUTiming *findTiming(const StaticInstPtr &inst);

    /** Step the pipeline.  Allow multiple steps? */
    void advance();
};

} // namespace cva6
} // namespace gem5

#endif /* __CPU_CVA6_FUNC_UNIT_HH__ */
