#pragma once

#include <cstdint>
#include <map>
#include <vsa/RIC.hpp>

/// @brief A representation of all addresses that an a-loc
/// could hold. It is represented as a mapping of memory regions
/// (represented as uints) to *offsets* from the start of that
/// region.
struct ValueSet {
    std::map<uint64_t, RIC> values;

    ValueSet() {}
    ValueSet(int c) { this->values.insert({0, RIC(c)}); }

    bool operator==(ValueSet &);
    bool operator!=(ValueSet &);

    bool isSubset(ValueSet &);

    void meetWith(ValueSet &);
    void joinWith(ValueSet);
    void widenWith(ValueSet &);

    void adjust(int);

    void removeLowerBounds();
    void removeUpperBounds();

    RIC getGlobal() {
      return this->values[0];
    }
};
