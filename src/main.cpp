#include <iostream>

#include <vsa/RIC.hpp>

int main(int argc, char *argv[]) {
    // stride, start, end, offset
    RIC a{7, SVF::BoundedInt(5), SVF::BoundedInt(5), 1};
    RIC b = TOP;

    std::cout << a.isSubset(b) << std::endl;
}
