#include "cpu/cva6/scoreboard.hh"

#include "cpu/reg_class.hh"
#include "debug/CVA6Scoreboard.hh"
#include "debug/CVA6Timing.hh"

namespace gem5
{

namespace cva6
{

bool
Scoreboard::findIndex(const RegId& reg, Index &scoreboard_index)
{
    bool ret = false;

    switch (reg.classValue()) {
      case IntRegClass:
        scoreboard_index = reg.index();
        ret = true;
        break;
      case FloatRegClass:
        scoreboard_index = floatRegOffset + reg.index();
        ret = true;
        break;
      case VecRegClass:
        scoreboard_index = vecRegOffset + reg.index();
        ret = true;
        break;
      case VecElemClass:
        scoreboard_index = vecRegElemOffset + reg.index();
        ret = true;
        break;
      case VecPredRegClass:
        scoreboard_index = vecPredRegOffset + reg.index();
        ret = true;
        break;
      case MatRegClass:
        scoreboard_index = matRegOffset + reg.index();
        ret = true;
        break;
      case CCRegClass:
        scoreboard_index = ccRegOffset + reg.index();
        ret = true;
        break;
      case MiscRegClass:
          /* Don't bother with Misc registers */
        ret = false;
        break;
      case InvalidRegClass:
        ret = false;
        break;
      default:
        panic("Unknown register class: %d", reg.classValue());
    }

    return ret;
}

void
Scoreboard::markupInstDests(CVA6DynInstPtr inst, Cycles retire_time,
    ThreadContext *thread_context, bool mark_unpredictable)
{
    if (inst->isFault())
        return;

    StaticInstPtr staticInst = inst->staticInst;
    unsigned int num_dests = staticInst->numDestRegs();

    auto *isa = thread_context->getIsaPtr();

    /** Mark each destination register */
    for (unsigned int dest_index = 0; dest_index < num_dests;
        dest_index++)
    {
        RegId reg = staticInst->destRegIdx(dest_index).flatten(*isa);
        Index index;

        if (findIndex(reg, index)) {
            if (mark_unpredictable)
                numUnpredictableResults[index]++;

            inst->flatDestRegIdx[dest_index] = reg;

            numResults[index]++;
            returnCycle[index] = retire_time;
            /* We should be able to rely on only being given accending
             *  execSeqNums, but sanity check */
            if (inst->id.execSeqNum > writingInst[index]) {
                writingInst[index] = inst->id.execSeqNum;
                fuIndices[index] = inst->fuIndex;
            }

            DPRINTF(CVA6Scoreboard, "Marking up inst: %s"
                " regIndex: %d final numResults: %d returnCycle: %d\n",
                *inst, index, numResults[index], returnCycle[index]);
        } else {
            /* Use an invalid ID to mark invalid/untracked dests */
            inst->flatDestRegIdx[dest_index] = RegId();
        }
    }
}

InstSeqNum
Scoreboard::execSeqNumToWaitFor(CVA6DynInstPtr inst,
    ThreadContext *thread_context)
{
    InstSeqNum ret = 0;

    if (inst->isFault())
        return ret;

    StaticInstPtr staticInst = inst->staticInst;
    unsigned int num_srcs = staticInst->numSrcRegs();

    auto *isa = thread_context->getIsaPtr();

    for (unsigned int src_index = 0; src_index < num_srcs; src_index++) {
        RegId reg = staticInst->srcRegIdx(src_index).flatten(*isa);
        unsigned short int index;

        if (findIndex(reg, index)) {
            if (writingInst[index] > ret)
                ret = writingInst[index];
        }
    }

    DPRINTF(CVA6Scoreboard, "Inst: %s depends on execSeqNum: %d\n",
        *inst, ret);

    return ret;
}

void
Scoreboard::clearInstDests(CVA6DynInstPtr inst, bool clear_unpredictable)
{
    if (inst->isFault())
        return;

    StaticInstPtr staticInst = inst->staticInst;
    unsigned int num_dests = staticInst->numDestRegs();

    /** Mark each destination register */
    for (unsigned int dest_index = 0; dest_index < num_dests;
        dest_index++)
    {
        const RegId& reg = inst->flatDestRegIdx[dest_index];
        Index index;

        if (findIndex(reg, index)) {
            if (clear_unpredictable && numUnpredictableResults[index] != 0)
                numUnpredictableResults[index] --;

            numResults[index] --;

            if (numResults[index] == 0) {
                returnCycle[index] = Cycles(0);
                writingInst[index] = 0;
                fuIndices[index] = invalidFUIndex;
            }

            DPRINTF(CVA6Scoreboard, "Clearing inst: %s"
                " regIndex: %d final numResults: %d\n",
                *inst, index, numResults[index]);
        }
    }
}

bool
Scoreboard::canInstIssue(CVA6DynInstPtr inst,
    const std::vector<Cycles> *src_reg_relative_latencies,
    const std::vector<bool> *cant_forward_from_fu_indices,
    Cycles now, ThreadContext *thread_context)
{
    /* Always allow fault to be issued */
    if (inst->isFault())
        return true;

    StaticInstPtr staticInst = inst->staticInst;
    unsigned int num_srcs = staticInst->numSrcRegs();

    /* Default to saying you can issue */
    bool ret = true;

    unsigned int num_relative_latencies = 0;
    Cycles default_relative_latency = Cycles(0);

    /* Where relative latencies are given, the default is the last
     *  one as that allows the rel. lat. list to be shorted than the
     *  number of src. regs */
    if (src_reg_relative_latencies &&
        src_reg_relative_latencies->size() != 0)
    {
        num_relative_latencies = src_reg_relative_latencies->size();
        default_relative_latency = (*src_reg_relative_latencies)
            [num_relative_latencies-1];
    }

    auto *isa = thread_context->getIsaPtr();

    /* For each source register, find the latest result */
    unsigned int src_index = 0;
    while (src_index < num_srcs && /* More registers */
        ret /* Still possible */)
    {
        RegId reg = staticInst->srcRegIdx(src_index).flatten(*isa);
        unsigned short int index;

        if (findIndex(reg, index)) {
            int src_reg_fu = fuIndices[index];
            bool cant_forward = src_reg_fu != invalidFUIndex &&
                cant_forward_from_fu_indices &&
                src_reg_fu < cant_forward_from_fu_indices->size() &&
                (*cant_forward_from_fu_indices)[src_reg_fu];

            Cycles relative_latency = (cant_forward ? Cycles(0) :
                (src_index >= num_relative_latencies ?
                    default_relative_latency :
                    (*src_reg_relative_latencies)[src_index]));

            if (returnCycle[index] > (now + relative_latency) ||
                numUnpredictableResults[index] != 0)
            {
                ret = false;
            }
        }
        src_index++;
    }

    if (debug::CVA6Timing) {
        if (ret && num_srcs > num_relative_latencies &&
            num_relative_latencies != 0)
        {
            DPRINTF(CVA6Timing, "Warning, inst: %s timing extra decode has"
                " more src. regs: %d than relative latencies: %d\n",
                staticInst->disassemble(0), num_srcs, num_relative_latencies);
        }
    }

    return ret;
}

void
Scoreboard::cva6Trace() const
{
    std::ostringstream result_stream;

    bool printed_element = false;

    unsigned int i = 0;
    while (i < numRegs) {
        unsigned short int num_results = numResults[i];
        unsigned short int num_unpredictable_results =
            numUnpredictableResults[i];

        if (!(num_results == 0 && num_unpredictable_results == Cycles(0))) {
            if (printed_element)
                result_stream << ',';

            result_stream << '(' << i << ','
                 << num_results << '/'
                 << num_unpredictable_results << '/'
                 << returnCycle[i] << '/'
                 << writingInst[i] << ')';

            printed_element = true;
        }

        i++;
    }

    cva6::cva6Trace("busy=%s\n", result_stream.str());
}

} // namespace cva6
} // namespace gem5
