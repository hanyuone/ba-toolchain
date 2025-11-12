#include <algorithm>
#include <numeric>
#include <static/asi/ASI.hpp>

/// Given a value set, find all addresses that the value
/// set could be referencing.
std::map<uint64_t, std::vector<ALoc>> ASI::findALocs(ValueSet address) {
    std::map<uint64_t, std::vector<ALoc>> referenced;

    for (ALoc aloc : this->alocs) {
        uint64_t region = aloc.region;
        auto addrOfRegion = address.values.find(region);

        if (addrOfRegion == address.values.end()) {
            continue;
        }

        RIC ric = (*addrOfRegion).second;

        bool ricLowerInAloc = ric.lower() >= aloc.offset &&
                              ric.lower() < (aloc.offset + aloc.size);
        bool ricSurroundsAloc = ric.lower() < aloc.offset &&
                                ric.upper() >= (aloc.offset + aloc.size);
        bool ricUpperInAloc = ric.upper() >= aloc.offset &&
                              ric.upper() < (aloc.offset + aloc.size);

        if (ricLowerInAloc || ricSurroundsAloc || ricUpperInAloc) {
            referenced[region].push_back(aloc);
        }
    }

    return referenced;
}

ASIType *ASI::infer(ValueSet address, size_t size, ALoc at) {
    RIC ric = address.values[at.region];

    if (ric.isConstant() && size == at.size) {
        // We're accessing x bytes of our a-loc, which has size x bytes.
        // The underlying type is just a contiguous block of bytes
        IntType *inferType = new IntType(size);
        return inferType;
    } else if (ric.isConstant()) {
        // We're accessing some *subset* of our a-loc
        int ricValue = ric.getConstant();
        int ricOffset = ricValue - at.offset;

        StructType *inferType = new StructType();

        if (ricOffset == 0) {
            // Accessing start
            IntType *access = new IntType(size);
            IntType *remainder = new IntType(at.size - size);

            inferType->addChild(access);
            inferType->addChild(remainder);
        } else if (ricOffset + size == (at.offset + at.size)) {
            // Accessing end
            IntType *remainder = new IntType(at.size - size);
            IntType *access = new IntType(size);

            inferType->addChild(remainder);
            inferType->addChild(access);
        } else {
            // Accessing somewhere in the middle
            IntType *leftRemainder = new IntType(ricOffset);
            IntType *access = new IntType(size);
            IntType *rightRemainder = new IntType(at.size - (size + ricOffset));

            inferType->addChild(leftRemainder);
            inferType->addChild(access);
            inferType->addChild(rightRemainder);
        }

        return inferType;
    }

    // Array access
    bool bufferOverflow = false;

    if (size > ric.stride) {
        // We access overlapping elements, which is a
        // high indication of a buffer overflow
        bufferOverflow = true;

        // Reset stride to something appropriate
        size = ric.stride;
    }

    ASIType *childType;
    if (size == ric.stride) {
        childType = new IntType(size);
    } else {
        int childOffset = (ric.offset - at.offset) % ric.stride;

        if (childOffset + size > ric.stride) {
            bufferOverflow = true;
            childType = new IntType(ric.stride);
        } else {
            StructType *structType = new StructType();

            if (childOffset == 0) {
                // Accessing start
                IntType *access = new IntType(size);
                IntType *remainder = new IntType(ric.stride - size);

                structType->addChild(access);
                structType->addChild(remainder);
            } else if (childOffset + size == ric.stride) {
                // Accessing end
                IntType *remainder = new IntType(ric.stride - size);
                IntType *access = new IntType(size);

                structType->addChild(remainder);
                structType->addChild(access);
            } else {
                // Accessing somewhere in the middle
                IntType *leftRemainder = new IntType(childOffset);
                IntType *access = new IntType(size);
                IntType *rightRemainder =
                    new IntType(ric.stride - (size + childOffset));

                structType->addChild(leftRemainder);
                structType->addChild(access);
                structType->addChild(rightRemainder);
            }

            childType = structType;
        }
    }

    // Assumes that the a-loc we're accessing starts with
    // an array. For more complicated cases this might not
    // be the case, but this is a safe assumption to make
    // in order to have an inferred data type that is somewhat
    // useful
    int possibleElements = at.size / ric.stride;
    int accessedElements =
        (ric.end.getIntNumeral() - ric.start.getIntNumeral()) + 1;

    ASIType *inferType;

    if (possibleElements == accessedElements) {
        inferType = new ArrayType(childType, possibleElements);
    } else {
        StructType *structType = new StructType();
        ArrayType *arrayType = new ArrayType(childType, accessedElements);

        int elementOffset = (ric.offset - at.offset) % ric.stride;
        int arrayOffset = (ric.offset - at.offset) - elementOffset;

        if (arrayOffset == 0) {
            // Accessing start
            IntType *remainder =
                new IntType(at.size - (accessedElements * ric.stride));

            structType->addChild(arrayType);
            structType->addChild(remainder);
        } else if (arrayOffset + ric.stride == at.size) {
            // Accessing end
            IntType *remainder =
                new IntType(at.size - (accessedElements * ric.stride));

            structType->addChild(remainder);
            structType->addChild(arrayType);
        } else {
            // Accessing somewhere in the middle
            IntType *leftRemainder = new IntType(at.size - arrayOffset);
            IntType *rightRemainder = new IntType(
                at.size - (arrayOffset + accessedElements * ric.stride));

            structType->addChild(leftRemainder);
            structType->addChild(arrayType);
            structType->addChild(rightRemainder);
        }

        inferType = structType;
    }

    inferType->setBufferOverflow(bufferOverflow);
    return inferType;
}

ArrayType *ASI::unifyArrays(ArrayType *lhs, ArrayType *rhs) {
    ASIType *lhsChild = lhs->getChild();
    ASIType *rhsChild = rhs->getChild();

    int lcm = std::lcm(lhsChild->getSize(), rhsChild->getSize());

    ASIType *newLhsChild;

    int leftElements = lcm / lhsChild->getSize();
    if (leftElements == 1) {
        newLhsChild = lhsChild;
    } else {
        StructType *structChild = new StructType();

        for (int i = 0; i < leftElements; i++) {
            structChild->addChild(lhsChild);
        }

        newLhsChild = structChild;
    }

    ASIType *newRhsChild;

    int rightElements = lcm / rhsChild->getSize();
    if (rightElements == 1) {
        newRhsChild = rhsChild;
    } else {
        StructType *structChild = new StructType();

        for (int i = 0; i < rightElements; i++) {
            structChild->addChild(rhsChild);
        }

        newRhsChild = structChild;
    }

    ASIType *unifiedChild = unify(newLhsChild, newRhsChild);

    size_t newElements = lhs->getSize() / lcm;
    return new ArrayType(unifiedChild, newElements);
}

/// @brief Split type into two - the first has `n` bytes, the the second
/// has the remainder.
/// @param type an integer or array type
/// @param n the number of bytes to take for first type
/// @return 
std::pair<ASIType *, ASIType *> ASI::split(ASIType *type, size_t n) {
    if (IntType *intType = dynamic_cast<IntType *>(type)) {
        IntType *target = new IntType(n);
        IntType *remainder = new IntType(intType->getSize() - n);

        return {target, remainder};
    }

    ArrayType *arrayType = dynamic_cast<ArrayType *>(type);

    // Assumes that `n` lies exactly on array element boundaries
    int index = n / arrayType->getChild()->getSize();
    if (index == 1) {
        ASIType *target = arrayType->getChild();
        ArrayType *remainder = new ArrayType(arrayType->getChild(), arrayType->getSize() - 1);

        return {target, remainder};
    } else if (index == arrayType->getSize() - 1) {
        ArrayType *target = new ArrayType(arrayType->getChild(), arrayType->getSize() - 1);
        ASIType *remainder = arrayType->getChild();

        return {target, remainder};
    } else {
        ArrayType *target = new ArrayType(arrayType->getChild(), index);
        ArrayType *remainder = new ArrayType(arrayType->getChild(), arrayType->getSize() - index);

        return {target, remainder};
    }
}

/// Unify two structs by slowly removing/shrinking elements from both sides
/// of the unification, until we have a complete unified data structure
StructType *ASI::unifyStructs(StructType *lhs, StructType *rhs) {
    StructType *result = new StructType();

    std::vector<ASIType *> lhsChildren = lhs->getChildren();
    std::deque<ASIType *> lhsDeque(lhsChildren.begin(), lhsChildren.end());

    std::vector<ASIType *> rhsChildren = rhs->getChildren();
    std::deque<ASIType *> rhsDeque(rhsChildren.begin(), rhsChildren.end());

    while (!lhsDeque.empty()) {
        ASIType *leftChild = lhsDeque.front();
        lhsDeque.pop_front();

        ASIType *rightChild = rhsDeque.front();
        rhsDeque.pop_front();

        if (leftChild->getSize() == rightChild->getSize()) {
            // Both children are of equal size, just unify them and add to result
            ASIType *unified = unify(leftChild, rightChild);
            result->addChild(unified);
        } else if (leftChild->getSize() > rightChild->getSize()) {
            // Split left child, unify whatever is equal and add remainder back to lhsDeque
            auto splitLeft = split(leftChild, rightChild->getSize());
            lhsDeque.push_front(splitLeft.second);

            ASIType *unified = unify(splitLeft.first, rightChild);
            result->addChild(unified);
        } else {
            // Split right child, unify whatever is equal and add remainder back to rhsDeque
            auto splitRight = split(rightChild, leftChild->getSize());
            rhsDeque.push_front(splitRight.second);

            ASIType *unified = unify(leftChild, splitRight.first);
            result->addChild(unified);
        }
    }

    return result;
}

ASIType *ASI::unify(ASIType *existing, ASIType *inferred) {
    // If either type is an IntType, return the other one
    if (dynamic_cast<IntType *>(existing) != NULL) {
        return inferred;
    }

    if (dynamic_cast<IntType *>(inferred) != NULL) {
        return existing;
    }

    // If both are arrays, divide each array according to GCD,
    // unify all possible elements together. Otherwise, treat
    // both as structs and perform ASI algorithm
    if (ArrayType *existingArray = dynamic_cast<ArrayType *>(existing)) {
        if (ArrayType *inferredArray = dynamic_cast<ArrayType *>(inferred)) {
            return unifyArrays(existingArray, inferredArray);
        }

        StructType *inferredStruct = dynamic_cast<StructType *>(inferred);
        StructType *existingStruct = new StructType();
        existingStruct->addChild(existingArray);

        return unifyStructs(existingStruct, inferredStruct);
    }

    StructType *existingStruct = dynamic_cast<StructType *>(existing);
    StructType *inferredStruct;

    if (ArrayType *inferredArray = dynamic_cast<ArrayType *>(inferred)) {
        inferredStruct = new StructType();
        inferredStruct->addChild(inferredArray);
    } else {
        inferredStruct = dynamic_cast<StructType *>(inferred);
    }

    return unifyStructs(existingStruct, inferredStruct);
}

void ASI::simplifyTypes() {
    auto prev = this->regionsToTypes;
    this->regionsToTypes.clear();

    for (auto kv : prev) {
        ALoc aloc = kv.first;
        ASIType *type = kv.second;

        if (StructType *structType = dynamic_cast<StructType *>(type)) {
            int offset = aloc.offset;

            for (ASIType *child : structType->getChildren()) {
                ALoc newAloc{aloc.region, offset, child->getSize()};
                this->regionsToTypes[newAloc] = child;
                offset += child->getSize();
            }
        } else {
            this->regionsToTypes[aloc] = type;
        }
    }
}

void ASI::analyse() {
    // Copy alocs into regions and types map
    for (ALoc aloc : this->alocs) {
        IntType *intType = new IntType(aloc.size);
        this->regionsToTypes[aloc] = intType;
    }

    for (auto kv : this->accesses) {
        std::pair<ValueSet, size_t> access = kv.second;
        ValueSet address = access.first;
        size_t size = access.second;

        // We're not accessing any address
        if (address.values.empty()) {
            continue;
        }

        // Extract existing type(s) of a-locs this data access affects
        std::map<uint64_t, std::vector<ALoc>> found = findALocs(address);

        for (auto kv : found) {
            uint64_t region = kv.first;
            std::vector<ALoc> foundInRegion = kv.second;

            ASIType *existingMemory;
            if (foundInRegion.size() == 1) {
                existingMemory = this->regionsToTypes[foundInRegion[0]];
            } else {
                StructType *container = new StructType();

                std::sort(foundInRegion.begin(), foundInRegion.end());
                for (ALoc target : foundInRegion) {
                    container->addChild(this->regionsToTypes[target]);
                }

                existingMemory = container;
            }

            // Since we're updating the map at the very end, remove the affected
            // a-locs from the map
            for (ALoc target : foundInRegion) {
                this->regionsToTypes.erase(target);
            }

            // Create new a-loc that covers all possible previous a-locs
            ALoc newALoc{1, foundInRegion[0].offset, existingMemory->getSize()};

            // Infer type of a-loc(s) from data access
            ASIType *inferredMemory = infer(address, size, newALoc);

            // We have generated a constraint - unify according to the
            // constraint
            ASIType *unified = unify(existingMemory, inferredMemory);

            // Create new a-loc
            // We know that the first a-loc will have the starting address
            this->regionsToTypes[newALoc] = unified;
        }
    }

    simplifyTypes();
}

std::map<ALoc, ASIType *> ASI::getTypes() { return this->regionsToTypes; }
