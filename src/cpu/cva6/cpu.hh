#ifndef __CPU_CVA6_CPU_HH__
#define __CPU_CVA6_CPU_HH__

#include "base/compiler.hh"
#include "base/random.hh"
#include "cpu/base.hh"
#include "cpu/cva6/activity.hh"
#include "cpu/cva6/stats.hh"
#include "cpu/simple_thread.hh"
#include "enums/ThreadPolicyCVA6.hh"
#include "params/BaseCVA6CPU.hh"

namespace gem5
{

namespace cva6
{

/** Forward declared to break the cyclic inclusion dependencies between
 *  pipeline and cpu */
class Pipeline;

/** CVA6 will use the SimpleThread state for now */
typedef SimpleThread CVA6Thread;

} // namespace cva6

/**
 *  CVA6CPU is an in-order CPU model with four fixed pipeline stages:
 *
 *  Fetch1 - fetches lines from memory
 *  Fetch2 - decomposes lines into macro-op instructions
 *  Decode - decomposes macro-ops into micro-ops
 *  Execute - executes those micro-ops
 *
 *  This pipeline is carried in the CVA6CPU::pipeline object.
 *  The exec_context interface is not carried by CVA6CPU but by
 *      cva6::ExecContext objects
 *  created by cva6::Execute.
 */
class CVA6CPU : public BaseCPU
{
  protected:
    /** pipeline is a container for the clockable pipeline stage objects.
     *  Elements of pipeline call TheISA to implement the model. */
    cva6::Pipeline *pipeline;

  public:
    /** Activity recording for pipeline.  This belongs to Pipeline but
     *  stages will access it through the CPU as the CVA6CPU object
     *  actually mediates idling behaviour */
    cva6::CVA6ActivityRecorder *activityRecorder;

    /** These are thread state-representing objects for this CPU.  If
     *  you need a ThreadContext for *any* reason, use
     *  threads[threadId]->getTC() */
    std::vector<cva6::CVA6Thread *> threads;

  public:
    /** Provide a non-protected base class for CVA6's Ports as derived
     *  classes are created by Fetch1 and Execute */
    class CVA6CPUPort : public RequestPort
    {
      public:
        /** The enclosing cpu */
        CVA6CPU &cpu;

      public:
        CVA6CPUPort(const std::string& name_, CVA6CPU &cpu_)
            : RequestPort(name_), cpu(cpu_)
        { }

    };

    /** Thread Scheduling Policy (RoundRobin, Random, etc) */
    enums::ThreadPolicyCVA6 threadPolicy;
  protected:
     /** Return a reference to the data port. */
    Port &getDataPort() override;

    /** Return a reference to the instruction port. */
    Port &getInstPort() override;

  public:
    CVA6CPU(const BaseCVA6CPUParams &params);

    ~CVA6CPU();

  public:
    /** Starting, waking and initialisation */
    void init() override;
    void startup() override;
    void wakeup(ThreadID tid) override;

    /** Processor-specific statistics */
    cva6::CVA6Stats stats;

    /** Stats interface from SimObject (by way of BaseCPU) */
    void regStats() override;

    /** Simple inst count interface from BaseCPU */
    Counter totalInsts() const override;
    Counter totalOps() const override;

    void serializeThread(CheckpointOut &cp, ThreadID tid) const override;
    void unserializeThread(CheckpointIn &cp, ThreadID tid) override;

    /** Serialize pipeline data */
    void serialize(CheckpointOut &cp) const override;
    void unserialize(CheckpointIn &cp) override;

    /** Drain interface */
    DrainState drain() override;
    void drainResume() override;
    /** Signal from Pipeline that CVA6CPU should signal that a drain
     *  is complete and set its drainState */
    void signalDrainDone();
    void memWriteback() override;

    /** Switching interface from BaseCPU */
    void switchOut() override;
    void takeOverFrom(BaseCPU *old_cpu) override;

    /** Thread activation interface from BaseCPU. */
    void activateContext(ThreadID thread_id) override;
    void suspendContext(ThreadID thread_id) override;

    /** Thread scheduling utility functions */
    std::vector<ThreadID> roundRobinPriority(ThreadID priority)
    {
        std::vector<ThreadID> prio_list;
        for (ThreadID i = 1; i <= numThreads; i++) {
            prio_list.push_back((priority + i) % numThreads);
        }
        return prio_list;
    }

    std::vector<ThreadID> randomPriority()
    {
        std::vector<ThreadID> prio_list;
        for (ThreadID i = 0; i < numThreads; i++) {
            prio_list.push_back(i);
        }

        std::shuffle(prio_list.begin(), prio_list.end(),
                     random_mt.gen);

        return prio_list;
    }

    /** The tick method in the CVA6CPU is simply updating the cycle
     * counters as the ticking of the pipeline stages is already
     * handled by the Pipeline object.
     */
    void tick() { updateCycleCounters(BaseCPU::CPU_STATE_ON); }

    /** Interface for stages to signal that they have become active after
     *  a callback or eventq event where the pipeline itself may have
     *  already been idled.  The stage argument should be from the
     *  enumeration Pipeline::StageId */
    void wakeupOnEvent(unsigned int stage_id);
    EventFunctionWrapper *fetchEventWrapper;
};

} // namespace cva6

#endif /* __CPU_CVA6_CPU_HH__ */
