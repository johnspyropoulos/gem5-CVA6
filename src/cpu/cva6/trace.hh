/**
 * @file
 *
 *  This file contains miscellaneous classes and functions for formatting
 *  general trace information and also CVA6Trace information.
 *
 *  CVA6Trace is this model's cycle-by-cycle trace information for use by
 *  cva6view.
 */

#ifndef __CPU_CVA6_TRACE_HH__
#define __CPU_CVA6_TRACE_HH__

#include <string>

#include "base/named.hh"
#include "base/trace.hh"
#include "debug/CVA6Trace.hh"

namespace gem5
{

namespace cva6
{

/** DPRINTFN for CVA6Trace reporting */
template <class ...Args>
inline void
cva6Trace(const char *fmt, Args ...args)
{
    DPRINTF(CVA6Trace, (std::string("CVA6Trace: ") + fmt).c_str(), args...);
}

/** DPRINTFN for CVA6Trace CVA6Inst line reporting */
template <class ...Args>
inline void
cva6Inst(const Named &named, const char *fmt, Args ...args)
{
    DPRINTFS(CVA6Trace, &named, (std::string("CVA6Inst: ") + fmt).c_str(),
             args...);
}

/** DPRINTFN for CVA6Trace CVA6Line line reporting */
template <class ...Args>
inline void
cva6Line(const Named &named, const char *fmt, Args ...args)
{
    DPRINTFS(CVA6Trace, &named, (std::string("CVA6Line: ") + fmt).c_str(),
             args...);
}

} // namespace cva6
} // namespace gem5

#endif /* __CPU_CVA6_TRACE_HH__ */
