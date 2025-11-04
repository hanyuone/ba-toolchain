#pragma once

#include <map>

#include <vsa/ValueSet.hpp>

/// @brief A "quasi-variable" that could contain a range of values.
struct ALoc {
    uint64_t region;
    int offset;
    size_t size;

    bool operator<(const ALoc &) const;
    bool operator==(ALoc &);

    bool in(RIC);
    std::string toString();
};

/// @brief A mapping of registers and abstract locations to values, which
/// could then represent integers or addresses.
struct AbstractStore {
    std::map<ALoc, ValueSet> alocs;
    std::map<std::string, ValueSet> registers;

    bool operator==(AbstractStore &);

    ValueSet getALocSet(ALoc aloc) {
        return this->alocs[aloc];
    }

    ValueSet getRegisterSet(std::string reg) {
        return this->registers[reg];
    }

    void joinWith(AbstractStore &);
    void widenWith(AbstractStore &);
    void narrowWith(AbstractStore &);
};
