import m5
from m5.objects import Root

from gem5.components.boards.riscv_board import RiscvBoard
from gem5.components.memory import DualChannelDDR4_2400
from gem5.components.processors.simple_processor import CVA6Processor
from gem5.isas import ISA
from gem5.resources.resource import obtain_resource
from gem5.simulate.simulator import Simulator
from gem5.utils.requires import requires

# This runs a check to ensure the gem5 binary is compiled for RISCV.
requires(isa_required=ISA.RISCV)

# With RISCV, we use simple caches.
from gem5.components.cachehierarchies.classic.private_l1_cache_hierarchy import (
    PrivateL1CacheHierarchy,
)

# Here we setup the parameters of the l1 and l2 caches.
cache_hierarchy = PrivateL1CacheHierarchy(l1d_size="16kB", l1i_size="16kB")

# Memory: Dual Channel DDR4 2400 DRAM device.
memory = DualChannelDDR4_2400(size="3GB")

# Here we setup the processor. We use a simple processor.
processor = CVA6Processor(num_cores=2)

# Here we setup the board. The RiscvBoard allows for Full-System RISCV
# simulations.
board = RiscvBoard(
    clk_freq="3GHz",
    processor=processor,
    memory=memory,
    cache_hierarchy=cache_hierarchy,
)

# Here we a full system workload: "riscv-ubuntu-20.04-boot" which boots
# Ubuntu 20.04. Once the system successfully boots it encounters an `m5_exit`
# instruction which stops the simulation. When the simulation has ended you may
# inspect `m5out/system.pc.com_1.device` to see the stdout.
board.set_workload(obtain_resource("riscv-ubuntu-20.04-boot"))

simulator = Simulator(board=board)
simulator.run()
