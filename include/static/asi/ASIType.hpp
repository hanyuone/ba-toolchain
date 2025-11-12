#pragma once

#include <cstddef>
#include <string>
#include <vector>

class ASIType {
  public:
    virtual size_t getSize() = 0;
    virtual std::string toString() = 0;

    bool hasBufferOverflow() { return this->overflow; }

    void setBufferOverflow(bool overflow) { this->overflow = overflow; }

  private:
    bool overflow = false;
};

class ImpossibleType : public ASIType {
  public:
    ImpossibleType(size_t _bytes) : bytes(_bytes) {}

    size_t getSize() override;
    std::string toString() override;

  private:
    size_t bytes;
};

class IntType : public ASIType {
  public:
    IntType(size_t _bytes) : bytes(_bytes) {}

    size_t getSize() override;
    std::string toString() override;

  private:
    size_t bytes;
};

class ArrayType : public ASIType {
  public:
    ArrayType(ASIType *_childType, size_t _children)
        : childType(_childType), children(_children) {}

    size_t getSize() override;
    std::string toString() override;

    ASIType *getChild();

  private:
    ASIType *childType;
    size_t children;
};

class StructType : public ASIType {
  public:
    size_t getSize() override;
    std::string toString() override;

    std::vector<ASIType *> getChildren();
    void addChild(ASIType *);

  private:
    std::vector<ASIType *> children;
};
