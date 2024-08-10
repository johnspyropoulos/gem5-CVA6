from m5.defines import buildEnv
from m5.objects.BaseCPU import BaseCPU
from m5.objects.BranchPredictor import *
from m5.objects.DummyChecker import DummyChecker
from m5.objects.FuncUnit import OpClass
from m5.objects.TimingExpr import TimingExpr
from m5.params import *
from m5.proxy import *
from m5.SimObject import SimObject


class CVA6OpClass(SimObject):
    """Boxing of OpClass to get around build problems and provide a hook for
    future additions to OpClass checks"""

    type = "CVA6OpClass"
    cxx_header = "cpu/cva6/func_unit.hh"
    cxx_class = "gem5::CVA6OpClass"

    opClass = Param.OpClass("op class to match")


class CVA6OpClassSet(SimObject):
    """A set of matchable op classes"""

    type = "CVA6OpClassSet"
    cxx_header = "cpu/cva6/func_unit.hh"
    cxx_class = "gem5::CVA6OpClassSet"

    opClasses = VectorParam.CVA6OpClass(
        [], "op classes to be matched.  An empty list means any class"
    )


class CVA6FUTiming(SimObject):
    type = "CVA6FUTiming"
    cxx_header = "cpu/cva6/func_unit.hh"
    cxx_class = "gem5::CVA6FUTiming"

    mask = Param.UInt64(0, "mask for testing ExtMachInst")
    match = Param.UInt64(
        0,
        "match value for testing ExtMachInst:"
        " (ext_mach_inst & mask) == match",
    )
    suppress = Param.Bool(
        False, "if true, this inst. is not executed by this FU"
    )
    extraCommitLat = Param.Cycles(
        0, "extra cycles to stall commit for this inst."
    )
    extraCommitLatExpr = Param.TimingExpr(
        NULL, "extra cycles as a run-time evaluated expression"
    )
    extraAssumedLat = Param.Cycles(
        0,
        "extra cycles to add to scoreboard"
        " retire time for this insts dest registers once it leaves the"
        " functional unit.  For mem refs, if this is 0, the result's time"
        " is marked as unpredictable and no forwarding can take place.",
    )
    srcRegsRelativeLats = VectorParam.Cycles(
        "the maximum number of cycles"
        " after inst. issue that each src reg can be available for this"
        " inst. to issue"
    )
    opClasses = Param.CVA6OpClassSet(
        CVA6OpClassSet(),
        "op classes to be considered for this decode.  An empty set means any"
        " class",
    )
    description = Param.String(
        "", "description string of the decoding/inst class"
    )


def cva6MakeOpClassSet(op_classes):
    """Make a CVA6OpClassSet from a list of OpClass enum value strings"""

    def boxOpClass(op_class):
        return CVA6OpClass(opClass=op_class)

    return CVA6OpClassSet(opClasses=[boxOpClass(o) for o in op_classes])


class CVA6FU(SimObject):
    type = "CVA6FU"
    cxx_header = "cpu/cva6/func_unit.hh"
    cxx_class = "gem5::CVA6FU"

    opClasses = Param.CVA6OpClassSet(
        CVA6OpClassSet(),
        "type of operations allowed on this functional unit",
    )
    opLat = Param.Cycles(1, "latency in cycles")
    issueLat = Param.Cycles(
        1, "cycles until another instruction can be issued"
    )
    timings = VectorParam.CVA6FUTiming([], "extra decoding rules")

    cantForwardFromFUIndices = VectorParam.Unsigned(
        [],
        "list of FU indices from which this FU can't receive and early"
        " (forwarded) result",
    )


class CVA6FUPool(SimObject):
    type = "CVA6FUPool"
    cxx_header = "cpu/cva6/func_unit.hh"
    cxx_class = "gem5::CVA6FUPool"

    funcUnits = VectorParam.CVA6FU("functional units")


class CVA6DefaultIntFU(CVA6FU):
    opClasses = cva6MakeOpClassSet(["IntAlu"])
    timings = [CVA6FUTiming(description="Int", srcRegsRelativeLats=[2])]
    opLat = 3


class CVA6DefaultIntMulFU(CVA6FU):
    opClasses = cva6MakeOpClassSet(["IntMult"])
    timings = [CVA6FUTiming(description="Mul", srcRegsRelativeLats=[0])]
    opLat = 3


class CVA6DefaultIntDivFU(CVA6FU):
    opClasses = cva6MakeOpClassSet(["IntDiv"])
    issueLat = 9
    opLat = 9


class CVA6DefaultFloatSimdFU(CVA6FU):
    opClasses = cva6MakeOpClassSet(
        [
            "FloatAdd",
            "FloatCmp",
            "FloatCvt",
            "FloatMisc",
            "FloatMult",
            "FloatMultAcc",
            "FloatDiv",
            "FloatSqrt",
            "SimdAdd",
            "SimdAddAcc",
            "SimdAlu",
            "SimdCmp",
            "SimdCvt",
            "SimdMisc",
            "SimdMult",
            "SimdMultAcc",
            "SimdMatMultAcc",
            "SimdShift",
            "SimdShiftAcc",
            "SimdDiv",
            "SimdSqrt",
            "SimdFloatAdd",
            "SimdFloatAlu",
            "SimdFloatCmp",
            "SimdFloatCvt",
            "SimdFloatDiv",
            "SimdFloatMisc",
            "SimdFloatMult",
            "SimdFloatMultAcc",
            "SimdFloatMatMultAcc",
            "SimdFloatSqrt",
            "SimdReduceAdd",
            "SimdReduceAlu",
            "SimdReduceCmp",
            "SimdFloatReduceAdd",
            "SimdFloatReduceCmp",
            "SimdAes",
            "SimdAesMix",
            "SimdSha1Hash",
            "SimdSha1Hash2",
            "SimdSha256Hash",
            "SimdSha256Hash2",
            "SimdShaSigma2",
            "SimdShaSigma3",
            "Matrix",
            "MatrixMov",
            "MatrixOP",
        ]
    )

    timings = [CVA6FUTiming(description="FloatSimd", srcRegsRelativeLats=[2])]
    opLat = 6


class CVA6DefaultPredFU(CVA6FU):
    opClasses = cva6MakeOpClassSet(["SimdPredAlu"])
    timings = [CVA6FUTiming(description="Pred", srcRegsRelativeLats=[2])]
    opLat = 3


class CVA6DefaultMemFU(CVA6FU):
    opClasses = cva6MakeOpClassSet(
        ["MemRead", "MemWrite", "FloatMemRead", "FloatMemWrite"]
    )
    timings = [
        CVA6FUTiming(
            description="Mem", srcRegsRelativeLats=[1], extraAssumedLat=2
        )
    ]
    opLat = 1


class CVA6DefaultMiscFU(CVA6FU):
    opClasses = cva6MakeOpClassSet(["IprAccess", "InstPrefetch"])
    opLat = 1


class CVA6DefaultVecFU(CVA6FU):
    opClasses = cva6MakeOpClassSet(
        [
            "VectorUnitStrideLoad",
            "VectorUnitStrideStore",
            "VectorUnitStrideMaskLoad",
            "VectorUnitStrideMaskStore",
            "VectorStridedLoad",
            "VectorStridedStore",
            "VectorIndexedLoad",
            "VectorIndexedStore",
            "VectorUnitStrideFaultOnlyFirstLoad",
            "VectorWholeRegisterLoad",
            "VectorWholeRegisterStore",
            "VectorIntegerArith",
            "VectorFloatArith",
            "VectorFloatConvert",
            "VectorIntegerReduce",
            "VectorFloatReduce",
            "VectorMisc",
            "VectorIntegerExtension",
            "VectorConfig",
        ]
    )
    opLat = 1


class CVA6DefaultFUPool(CVA6FUPool):
    funcUnits = [
        CVA6DefaultIntFU(),
        CVA6DefaultIntFU(),
        CVA6DefaultIntMulFU(),
        CVA6DefaultIntDivFU(),
        CVA6DefaultFloatSimdFU(),
        CVA6DefaultPredFU(),
        CVA6DefaultMemFU(),
        CVA6DefaultMiscFU(),
        CVA6DefaultVecFU(),
    ]


class ThreadPolicyCVA6(Enum):
    vals = ["SingleThreaded", "RoundRobin", "Random"]


class BaseCVA6CPU(BaseCPU):
    type = "BaseCVA6CPU"
    cxx_header = "cpu/cva6/cpu.hh"
    cxx_class = "gem5::CVA6CPU"

    @classmethod
    def memory_mode(cls):
        return "timing"

    @classmethod
    def require_caches(cls):
        return True

    @classmethod
    def support_take_over(cls):
        return True

    threadPolicy = Param.ThreadPolicyCVA6(
        "RoundRobin", "Thread scheduling policy"
    )
    fetch1FetchLimit = Param.Unsigned(
        1, "Number of line fetches allowable in flight at once"
    )
    fetch1LineSnapWidth = Param.Unsigned(
        0,
        "Fetch1 'line' fetch snap size in bytes"
        " (0 means use system cache line size)",
    )
    fetch1LineWidth = Param.Unsigned(
        0,
        "Fetch1 maximum fetch size in bytes (0 means use system cache"
        " line size)",
    )
    fetch1ToFetch2ForwardDelay = Param.Cycles(
        1, "Forward cycle delay from Fetch1 to Fetch2 (1 means next cycle)"
    )
    fetch1ToFetch2BackwardDelay = Param.Cycles(
        1,
        "Backward cycle delay from Fetch2 to Fetch1 for branch prediction"
        " signalling (0 means in the same cycle, 1 mean the next cycle)",
    )

    fetch2InputBufferSize = Param.Unsigned(
        2, "Size of input buffer to Fetch2 in cycles-worth of insts."
    )
    fetch2ToDecodeForwardDelay = Param.Cycles(
        1, "Forward cycle delay from Fetch2 to Decode (1 means next cycle)"
    )
    fetch2CycleInput = Param.Bool(
        True,
        "Allow Fetch2 to cross input lines to generate full output each"
        " cycle",
    )

    decodeInputBufferSize = Param.Unsigned(
        3, "Size of input buffer to Decode in cycles-worth of insts."
    )
    decodeToExecuteForwardDelay = Param.Cycles(
        1, "Forward cycle delay from Decode to Execute (1 means next cycle)"
    )
    decodeInputWidth = Param.Unsigned(
        2,
        "Width (in instructions) of input to Decode (and implicitly"
        " Decode's own width)",
    )
    decodeCycleInput = Param.Bool(
        True,
        "Allow Decode to pack instructions from more than one input cycle"
        " to fill its output each cycle",
    )

    executeInputWidth = Param.Unsigned(
        2, "Width (in instructions) of input to Execute"
    )
    executeCycleInput = Param.Bool(
        True,
        "Allow Execute to use instructions from more than one input cycle"
        " each cycle",
    )
    executeIssueLimit = Param.Unsigned(
        2, "Number of issuable instructions in Execute each cycle"
    )
    executeMemoryIssueLimit = Param.Unsigned(
        1, "Number of issuable memory instructions in Execute each cycle"
    )
    executeCommitLimit = Param.Unsigned(
        2, "Number of committable instructions in Execute each cycle"
    )
    executeMemoryCommitLimit = Param.Unsigned(
        1, "Number of committable memory references in Execute each cycle"
    )
    executeInputBufferSize = Param.Unsigned(
        7, "Size of input buffer to Execute in cycles-worth of insts."
    )
    executeMemoryWidth = Param.Unsigned(
        0,
        "Width (and snap) in bytes of the data memory interface. (0 mean use"
        " the system cacheLineSize)",
    )
    executeMaxAccessesInMemory = Param.Unsigned(
        2,
        "Maximum number of concurrent accesses allowed to the memory system"
        " from the dcache port",
    )
    executeLSQMaxStoreBufferStoresPerCycle = Param.Unsigned(
        2, "Maximum number of stores that the store buffer can issue per cycle"
    )
    executeLSQRequestsQueueSize = Param.Unsigned(
        1, "Size of LSQ requests queue (address translation queue)"
    )
    executeLSQTransfersQueueSize = Param.Unsigned(
        2, "Size of LSQ transfers queue (memory transaction queue)"
    )
    executeLSQStoreBufferSize = Param.Unsigned(5, "Size of LSQ store buffer")
    executeBranchDelay = Param.Cycles(
        1,
        "Delay from Execute deciding to branch and Fetch1 reacting"
        " (1 means next cycle)",
    )

    executeFuncUnits = Param.CVA6FUPool(
        CVA6DefaultFUPool(), "FUlines for this processor"
    )

    executeSetTraceTimeOnCommit = Param.Bool(
        True, "Set inst. trace times to be commit times"
    )
    executeSetTraceTimeOnIssue = Param.Bool(
        False, "Set inst. trace times to be issue times"
    )

    executeAllowEarlyMemoryIssue = Param.Bool(
        True,
        "Allow mem refs to be issued to the LSQ before reaching the head of"
        " the in flight insts queue",
    )

    enableIdling = Param.Bool(
        True, "Enable cycle skipping when the processor is idle\n"
    )

    branchPred = Param.BranchPredictor(
        TournamentBP(numThreads=Parent.numThreads), "Branch Predictor"
    )

    def addCheckerCpu(self):
        print("Checker not yet supported by CVA6CPU")
        exit(1)
