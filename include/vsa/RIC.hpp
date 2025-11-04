#pragma once

#include <AE/Core/NumericValue.h>

/// @brief A reduced interval congruence - represents a range with
/// an offset and skips. If we have `RIC ric = {2, 0, 4, 1}`, then
/// this represents the value 2 * [0, 4] + 1 = {1, 3, 5, 7, 9}.
struct RIC {
    int stride;
    SVF::BoundedInt start;
    SVF::BoundedInt end;
    int offset;

    RIC() : stride(1), start(SVF::BoundedInt::plus_infinity()), end(SVF::BoundedInt::minus_infinity()), offset(0) {}
    RIC(int _offset) : stride(1), start(0), end(0), offset(_offset) {}
    RIC(int _stride, SVF::BoundedInt _start, SVF::BoundedInt _end, int _offset)
        : stride(_stride), start(_start), end(_end), offset(_offset) {}

    bool operator==(RIC &);
    bool operator!=(RIC &);

    std::string toString();

    bool isConstant();
    int getConstant();

    void set(const RIC &);

    bool isBottom();
    bool isTop();

    SVF::BoundedInt upper();
    SVF::BoundedInt lower();

    bool isSubset(RIC &);

    void meetWith(RIC &);
    void joinWith(RIC &);
    void widenWith(RIC &);
    void narrowWith(RIC &);

    bool contains(int);

    RIC eq(RIC);
    RIC le(RIC);
};

// The "bottom" of the RIC lattice has no elements, thus
// we define it as an impossible range that no element can satisfy
const RIC BOTTOM = {1, SVF::BoundedInt::plus_infinity(),
                    SVF::BoundedInt::minus_infinity(), 0};

// The "top" of the RIC lattice contains every element, thus
// we define it as a range in which every integer is contained
const RIC TOP = {1, SVF::BoundedInt::minus_infinity(),
                 SVF::BoundedInt::plus_infinity(), 0};
                 
inline bool RIC::operator==(RIC &rhs) {
    return this->stride == rhs.stride && this->start == rhs.start &&
           this->end == rhs.end && this->offset == rhs.offset;
}

inline bool RIC::operator!=(RIC &rhs) { return !this->operator==(rhs); }
