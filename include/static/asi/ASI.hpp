#pragma once

#include <static/asi/ASIType.hpp>
#include <static/vsa/AbstractStore.hpp>
#include <static/vsa/ValueSet.hpp>

class ASI {
  public:
    ASI(std::map<SVF::NodeID, std::pair<ValueSet, size_t>> _accesses,
        std::vector<ALoc> _alocs)
        : accesses(_accesses), alocs(_alocs) {}

    std::map<uint64_t, std::vector<ALoc>> findALocs(ValueSet);
    ASIType *infer(ValueSet, size_t, ALoc);

    ArrayType *unifyArrays(ArrayType *, ArrayType *);
    std::pair<ASIType *, ASIType *> split(ASIType *, size_t);
    StructType *unifyStructs(StructType *, StructType *);
    ASIType *unify(ASIType *, ASIType *);

    void simplifyTypes();
    void analyse();

    std::map<ALoc, ASIType *> getTypes();

  private:
    std::map<SVF::NodeID, std::pair<ValueSet, size_t>> accesses;
    std::vector<ALoc> alocs;

    std::map<ALoc, ASIType *> regionsToTypes;
};
