#include "cpu/cva6/func_unit.hh"

#include <iomanip>
#include <sstream>
#include <typeinfo>

#include "base/named.hh"
#include "base/trace.hh"
#include "debug/CVA6Timing.hh"
#include "enums/OpClass.hh"

namespace gem5
{

CVA6OpClassSet::CVA6OpClassSet(const CVA6OpClassSetParams &params) :
    SimObject(params),
    opClasses(params.opClasses),
    /* Initialise to true for an empty list so that 'fully capable' is
     *  the default */
    capabilityList(Num_OpClasses, (opClasses.empty() ? true : false))
{
    for (unsigned int i = 0; i < opClasses.size(); i++)
        capabilityList[opClasses[i]->opClass] = true;
}

CVA6FUTiming::CVA6FUTiming(
    const CVA6FUTimingParams &params) :
    SimObject(params),
    mask(params.mask),
    match(params.match),
    description(params.description),
    suppress(params.suppress),
    extraCommitLat(params.extraCommitLat),
    extraCommitLatExpr(params.extraCommitLatExpr),
    extraAssumedLat(params.extraAssumedLat),
    srcRegsRelativeLats(params.srcRegsRelativeLats),
    opClasses(params.opClasses)
{ }

namespace cva6
{

void
QueuedInst::reportData(std::ostream &os) const
{
    inst->reportData(os);
}

FUPipeline::FUPipeline(const std::string &name, const CVA6FU &description_,
    ClockedObject &timeSource_) :
    FUPipelineBase(name, "insts", description_.opLat),
    description(description_),
    timeSource(timeSource_),
    nextInsertCycle(Cycles(0))
{
    /* Issue latencies are set to 1 in calls to addCapability here.
     * Issue latencies are associated with the pipeline as a whole,
     * rather than instruction classes in CVA6 */

    /* All pipelines should be able to execute No_OpClass instructions */
    addCapability(No_OpClass, description.opLat, 1);

    /* Add the capabilities listed in the CVA6FU for this functional unit */
    for (unsigned int i = 0; i < description.opClasses->opClasses.size();
         i++)
    {
        addCapability(description.opClasses->opClasses[i]->opClass,
            description.opLat, 1);
    }

    for (unsigned int i = 0; i < description.timings.size(); i++) {
        CVA6FUTiming &timing = *(description.timings[i]);

        if (debug::CVA6Timing) {
            std::ostringstream lats;

            unsigned int num_lats = timing.srcRegsRelativeLats.size();
            unsigned int j = 0;
            while (j < num_lats) {
                lats << timing.srcRegsRelativeLats[j];

                j++;
                if (j != num_lats)
                    lats << ',';
            }

            DPRINTFS(CVA6Timing, static_cast<Named *>(this),
                "Adding extra timing decode pattern %d to FU"
                " mask: %016x match: %016x srcRegLatencies: %s\n",
                i, timing.mask, timing.match, lats.str());
        }
    }

    const std::vector<unsigned> &cant_forward =
        description.cantForwardFromFUIndices;

    /* Setup the bit vector cantForward... with the set indices
     *  specified in the parameters */
    for (auto i = cant_forward.begin(); i != cant_forward.end(); ++i) {
        cantForwardFromFUIndices.resize((*i) + 1, false);
        cantForwardFromFUIndices[*i] = true;
    }
}

Cycles
FUPipeline::cyclesBeforeInsert()
{
    if (nextInsertCycle == 0 || timeSource.curCycle() > nextInsertCycle)
        return Cycles(0);
    else
        return nextInsertCycle - timeSource.curCycle();
}

bool
FUPipeline::canInsert() const
{
    return nextInsertCycle == 0 || timeSource.curCycle() >= nextInsertCycle;
}

void
FUPipeline::advance()
{
    bool was_stalled = stalled;

    /* If an instruction was pushed into the pipeline, set the delay before
     *  the next instruction can follow */
    if (alreadyPushed()) {
        if (nextInsertCycle <= timeSource.curCycle()) {
            nextInsertCycle = timeSource.curCycle() + description.issueLat;
        }
    } else if (was_stalled && nextInsertCycle != 0) {
        /* Don't count stalled cycles as part of the issue latency */
        ++nextInsertCycle;
    }
    FUPipelineBase::advance();
}

CVA6FUTiming *
FUPipeline::findTiming(const StaticInstPtr &inst)
{
    /*
     * This will only work on ISAs with an instruction format with a fixed size
     * which can be categorized using bit masks. This is really only supported
     * on ARM and is a bit of a hack.
     */
    uint64_t mach_inst = inst->getEMI();

    const std::vector<CVA6FUTiming *> &timings =
        description.timings;
    unsigned int num_timings = timings.size();

    for (unsigned int i = 0; i < num_timings; i++) {
        CVA6FUTiming &timing = *timings[i];

        if (timing.provides(inst->opClass()) &&
            (mach_inst & timing.mask) == timing.match)
        {
            DPRINTFS(CVA6Timing, static_cast<Named *>(this),
                "Found extra timing match (pattern %d '%s')"
                " %s %16x (type %s)\n",
                i, timing.description, inst->disassemble(0), mach_inst,
                typeid(inst).name());

            return &timing;
        }
    }

    if (num_timings != 0) {
        DPRINTFS(CVA6Timing, static_cast<Named *>(this),
            "No extra timing info. found for inst: %s"
            " mach_inst: %16x\n",
            inst->disassemble(0), mach_inst);
    }

    return NULL;
}

} // namespace cva6
} // namespace gem5
