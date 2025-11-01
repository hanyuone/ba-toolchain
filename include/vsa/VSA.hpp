/// Based on the algorithm outlined in
/// "Analyzing Memory Accesses in x86 Executables"
/// by Balakrishnan and Reps, 2004.
#pragma once

#include <cstdint>
#include <map>
#include <string>

#include <AE/Core/ICFGWTO.h>
#include <Graphs/ICFG.h>
#include <SVFIR/SVFIR.h>
#include <Util/GeneralType.h>
#include <vsa/AbstractStore.hpp>
#include <vsa/ValueSet.hpp>

typedef std::map<SVF::NodeID, ValueSet> SVFVarState;

/// @brief Data structure for the abstract state and any temporary
/// variables stored within each basic block.
struct Snapshot {
    SVFVarState varState;
    AbstractStore abstractStore;
    SVF::s64_t nextPc;

    ValueSet getSVFVarSet(SVF::NodeID nodeID) {
        return this->varState[nodeID];
    }

    ValueSet getALocSet(ALoc aloc) {
        return this->abstractStore.getALocSet(aloc);
    }

    ValueSet getRegisterSet(std::string reg) {
        return this->abstractStore.getRegisterSet(reg);
    }

    void initSVFVar(SVF::ValVar *);
};

/// @brief A class that performs value-set analysis, a slightly-
/// adjusted version of abstract interpretation, with support for
/// constant "skips".
class VSA {
  public:
    VSA(SVF::ICFG *_icfg) : icfg(_icfg) {
        const int N_REGISTERS = 3;
        std::string REGISTERS[N_REGISTERS] = {"RAX", "RBX", "RCX"};

        this->svfir = SVF::PAG::getPAG();

        for (int i = 0; i < N_REGISTERS; i++) {
            ValueSet vs;
            this->blockState.abstractStore.registers[REGISTERS[i]] = vs;
        }
    }

    /// Return its abstract state given an ICFGNode
    AbstractStore &getAbsStateFromTrace(const SVF::ICFGNode *node) {
        return this->postBasicBlock[node].abstractStore;
    }

    void initWTO();
    void handleGlobalNode();
    void handleMainFunction(const SVF::FunObjVar *);
    void analyse();

    ValueSet get(SVF::NodeID, Snapshot &);

    std::vector<const SVF::ICFGNode *>
    getNextNodes(const SVF::ICFGNode *) const;
    std::vector<const SVF::ICFGNode *>
    getNextNodesOfCycle(const SVF::ICFGCycleWTO *) const;
    bool mergeStatesFromPredecessors(const SVF::ICFGNode *, AbstractStore &);

    bool isBranchFeasible(const SVF::IntraCFGEdge *, Snapshot &);
    bool isCmpBranchFeasible(const SVF::CmpStmt *, SVF::s64_t, SVFVarState &);
    bool isStartOfBasicBlock(const SVF::ICFGNode *);

    void handleFunction(const SVF::ICFGNode *);
    bool handleICFGNode(const SVF::ICFGNode *);
    void handleICFGCycle(const SVF::ICFGCycleWTO *);
    void handleCallSite(const SVF::CallICFGNode *);

    void updateAbsState(const SVF::SVFStmt *);
    void updateStateOnAddr(const SVF::AddrStmt *);
    void updateStateOnCmp(const SVF::CmpStmt *);
    void updateStateOnCall(const SVF::CallPE *);
    void updateStateOnRet(const SVF::RetPE *);
    void updateStateOnCopy(const SVF::CopyStmt *);
    void updateStateOnBinary(const SVF::BinaryOPStmt *);
    void updateStateOnStore(const SVF::StoreStmt *);
    void updateStateOnLoad(const SVF::LoadStmt *);
    void updateStateOnExtCall(const SVF::CallICFGNode *);
    void updateStateOnSelect(const SVF::SelectStmt *);
    void updateStateOnBranch(const SVF::BranchStmt *);

  protected:
    /// Map a function to its corresponding WTO
    SVF::Map<const SVF::FunObjVar *, SVF::ICFGWTO *> funcToWTO;
    /// A set of functions which are involved in recursions
    SVF::Set<const SVF::FunObjVar *> recursiveFuns;

  private:
    // Control-flow graph to analyse
    SVF::SVFIR *svfir;
    SVF::ICFG *icfg;

    // List of function cycles
    SVF::Map<const SVF::ICFGNode *, const SVF::ICFGCycleWTO *> cycleHeadToCycle;

    /// Program counter (and related variables), treated as constants
    SVF::s64_t pc;
    SVF::s64_t nextPc;
    SVF::s64_t returnPc;

    /// Global variables extracted from global node
    SVFVarState globalState;

    /// Mapping of variables (alocs, registers, SVF vars) to value sets,
    /// for the current basic block
    Snapshot blockState;
    /// Stack of mappings
    // TODO: add back for inter-procedural analysis
    // std::vector<Snapshot> stateStack;

    /// State of variables immediately before the start of a basic block
    std::map<const SVF::ICFGNode *, Snapshot> preBasicBlock;
    /// State of variables immediately after the end of a basic block
    std::map<const SVF::ICFGNode *, Snapshot> postBasicBlock;

    // TODO: remove once working on proper implementation
    int readAccesses;
};
