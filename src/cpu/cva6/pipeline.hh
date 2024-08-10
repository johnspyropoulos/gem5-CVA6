/**
 * @file
 *
 *  The constructed pipeline.  Kept out of CVA6CPU to keep the interface
 *  between the CPU and its grubby implementation details clean.
 */

#ifndef __CPU_CVA6_PIPELINE_HH__
#define __CPU_CVA6_PIPELINE_HH__

#include "cpu/cva6/activity.hh"
#include "cpu/cva6/cpu.hh"
#include "cpu/cva6/decode.hh"
#include "cpu/cva6/execute.hh"
#include "cpu/cva6/fetch1.hh"
#include "cpu/cva6/fetch2.hh"
#include "params/BaseCVA6CPU.hh"
#include "sim/ticked_object.hh"

namespace gem5
{

namespace cva6
{

/**
 * @namespace cva6
 *
 * CVA6 contains all the definitions within the CVA6CPU apart from the CPU
 * class itself
 */

/** The constructed pipeline.  Kept out of CVA6CPU to keep the interface
 *  between the CPU and its grubby implementation details clean. */
class Pipeline : public Ticked
{
  protected:
    CVA6CPU &cpu;

    /** Allow cycles to be skipped when the pipeline is idle */
    bool allow_idling;

    Latch<ForwardLineData> f1ToF2;
    Latch<BranchData> f2ToF1;
    Latch<ForwardInstData> f2ToD;
    Latch<ForwardInstData> dToE;
    Latch<BranchData> eToF1;

    Execute execute;
    Decode decode;
    Fetch2 fetch2;
    Fetch1 fetch1;

    /** Activity recording for the pipeline.  This is access through the CPU
     *  by the pipeline stages but belongs to the Pipeline as it is the
     *  cleanest place to initialise it */
    CVA6ActivityRecorder activityRecorder;

  public:
    /** Enumerated ids of the 'stages' for the activity recorder */
    enum StageId
    {
        /* A stage representing wakeup of the whole processor */
        CPUStageId = 0,
        /* Real pipeline stages */
        Fetch1StageId, Fetch2StageId, DecodeStageId, ExecuteStageId,
        Num_StageId /* Stage count */
    };

    /** True after drain is called but draining isn't complete */
    bool needToSignalDrained;

  public:
    Pipeline(CVA6CPU &cpu_, const BaseCVA6CPUParams &params);

  public:
    /** Wake up the Fetch unit.  This is needed on thread activation esp.
     *  after quiesce wakeup */
    void wakeupFetch(ThreadID tid);

    /** Try to drain the CPU */
    bool drain();

    void drainResume();

    /** Test to see if the CPU is drained */
    bool isDrained();

    /** A custom evaluate allows report in the right place (between
     *  stages and pipeline advance) */
    void evaluate() override;

    void cva6Trace() const;

    /** Functions below here are BaseCPU operations passed on to pipeline
     *  stages */

    /** Return the IcachePort belonging to Fetch1 for the CPU */
    CVA6CPU::CVA6CPUPort &getInstPort();
    /** Return the DcachePort belonging to Execute for the CPU */
    CVA6CPU::CVA6CPUPort &getDataPort();

    /** To give the activity recorder to the CPU */
    CVA6ActivityRecorder *getActivityRecorder() { return &activityRecorder; }
};

} // namespace cva6
} // namespace gem5

#endif /* __CPU_CVA6_PIPELINE_HH__ */
