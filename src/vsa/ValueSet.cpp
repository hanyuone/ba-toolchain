#include <limits>
#include <vsa/ValueSet.hpp>

bool ValueSet::operator==(ValueSet &rhs) {
    for (auto kv : this->values) {
        auto rhsKv = rhs.values.find(kv.first);

        if (rhsKv == rhs.values.end()) {
            return false;
        }

        if (kv.second != (*rhsKv).second) {
            return false;
        }
    }

    return true;
}

bool ValueSet::operator!=(ValueSet &rhs) {
    return !this->operator==(rhs);
}

bool ValueSet::isSubset(ValueSet &rhs) {
    for (auto locMapping : this->values) {
        uint64_t region = locMapping.first;
        auto rhsRegion = rhs.values.find(region);

        if (rhsRegion == rhs.values.end()) {
            return false;
        }

        RIC rhsRic = (*rhsRegion).second;

        if (!locMapping.second.isSubset(rhsRic)) {
            return false;
        }
    }

    return true;
}

void ValueSet::meetWith(ValueSet &rhs) {
    for (auto it = this->values.begin(); it != this->values.end();) {
        // Find region in other section
        uint64_t region = (*it).first;
        auto rhsRegion = rhs.values.find(region);

        if (rhsRegion == rhs.values.end()) {
            it = this->values.erase(it);
        } else {
            (*it).second.meetWith((*rhsRegion).second);
            it++;
        }
    }
}

void ValueSet::joinWith(ValueSet rhs) {
    // Find regions in other section to either add or join to
    for (auto it = rhs.values.begin(); it != rhs.values.end(); it++) {
        // Find corresponding regions in this
        uint64_t region = (*it).first;
        auto lhsRegion = this->values.find(region);

        if (lhsRegion == this->values.end()) {
            this->values.insert(std::pair<uint64_t, RIC>(region, (*it).second));
        } else {
            (*lhsRegion).second.joinWith((*it).second);
        }
    }
}

/// @brief Widens the current value set, according to some other
/// value set. Currently can only widen in one direction.
/// @param rhs 
void ValueSet::widenWith(ValueSet &rhs) {
    for (auto kv = this->values.begin(); kv != this->values.end(); kv++) {
        auto rhsRegion = rhs.values.find((*kv).first);
        if (rhsRegion == rhs.values.end()) {
            continue;
        }

        (*kv).second.widenWith((*rhsRegion).second);
    }
}

void ValueSet::narrowWith(ValueSet &rhs) {
    for (auto kv = this->values.begin(); kv != this->values.end(); kv++) {
        auto rhsRegion = rhs.values.find((*kv).first);
        if (rhsRegion == rhs.values.end()) {
            continue;
        }

        (*kv).second.narrowWith((*rhsRegion).second);
    }
}

void ValueSet::adjust(int c) {
    for (auto it = this->values.begin(); it != this->values.end(); it++) {
        (*it).second.offset += c;
    }
}

void ValueSet::removeLowerBounds() {
    for (auto ric : this->values) {
        ric.second.start = SVF::BoundedInt::minus_infinity();
    }
}

void ValueSet::removeUpperBounds() {
    for (auto ric : this->values) {
        ric.second.end = SVF::BoundedInt::plus_infinity();
    }
}
