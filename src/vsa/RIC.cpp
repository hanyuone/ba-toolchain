#include <algorithm>
#include <numeric>
#include <vsa/RIC.hpp>

std::string RIC::toString() {
    return std::to_string(this->stride) + " * [" + this->start.to_string() +
           ", " + this->end.to_string() + "] + " + std::to_string(this->offset);
}

bool RIC::isConstant() {
    return this->start == this->end && !this->start.is_infinity();
}

int RIC::getConstant() { return this->offset + this->start.getIntNumeral(); }

void RIC::set(const RIC &ric) {
    this->offset = ric.offset;
    this->start = ric.start;
    this->end = ric.end;
    this->stride = ric.stride;
}

bool RIC::isBottom() {
    return this->start.is_plus_infinity() && this->end.is_minus_infinity();
}

bool RIC::isTop() {
    return this->start.is_minus_infinity() && this->end.is_plus_infinity() &&
           this->stride == 1;
}

SVF::BoundedInt RIC::lower() {
    return this->offset + (this->stride * this->start);
}

SVF::BoundedInt RIC::upper() {
    return this->offset + (this->stride * this->end);
}

bool RIC::isSubset(RIC &rhs) {
    // Edgecase: LHS is bottom, always true
    if (this->isBottom()) {
        return true;
    }

    // Edgecase: RHS is bottom - LHS is not bottom, always false
    if (rhs.isBottom()) {
        return false;
    }

    // Edgecase: RHS is top, always true
    if (rhs.isTop()) {
        return true;
    }

    // Edgecase: LHS is top - RHS is not top, always false
    if (this->isTop()) {
        return false;
    }

    // Edgecase: LHS has only one element
    if (this->start == this->end) {
        SVF::BoundedInt singleValue = this->lower();
        SVF::BoundedInt rhsIndex = (singleValue - rhs.offset) / rhs.stride;

        return rhsIndex >= rhs.start && rhsIndex <= rhs.end;
    }

    // Main rule: LHS stride is *multiple* of RHS stride, LHS bounds
    // exist in RHS set
    if (this->stride % rhs.stride != 0) {
        return false;
    }

    SVF::BoundedInt rawLower = this->lower();
    SVF::BoundedInt rawUpper = this->upper();

    SVF::BoundedInt rhsLowerIndex = (rawLower - rhs.offset) / rhs.stride;
    SVF::BoundedInt rhsUpperIndex = (rawUpper - rhs.offset) / rhs.stride;

    if (rhsLowerIndex < rhs.start || rhsUpperIndex > rhs.end) {
        return false;
    }

    return true;
}

/// @brief Overwrite this RIC with the intersection (meet) of
/// this and another RIC.
/// @param rhs The RIC to meet with.
void RIC::meetWith(RIC &rhs) {
    if (this->isBottom() || rhs.isTop()) {
        return;
    }

    if (rhs.isBottom()) {
        this->set(BOTTOM);
        return;
    }

    if (this->isTop()) {
        this->set(rhs);
        return;
    }

    SVF::BoundedInt lhsLower = this->lower();
    SVF::BoundedInt rhsLower = rhs.lower();

    SVF::BoundedInt lhsUpper = this->upper();
    SVF::BoundedInt rhsUpper = rhs.upper();

    // Two ranges don't overlap
    if (lhsUpper < rhsLower || rhsUpper < lhsLower) {
        this->set(BOTTOM);
        return;
    }

    std::vector<SVF::BoundedInt> lowerCandidates = {lhsLower, rhsLower};
    SVF::BoundedInt lower = SVF::BoundedInt::max(lowerCandidates);
    std::vector<SVF::BoundedInt> upperCandidates = {lhsUpper, rhsUpper};
    SVF::BoundedInt upper = SVF::BoundedInt::min(upperCandidates);

    int stride = std::lcm(this->stride, rhs.stride);

    // Our first "candidate" of some value that both RICs could contain
    // is between `lower` and `upper`.
    int candidate;
    if (lower.is_minus_infinity() && upper.is_plus_infinity()) {
        candidate = 0;
    } else if (lower.is_minus_infinity()) {
        candidate = (upper - stride).getIntNumeral();
    } else {
        candidate = lower.getIntNumeral();
    }

    // We are guaranteed that there is at most one place of overlap between
    // our candidate and `candidate + stride`. If we find such a candidate,
    // then every `candidate + stride * x` within bounds is in our new
    // RIC.
    for (int i = 0; i < stride; i++) {
        if (candidate > upper) {
            break;
        }

        int check = candidate + i;

        // Check if the current value is in *both* RICs
        if ((check - this->offset) % this->stride != 0) {
            continue;
        }

        if ((check - rhs.offset) % rhs.stride != 0) {
            continue;
        }

        this->stride = stride;
        // Our candidate is either the lowest possible value of overlap,
        // or our starting bound is -inf
        this->start = lower.is_minus_infinity() ? lower : 0;
        this->end = upper.is_plus_infinity()
                        ? upper
                        : (upper - this->offset).getIntNumeral() / stride;
        this->offset = check;
        return;
    }

    // We've gone through all possible candidates within range and no
    // values overlap, therefore the meet set is empty
    this->set(BOTTOM);
}

/// @brief Overwrite this RIC with the union (join) of this and another RIC.
/// @param rhs The RIC to join with.
void RIC::joinWith(RIC &rhs) {
    if (this->isTop() || rhs.isBottom()) {
        return;
    }

    if (this->isBottom()) {
        this->set(rhs);
        return;
    }

    if (rhs.isTop()) {
        this->set(TOP);
        return;
    }

    SVF::BoundedInt lhsLower = this->lower();
    SVF::BoundedInt rhsLower = rhs.lower();

    SVF::BoundedInt lhsUpper = this->upper();
    SVF::BoundedInt rhsUpper = rhs.upper();

    std::vector<SVF::BoundedInt> lowerCandidates = {lhsLower, rhsLower};
    SVF::BoundedInt lower = SVF::BoundedInt::min(lowerCandidates);
    std::vector<SVF::BoundedInt> upperCandidates = {lhsUpper, rhsUpper};
    SVF::BoundedInt upper = SVF::BoundedInt::max(upperCandidates);

    // First candidate stride: GCD of both strides
    int stride = std::gcd(this->stride, rhs.stride);

    // Check whether the two RICs are "aligned" on our
    // candidate stride - if they are, then we in fact have our
    // new stride
    int offsetDiff = std::abs(this->offset - rhs.offset) % stride;
    if (offsetDiff != 0) {
        // Two RICs are misaligned - therefore our stride has
        // to adjust for that
        stride = std::gcd(stride, offsetDiff);
    }

    this->stride = stride;
    this->start = lower.is_minus_infinity() ? lower : 0;
    this->end = (upper - lower) / stride;
    this->offset =
        lower.is_minus_infinity() ? offsetDiff : lower.getIntNumeral();
}

void RIC::widenWith(RIC &rhs) {
    // Ignore cases where strides are different
    if (this->stride != rhs.stride) {
        return;
    }

    // Adjust RHS, so that the offsets are the same
    int adjust = rhs.offset - this->offset;
    if (adjust % this->stride != 0) {
        return;
    }

    int adjustSteps = adjust / this->stride;
    SVF::BoundedInt newStart = rhs.start - adjustSteps;
    SVF::BoundedInt newEnd = rhs.end - adjustSteps;

    if (newStart < this->start) {
        this->start = SVF::BoundedInt::minus_infinity();
    }

    if (newEnd > this->end) {
        this->end = SVF::BoundedInt::plus_infinity();
    }
}

RIC RIC::eq(RIC rhs) {
    if (this->isConstant() && rhs.isConstant()) {
        return RIC((int)this->getConstant() == rhs.getConstant());
    }

    RIC copy = (*this);
    copy.meetWith(rhs);

    if (copy.isBottom()) {
        return RIC(0);
    } else {
        return RIC(1, 0, 1, 0);
    }
}

RIC RIC::le(RIC rhs) {
    if (this->isConstant() && rhs.isConstant()) {
        return RIC((int)this->getConstant() < rhs.getConstant());
    }

    if (this->upper() <= rhs.lower()) {
        return RIC(1);
    } else if (rhs.upper() <= this->lower()) {
        return RIC(0);
    } else {
        return RIC(1, 0, 1, 0);
    }
}
