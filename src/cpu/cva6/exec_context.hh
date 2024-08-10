/**
 * @file
 *
 *  ExecContext bears the exec_context interface for CVA6.
 */

#ifndef __CPU_CVA6_EXEC_CONTEXT_HH__
#define __CPU_CVA6_EXEC_CONTEXT_HH__

#include "cpu/base.hh"
#include "cpu/cva6/execute.hh"
#include "cpu/cva6/pipeline.hh"
#include "cpu/exec_context.hh"
#include "cpu/simple_thread.hh"
#include "debug/CVA6Execute.hh"
#include "mem/request.hh"

namespace gem5
{

namespace cva6
{

/* Forward declaration of Execute */
class Execute;

/** ExecContext bears the exec_context interface for CVA6.  This nicely
 *  separates that interface from other classes such as Pipeline, CVA6CPU
 *  and DynCVA6Inst and makes it easier to see what state is accessed by it.
 */
class ExecContext : public gem5::ExecContext
{
  public:
    CVA6CPU &cpu;

    /** ThreadState object, provides all the architectural state. */
    SimpleThread &thread;

    /** The execute stage so we can peek at its contents. */
    Execute &execute;

    /** Instruction for the benefit of memory operations and for PC */
    CVA6DynInstPtr inst;

    ExecContext (
        CVA6CPU &cpu_,
        SimpleThread &thread_, Execute &execute_,
        CVA6DynInstPtr inst_) :
        cpu(cpu_),
        thread(thread_),
        execute(execute_),
        inst(inst_)
    {
        DPRINTF(CVA6Execute, "ExecContext setting PC: %s\n", *inst->pc);
        pcState(*inst->pc);
        setPredicate(inst->readPredicate());
        setMemAccPredicate(inst->readMemAccPredicate());
    }

    ~ExecContext()
    {
        inst->setPredicate(readPredicate());
        inst->setMemAccPredicate(readMemAccPredicate());
    }

    Fault
    initiateMemRead(Addr addr, unsigned int size,
                    Request::Flags flags,
                    const std::vector<bool>& byte_enable) override
    {
        assert(byte_enable.size() == size);
        return execute.getLSQ().pushRequest(inst, true /* load */, nullptr,
            size, addr, flags, nullptr, nullptr, byte_enable);
    }

    Fault
    initiateMemMgmtCmd(Request::Flags flags) override
    {
        panic("ExecContext::initiateMemMgmtCmd() not implemented "
              " on CVA6CPU\n");
        return NoFault;
    }

    Fault
    writeMem(uint8_t *data, unsigned int size, Addr addr,
             Request::Flags flags, uint64_t *res,
             const std::vector<bool>& byte_enable)
        override
    {
        assert(byte_enable.size() == size);
        return execute.getLSQ().pushRequest(inst, false /* store */, data,
            size, addr, flags, res, nullptr, byte_enable);
    }

    Fault
    initiateMemAMO(Addr addr, unsigned int size, Request::Flags flags,
                   AtomicOpFunctorPtr amo_op) override
    {
        // AMO requests are pushed through the store path
        return execute.getLSQ().pushRequest(inst, false /* amo */, nullptr,
            size, addr, flags, nullptr, std::move(amo_op),
            std::vector<bool>(size, true));
    }

    RegVal
    getRegOperand(const StaticInst *si, int idx) override
    {
        const RegId &reg = si->srcRegIdx(idx);
        if (reg.is(InvalidRegClass))
            return 0;
        return thread.getReg(reg);
    }

    void
    getRegOperand(const StaticInst *si, int idx, void *val) override
    {
        thread.getReg(si->srcRegIdx(idx), val);
    }

    void *
    getWritableRegOperand(const StaticInst *si, int idx) override
    {
        return thread.getWritableReg(si->destRegIdx(idx));
    }

    void
    setRegOperand(const StaticInst *si, int idx, RegVal val) override
    {
        const RegId &reg = si->destRegIdx(idx);
        if (reg.is(InvalidRegClass))
            return;
        thread.setReg(si->destRegIdx(idx), val);
    }

    void
    setRegOperand(const StaticInst *si, int idx, const void *val) override
    {
        thread.setReg(si->destRegIdx(idx), val);
    }

    bool
    readPredicate() const override
    {
        return thread.readPredicate();
    }

    void
    setPredicate(bool val) override
    {
        thread.setPredicate(val);
    }

    bool
    readMemAccPredicate() const override
    {
        return thread.readMemAccPredicate();
    }

    void
    setMemAccPredicate(bool val) override
    {
        thread.setMemAccPredicate(val);
    }

    // hardware transactional memory
    uint64_t
    getHtmTransactionUid() const override
    {
        panic("ExecContext::getHtmTransactionUid() not"
              "implemented on CVA6CPU\n");
        return 0;
    }

    uint64_t
    newHtmTransactionUid() const override
    {
        panic("ExecContext::newHtmTransactionUid() not"
              "implemented on CVA6CPU\n");
        return 0;
    }

    bool
    inHtmTransactionalState() const override
    {
        // ExecContext::inHtmTransactionalState() not
        // implemented on CVA6CPU
        return false;
    }

    uint64_t
    getHtmTransactionalDepth() const override
    {
        panic("ExecContext::getHtmTransactionalDepth() not"
              "implemented on CVA6CPU\n");
        return 0;
    }

    const PCStateBase &
    pcState() const override
    {
        return thread.pcState();
    }

    void
    pcState(const PCStateBase &val) override
    {
        thread.pcState(val);
    }

    RegVal
    readMiscRegNoEffect(int misc_reg) const
    {
        return thread.readMiscRegNoEffect(misc_reg);
    }

    RegVal
    readMiscReg(int misc_reg) override
    {
        return thread.readMiscReg(misc_reg);
    }

    void
    setMiscReg(int misc_reg, RegVal val) override
    {
        thread.setMiscReg(misc_reg, val);
    }

    RegVal
    readMiscRegOperand(const StaticInst *si, int idx) override
    {
        const RegId& reg = si->srcRegIdx(idx);
        assert(reg.is(MiscRegClass));
        return thread.readMiscReg(reg.index());
    }

    void
    setMiscRegOperand(const StaticInst *si, int idx, RegVal val) override
    {
        const RegId& reg = si->destRegIdx(idx);
        assert(reg.is(MiscRegClass));
        return thread.setMiscReg(reg.index(), val);
    }

    ThreadContext *tcBase() const override { return thread.getTC(); }

    /* @todo, should make stCondFailures persistent somewhere */
    unsigned int readStCondFailures() const override { return 0; }
    void setStCondFailures(unsigned int st_cond_failures) override {}

    ContextID contextId() { return thread.contextId(); }
    /* ISA-specific (or at least currently ISA singleton) functions */

    /* X86: TLB twiddling */
    void
    demapPage(Addr vaddr, uint64_t asn) override
    {
        thread.getMMUPtr()->demapPage(vaddr, asn);
    }

    BaseCPU *getCpuPtr() { return &cpu; }

  public:
    // monitor/mwait funtions
    void
    armMonitor(Addr address) override
    {
        getCpuPtr()->armMonitor(inst->id.threadId, address);
    }

    bool
    mwait(PacketPtr pkt) override
    {
        return getCpuPtr()->mwait(inst->id.threadId, pkt);
    }

    void
    mwaitAtomic(ThreadContext *tc) override
    {
        return getCpuPtr()->mwaitAtomic(inst->id.threadId, tc, thread.mmu);
    }

    AddressMonitor *
    getAddrMonitor() override
    {
        return getCpuPtr()->getCpuAddrMonitor(inst->id.threadId);
    }
};

} // namespace cva6
} // namespace gem5

#endif /* __CPU_CVA6_EXEC_CONTEXT_HH__ */
