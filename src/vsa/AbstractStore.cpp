#include <vsa/AbstractStore.hpp>

bool ALoc::operator<(const ALoc &rhs) const {
    if (this->region < rhs.region) {
        return true;
    }

    if (this->region > rhs.region) {
        return false;
    }

    if (this->offset < rhs.offset) {
        return true;
    }

    if (this->offset > rhs.offset) {
        return false;
    }

    if (this->size < rhs.size) {
        return true;
    }

    if (this->size > rhs.size) {
        return false;
    }

    return false;
}

bool ALoc::in(RIC ric) {
    int alocUpper = this->offset + this->size;

    return (ric.lower() >= this->offset && ric.lower() < alocUpper) ||
           (ric.upper() >= this->offset && ric.upper() < alocUpper);
}

std::string ALoc::toString() {
    return "mem" + std::to_string(this->region) + "_" +
           std::to_string(this->offset) + "_" + std::to_string(this->size);
}

bool AbstractStore::operator==(AbstractStore &rhs) {
    // Iterate over a-loc mapping
    for (auto kv : this->alocs) {
        ALoc aloc = kv.first;
        auto rhsKv = rhs.alocs.find(aloc);

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

void AbstractStore::joinWith(AbstractStore &rhs) {
    for (auto kv : rhs.alocs) {
        auto aloc = kv.first;
        auto thisCandidate = this->alocs.find(aloc);

        if (thisCandidate != this->alocs.end()) {
            (*thisCandidate).second.joinWith(kv.second);
        } else {
            this->alocs.insert({aloc, kv.second});
        }
    }

    for (auto kv : rhs.registers) {
        auto reg = kv.first;
        auto thisCandidate = this->registers.find(reg);

        if (thisCandidate != this->registers.end()) {
            (*thisCandidate).second.joinWith(kv.second);
        } else {
            this->registers.insert({reg, kv.second});
        }
    }
}

void AbstractStore::widenWith(AbstractStore &rhs) {
    for (auto kv = this->alocs.begin(); kv != this->alocs.end(); kv++) {
        auto aloc = (*kv).first;
        auto rhsCandidate = rhs.alocs.find(aloc);

        if (rhsCandidate != rhs.alocs.end()) {
            (*kv).second.widenWith((*rhsCandidate).second);
        }
    }

    for (auto kv : this->registers) {
        auto reg = kv.first;
        this->registers[reg].widenWith(rhs.registers[reg]);
    }
}

void AbstractStore::narrowWith(AbstractStore &rhs) {
    for (auto kv = this->alocs.begin(); kv != this->alocs.end(); kv++) {
        auto aloc = (*kv).first;
        auto rhsCandidate = rhs.alocs.find(aloc);

        if (rhsCandidate != rhs.alocs.end()) {
            (*kv).second.narrowWith((*rhsCandidate).second);
        }
    }

    for (auto kv : this->registers) {
        auto reg = kv.first;
        auto rhsCandidate = rhs.registers.find(reg);

        if (rhsCandidate != rhs.registers.end()) {
            kv.second.narrowWith((*rhsCandidate).second);
        }
    }
}
