#include <cstdint>
#include <map>
#include <vsa/RIC.hpp>

/// @brief A representation of all addresses that an a-loc
/// could hold. It is represented as a mapping of memory regions
/// (represented as uints) to *offsets* from the start of that
/// region.
class ValueSet {
  public:
    virtual bool isSubset(ValueSet &rhs);

    virtual void meetWith(ValueSet &rhs);
    virtual void joinWith(ValueSet &rhs);
    virtual void widenWith(ValueSet &rhs);

    virtual void adjust(int c);

    virtual void removeLowerBounds();
    virtual void removeUpperBounds();

  private:
    std::map<uint64_t, RIC> values;
};
