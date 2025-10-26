/// Based on the algorithm outlined in
/// "Analyzing Memory Accesses in x86 Executables"
/// by Balakrishnan and Reps, 2004.

#include <cstdint>
#include <map>
#include <string>

#include <Graphs/ICFG.h>
#include <vsa/ValueSet.hpp>

/// @brief A "quasi-variable" that could contain a range of values.
struct ALoc {
    uint64_t location;
    int offset;
    size_t size;
};

/// @brief A class that performs value-set analysis, a slightly-
/// adjusted version of abstract interpretation, with support for
/// constant "skips".
class VSA {
  public:
    VSA(SVF::ICFG *_icfg) : icfg(_icfg) {}
    virtual void analyse();

  private:
    /// @brief A mapping of a-locs to potential addresses.
    typedef std::map<ALoc, ValueSet> AbstractStore;

    // Control-flow graph to analyse
    SVF::ICFG *icfg;

    /// Abstract trace immediately before an ICFGNode.
    std::map<const SVF::ICFGNode *, AbstractStore> preAbsTrace;
    /// Abstract trace immediately after an ICFGNode.
    std::map<const SVF::ICFGNode *, AbstractStore> postAbsTrace;
};
