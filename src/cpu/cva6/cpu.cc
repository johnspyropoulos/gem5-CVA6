#include "cpu/cva6/cpu.hh"

#include "cpu/cva6/dyn_inst.hh"
#include "cpu/cva6/fetch1.hh"
#include "cpu/cva6/pipeline.hh"
#include "debug/CVA6CPU.hh"
#include "debug/Drain.hh"
#include "debug/Quiesce.hh"

namespace gem5
{

CVA6CPU::CVA6CPU(const BaseCVA6CPUParams &params) :
    BaseCPU(params),
    threadPolicy(params.threadPolicy),
    stats(this)
{
    /* This is only written for one thread at the moment */
    cva6::CVA6Thread *thread;

    for (ThreadID i = 0; i < numThreads; i++) {
        if (FullSystem) {
            thread = new cva6::CVA6Thread(this, i, params.system,
                    params.mmu, params.isa[i], params.decoder[i]);
            thread->setStatus(ThreadContext::Halted);
        } else {
            thread = new cva6::CVA6Thread(this, i, params.system,
                    params.workload[i], params.mmu,
                    params.isa[i], params.decoder[i]);
        }

        threads.push_back(thread);
        ThreadContext *tc = thread->getTC();
        threadContexts.push_back(tc);
    }


    if (params.checker) {
        fatal("The CVA6 model doesn't support checking (yet)\n");
    }

    pipeline = new cva6::Pipeline(*this, params);
    activityRecorder = pipeline->getActivityRecorder();

    fetchEventWrapper = NULL;
}

CVA6CPU::~CVA6CPU()
{
    delete pipeline;

    if (fetchEventWrapper != NULL)
        delete fetchEventWrapper;

    for (ThreadID thread_id = 0; thread_id < threads.size(); thread_id++) {
        delete threads[thread_id];
    }
}

void
CVA6CPU::init()
{
    BaseCPU::init();

    if (!params().switched_out && system->getMemoryMode() != enums::timing) {
        fatal("The CVA6 CPU requires the memory system to be in "
            "'timing' mode.\n");
    }
}

/** Stats interface from SimObject (by way of BaseCPU) */
void
CVA6CPU::regStats()
{
    BaseCPU::regStats();
    pipeline->regStats();
}

void
CVA6CPU::serializeThread(CheckpointOut &cp, ThreadID thread_id) const
{
    threads[thread_id]->serialize(cp);
}

void
CVA6CPU::unserializeThread(CheckpointIn &cp, ThreadID thread_id)
{
    threads[thread_id]->unserialize(cp);
}

void
CVA6CPU::serialize(CheckpointOut &cp) const
{
    pipeline->serialize(cp);
    BaseCPU::serialize(cp);
}

void
CVA6CPU::unserialize(CheckpointIn &cp)
{
    pipeline->unserialize(cp);
    BaseCPU::unserialize(cp);
}

void
CVA6CPU::wakeup(ThreadID tid)
{
    DPRINTF(Drain, "[tid:%d] CVA6CPU wakeup\n", tid);
    assert(tid < numThreads);

    if (threads[tid]->status() == ThreadContext::Suspended) {
        threads[tid]->activate();
    }
}

void
CVA6CPU::startup()
{
    DPRINTF(CVA6CPU, "CVA6CPU startup\n");

    BaseCPU::startup();

    for (ThreadID tid = 0; tid < numThreads; tid++)
        pipeline->wakeupFetch(tid);
}

DrainState
CVA6CPU::drain()
{
    // Deschedule any power gating event (if any)
    deschedulePowerGatingEvent();

    if (switchedOut()) {
        DPRINTF(Drain, "CVA6 CPU switched out, draining not needed.\n");
        return DrainState::Drained;
    }

    DPRINTF(Drain, "CVA6CPU drain\n");

    /* Need to suspend all threads and wait for Execute to idle.
     * Tell Fetch1 not to fetch */
    if (pipeline->drain()) {
        DPRINTF(Drain, "CVA6CPU drained\n");
        return DrainState::Drained;
    } else {
        DPRINTF(Drain, "CVA6CPU not finished draining\n");
        return DrainState::Draining;
    }
}

void
CVA6CPU::signalDrainDone()
{
    DPRINTF(Drain, "CVA6CPU drain done\n");
    Drainable::signalDrainDone();
}

void
CVA6CPU::drainResume()
{
    /* When taking over from another cpu make sure lastStopped
     * is reset since it might have not been defined previously
     * and might lead to a stats corruption */
    pipeline->resetLastStopped();

    if (switchedOut()) {
        DPRINTF(Drain, "drainResume while switched out.  Ignoring\n");
        return;
    }

    DPRINTF(Drain, "CVA6CPU drainResume\n");

    if (!system->isTimingMode()) {
        fatal("The CVA6 CPU requires the memory system to be in "
            "'timing' mode.\n");
    }

    for (ThreadID tid = 0; tid < numThreads; tid++){
        wakeup(tid);
    }

    pipeline->drainResume();

    // Reschedule any power gating event (if any)
    schedulePowerGatingEvent();
}

void
CVA6CPU::memWriteback()
{
    DPRINTF(Drain, "CVA6CPU memWriteback\n");
}

void
CVA6CPU::switchOut()
{
    DPRINTF(CVA6CPU, "CVA6CPU switchOut\n");

    assert(!switchedOut());
    BaseCPU::switchOut();

    /* Check that the CPU is drained? */
    activityRecorder->reset();
}

void
CVA6CPU::takeOverFrom(BaseCPU *old_cpu)
{
    DPRINTF(CVA6CPU, "CVA6CPU takeOverFrom\n");

    BaseCPU::takeOverFrom(old_cpu);
}

void
CVA6CPU::activateContext(ThreadID thread_id)
{
    DPRINTF(CVA6CPU, "ActivateContext thread: %d\n", thread_id);

    /* Do some cycle accounting.  lastStopped is reset to stop the
     *  wakeup call on the pipeline from adding the quiesce period
     *  to BaseCPU::numCycles */
    stats.quiesceCycles += pipeline->cyclesSinceLastStopped();
    pipeline->resetLastStopped();

    /* Wake up the thread, wakeup the pipeline tick */
    threads[thread_id]->activate();
    wakeupOnEvent(cva6::Pipeline::CPUStageId);

    if (!threads[thread_id]->getUseForClone())//the thread is not cloned
    {
        pipeline->wakeupFetch(thread_id);
    } else { //the thread from clone
        if (fetchEventWrapper != NULL)
            delete fetchEventWrapper;
        fetchEventWrapper = new EventFunctionWrapper([this, thread_id]
                  { pipeline->wakeupFetch(thread_id); }, "wakeupFetch");
        schedule(*fetchEventWrapper, clockEdge(Cycles(0)));
    }

    BaseCPU::activateContext(thread_id);
}

void
CVA6CPU::suspendContext(ThreadID thread_id)
{
    DPRINTF(CVA6CPU, "SuspendContext %d\n", thread_id);

    threads[thread_id]->suspend();

    BaseCPU::suspendContext(thread_id);
}

void
CVA6CPU::wakeupOnEvent(unsigned int stage_id)
{
    DPRINTF(Quiesce, "Event wakeup from stage %d\n", stage_id);

    /* Mark that some activity has taken place and start the pipeline */
    activityRecorder->activateStage(stage_id);
    pipeline->start();
}

Port &
CVA6CPU::getInstPort()
{
    return pipeline->getInstPort();
}

Port &
CVA6CPU::getDataPort()
{
    return pipeline->getDataPort();
}

Counter
CVA6CPU::totalInsts() const
{
    Counter ret = 0;

    for (auto i = threads.begin(); i != threads.end(); i ++)
        ret += (*i)->numInst;

    return ret;
}

Counter
CVA6CPU::totalOps() const
{
    Counter ret = 0;

    for (auto i = threads.begin(); i != threads.end(); i ++)
        ret += (*i)->numOp;

    return ret;
}

} // namespace gem5
