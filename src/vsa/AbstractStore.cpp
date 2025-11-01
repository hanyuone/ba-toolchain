#include <vsa/AbstractStore.hpp>

bool ALoc::operator<(const ALoc &rhs) const {
    return this->location < rhs.location || this->offset < rhs.offset ||
           this->size < rhs.size;
}

bool AbstractStore::operator==(AbstractStore &rhs) {
    // Iterate over a-loc mapping
    for (auto kv : this->alocs) {
        auto rhsKv = rhs.alocs.find(kv.first);

        if (rhsKv == rhs.alocs.end()) {
            return false;
        }

        if (kv.second != (*rhsKv).second) {
            return false;
        }
    }

    // Iterate over register mapping
    for (auto kv : this->registers) {
        auto rhsKv = rhs.registers.find(kv.first);

        if (rhsKv == rhs.registers.end()) {
            return false;
        }

        if (kv.second != (*rhsKv).second) {
            return false;
        }
    }

    return true;
}

void AbstractStore::widenWith(AbstractStore &rhs) {
    for (auto kv : this->alocs) {
        auto aloc = kv.first;
        auto rhsCandidate = rhs.alocs.find(aloc);

        if (rhsCandidate != rhs.alocs.end()) {
            kv.second.widenWith((*rhsCandidate).second);
        }
    }

    for (auto kv : this->registers) {
        auto reg = kv.first;
        auto rhsCandidate = rhs.registers.find(reg);

        if (rhsCandidate != rhs.registers.end()) {
            kv.second.widenWith((*rhsCandidate).second);
        }
    }
}
