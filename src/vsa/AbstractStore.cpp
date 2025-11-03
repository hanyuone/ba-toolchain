#include <vsa/AbstractStore.hpp>

bool ALoc::operator<(const ALoc &rhs) const {
    return this->region < rhs.region || this->offset < rhs.offset ||
           this->size < rhs.size;
}

std::string ALoc::toString() {
    return "mem" + std::to_string(this->region) + "_" +
           std::to_string(this->offset);
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

void AbstractStore::joinWith(AbstractStore &rhs) {
    for (auto kv : rhs.alocs) {
        auto aloc = kv.first;
        auto thisCandidate = this->alocs.find(aloc);

        if (thisCandidate != this->alocs.end()) {
            (*thisCandidate).second.joinWith(kv.second);
            std::cout << "ALoc at offset " << (*thisCandidate).first.offset
                      << " now has value "
                      << (*thisCandidate).second.getGlobal().toString()
                      << std::endl;
        } else {
            this->alocs.insert({aloc, kv.second});
        }
    }

    for (auto kv : this->registers) {
        auto reg = kv.first;
        auto rhsCandidate = rhs.registers.find(reg);

        if (rhsCandidate != rhs.registers.end()) {
            kv.second.joinWith((*rhsCandidate).second);
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
