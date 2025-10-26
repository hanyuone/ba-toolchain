#include <AE/Core/NumericValue.h>

/// @brief A reduced interval congruence - represents a range with
/// an offset and skips. If we have `RIC ric = {2, 0, 4, 1}`, then
/// this represents the value 2 * [0, 4] + 1 = {1, 3, 5, 7, 9}.
struct RIC {
    int stride;
    SVF::BoundedInt start;
    SVF::BoundedInt end;
    int offset;

    RIC(int _stride, SVF::BoundedInt _start, SVF::BoundedInt _end, int _offset)
        : stride(_stride), start(_start), end(_end), offset(_offset) {}

    void set(const RIC &ric);

    bool isBottom();
    bool isTop();
    bool isSubset(RIC &rhs);

    void meetWith(RIC &rhs);
    void joinWith(RIC &rhs);
    void widenWith(RIC &rhs);
};

// The "bottom" of the RIC lattice has no elements, thus
// we define it as an impossible range that no element can satisfy
const RIC BOTTOM = {1, SVF::BoundedInt::plus_infinity(),
                    SVF::BoundedInt::minus_infinity(), 0};

// The "top" of the RIC lattice contains every element, thus
// we define it as a range in which every integer is contained
const RIC TOP = {1, SVF::BoundedInt::minus_infinity(),
                 SVF::BoundedInt::plus_infinity(), 0};
