#include <WPA/Andersen.h>

#include <vsa/VSA.hpp>

// according to varieties of cmp insts,
// maybe var X var, var X const, const X var, const X const
// we accept 'var X const' 'var X var' 'const X const'
// if 'const X var', we need to reverse op0 op1 and its predicate 'var X' const'
// X' is reverse predicate of X
// == -> !=, != -> ==, > -> <=, >= -> <, < -> >=, <= -> >
SVF::Map<SVF::s32_t, SVF::s32_t> _reverse_predicate = {
    {SVF::CmpStmt::Predicate::FCMP_OEQ,
     SVF::CmpStmt::Predicate::FCMP_ONE}, // == -> !=
    {SVF::CmpStmt::Predicate::FCMP_UEQ,
     SVF::CmpStmt::Predicate::FCMP_UNE}, // == -> !=
    {SVF::CmpStmt::Predicate::FCMP_OGT,
     SVF::CmpStmt::Predicate::FCMP_OLE}, // > -> <=
    {SVF::CmpStmt::Predicate::FCMP_OGE,
     SVF::CmpStmt::Predicate::FCMP_OLT}, // >= -> <
    {SVF::CmpStmt::Predicate::FCMP_OLT,
     SVF::CmpStmt::Predicate::FCMP_OGE}, // < -> >=
    {SVF::CmpStmt::Predicate::FCMP_OLE,
     SVF::CmpStmt::Predicate::FCMP_OGT}, // <= -> >
    {SVF::CmpStmt::Predicate::FCMP_ONE,
     SVF::CmpStmt::Predicate::FCMP_OEQ}, // != -> ==
    {SVF::CmpStmt::Predicate::FCMP_UNE,
     SVF::CmpStmt::Predicate::FCMP_UEQ}, // != -> ==
    {SVF::CmpStmt::Predicate::ICMP_EQ,
     SVF::CmpStmt::Predicate::ICMP_NE}, // == -> !=
    {SVF::CmpStmt::Predicate::ICMP_NE,
     SVF::CmpStmt::Predicate::ICMP_EQ}, // != -> ==
    {SVF::CmpStmt::Predicate::ICMP_UGT,
     SVF::CmpStmt::Predicate::ICMP_ULE}, // > -> <=
    {SVF::CmpStmt::Predicate::ICMP_ULT,
     SVF::CmpStmt::Predicate::ICMP_UGE}, // < -> >=
    {SVF::CmpStmt::Predicate::ICMP_UGE,
     SVF::CmpStmt::Predicate::ICMP_ULT}, // >= -> <
    {SVF::CmpStmt::Predicate::ICMP_SGT,
     SVF::CmpStmt::Predicate::ICMP_SLE}, // > -> <=
    {SVF::CmpStmt::Predicate::ICMP_SLT,
     SVF::CmpStmt::Predicate::ICMP_SGE}, // < -> >=
    {SVF::CmpStmt::Predicate::ICMP_SGE,
     SVF::CmpStmt::Predicate::ICMP_SLT}, // >= -> <
};

SVF::Map<SVF::s32_t, SVF::s32_t> _switch_lhsrhs_predicate = {
    {SVF::CmpStmt::Predicate::FCMP_OEQ,
     SVF::CmpStmt::Predicate::FCMP_OEQ}, // == -> ==
    {SVF::CmpStmt::Predicate::FCMP_UEQ,
     SVF::CmpStmt::Predicate::FCMP_UEQ}, // == -> ==
    {SVF::CmpStmt::Predicate::FCMP_OGT,
     SVF::CmpStmt::Predicate::FCMP_OLT}, // > -> <
    {SVF::CmpStmt::Predicate::FCMP_OGE,
     SVF::CmpStmt::Predicate::FCMP_OLE}, // >= -> <=
    {SVF::CmpStmt::Predicate::FCMP_OLT,
     SVF::CmpStmt::Predicate::FCMP_OGT}, // < -> >
    {SVF::CmpStmt::Predicate::FCMP_OLE,
     SVF::CmpStmt::Predicate::FCMP_OGE}, // <= -> >=
    {SVF::CmpStmt::Predicate::FCMP_ONE,
     SVF::CmpStmt::Predicate::FCMP_ONE}, // != -> !=
    {SVF::CmpStmt::Predicate::FCMP_UNE,
     SVF::CmpStmt::Predicate::FCMP_UNE}, // != -> !=
    {SVF::CmpStmt::Predicate::ICMP_EQ,
     SVF::CmpStmt::Predicate::ICMP_EQ}, // == -> ==
    {SVF::CmpStmt::Predicate::ICMP_NE,
     SVF::CmpStmt::Predicate::ICMP_NE}, // != -> !=
    {SVF::CmpStmt::Predicate::ICMP_UGT,
     SVF::CmpStmt::Predicate::ICMP_ULT}, // > -> <
    {SVF::CmpStmt::Predicate::ICMP_ULT,
     SVF::CmpStmt::Predicate::ICMP_UGT}, // < -> >
    {SVF::CmpStmt::Predicate::ICMP_UGE,
     SVF::CmpStmt::Predicate::ICMP_ULE}, // >= -> <=
    {SVF::CmpStmt::Predicate::ICMP_SGT,
     SVF::CmpStmt::Predicate::ICMP_SLT}, // > -> <
    {SVF::CmpStmt::Predicate::ICMP_SLT,
     SVF::CmpStmt::Predicate::ICMP_SGT}, // < -> >
    {SVF::CmpStmt::Predicate::ICMP_SGE,
     SVF::CmpStmt::Predicate::ICMP_SLE}, // >= -> <=
};

void Snapshot::initSVFVar(SVF::ValVar *valVar) {
    SVF::NodeID varId = valVar->getId();

    // Currently only handles ConstIntValVar and ConstNullPtrValVar
    if (const SVF::ConstIntValVar *consInt =
            SVF::SVFUtil::dyn_cast<SVF::ConstIntValVar>(valVar)) {
        SVF::s64_t numeral = consInt->getSExtValue();
        this->varState[varId] = ValueSet(numeral);
    } else if (SVF::SVFUtil::isa<SVF::ConstNullPtrValVar>(valVar)) {
        this->varState[varId] = ValueSet(0);
    }

    return;
}

/// @brief Finds any recursive functions. Also finds any loops within a
/// function, and stores them in weak topological order (WTO).
void VSA::initWTO() {
    SVF::AndersenWaveDiff *ander =
        SVF::AndersenWaveDiff::createAndersenWaveDiff(svfir);

    // Detect if the call graph has cycles by finding its strongly connected
    // components (SCC)
    SVF::Andersen::CallGraphSCC *callGraphScc = ander->getCallGraphSCC();
    callGraphScc->find();
    auto callGraph = ander->getCallGraph();

    // Iterate through the call graph
    for (auto it = callGraph->begin(); it != callGraph->end(); it++) {
        // Check if the current function is part of a cycle
        if (callGraphScc->isInCycle(it->second->getId()))
            this->recursiveFuns.insert(
                it->second->getFunction()); // Mark the function as recursive
    }

    // Initialize WTO for each function in the module
    for (const auto &item : *callGraph) {
        const SVF::FunObjVar *fun = item.second->getFunction();
        if (fun->isDeclaration())
            continue;

        auto *wto = new SVF::ICFGWTO(icfg->getFunEntryICFGNode(fun));
        wto->init();
        this->funcToWTO[fun] = wto;
    }

    for (auto fun : this->funcToWTO) {
        for (const SVF::ICFGWTOComp *comp : fun.second->getWTOComponents()) {
            if (const SVF::ICFGCycleWTO *cycle =
                    SVF::SVFUtil::dyn_cast<SVF::ICFGCycleWTO>(comp)) {
                this->cycleHeadToCycle[cycle->head()->getICFGNode()] = cycle;
            }
        }
    }
}

void VSA::handleGlobalNode() {
    const SVF::ICFGNode *globalNode = this->icfg->getGlobalICFGNode();

    // Run through each global variable assignment
    for (const SVF::SVFStmt *stmt : globalNode->getSVFStmts()) {
        updateAbsState(stmt);
    }

    // Now everything should be stored in this->blockState - move all
    // variables into this->globalState
    this->globalState = this->blockState.varState;
    this->blockState.varState.clear();
}

/// @brief Handle the `main` function (the entry point), which calls our
/// actual, Remill-lifted entry point. Assumes that `main` is defined in
/// a certain way:
/// ```ll
/// define i32 @main() {
///   %1 = call ptr @__lifter_init_memory()
///   %2 = call ptr @LIFTED.main(ptr @LIFTED.STATE, i64 4425, ptr %1)
///   %3 = load i32, ptr getelementptr inbounds (%struct.State, ptr
///   @LIFTED.STATE, i32 0, i32 0, i32 6, i32 1, i32 0, i32 0), align 4 call
///   void @__lifter_free_memory(ptr %1) ret i32 %3
/// }
/// ```
/// @param main the ObjVar representing `main`
void VSA::handleMainFunction(const SVF::FunObjVar *main) {
    // Navigate to proper ICFG node
    auto lifterMemoryNode =
        getNextNodes(this->svfir->getICFG()->getFunEntryICFGNode(main))[0];
    auto retLifterMemoryNode = getNextNodes(lifterMemoryNode)[0];
    auto callEntryIcfgNode = getNextNodes(retLifterMemoryNode)[0];

    // Extract PC parameter being called
    auto callEntryNode =
        SVF::SVFUtil::dyn_cast<SVF::CallICFGNode>(callEntryIcfgNode);
    auto initialPcVar = callEntryNode->getActualParms()[1];

    auto initialPcConst = this->globalState[initialPcVar->getId()];
    this->pc = initialPcConst.getConstant();

    // Continue on with AE for actual entry point
    auto funEntryNode =
        (*callEntryIcfgNode->getOutEdges().begin())->getDstNode();
    handleFunction(funEntryNode);
}

void VSA::analyse() {
    initWTO();

    handleGlobalNode();

    // Process the main function if it exists
    if (const SVF::FunObjVar *fun = svfir->getFunObjVar("main")) {
        handleMainFunction(fun);
    }
}

/// @brief Find the value set of an SVF variable, given a specific snapshot.
/// This function will also look through *global* variables.
/// @param id
/// @param snapshot
/// @return
ValueSet VSA::getSVFVarSet(SVF::NodeID id, Snapshot &snapshot) {
    // Find in globals
    auto globalsFind = this->globalState.find(id);

    if (globalsFind != this->globalState.end()) {
        return (*globalsFind).second;
    }

    return snapshot.getSVFVarSet(id);
}

/**
 * @brief Get the next nodes of a node
 *
 * Returns the next nodes of a node that are inside the same function.
 * And if CallICFGNode, shortcut to the RetICFGNode.
 *
 * @param node The node to get the next nodes of
 * @return The next nodes of the node
 */
std::vector<const SVF::ICFGNode *>
VSA::getNextNodes(const SVF::ICFGNode *node) const {
    std::vector<const SVF::ICFGNode *> outEdges;

    for (const SVF::ICFGEdge *edge : node->getOutEdges()) {
        const SVF::ICFGNode *dst = edge->getDstNode();
        // Only nodes inside the same function are included
        if (dst->getFun() == node->getFun()) {
            outEdges.push_back(dst);
        }
    }

    if (const SVF::CallICFGNode *callNode =
            SVF::SVFUtil::dyn_cast<SVF::CallICFGNode>(node)) {
        // Shortcut to the RetICFGNode
        const SVF::ICFGNode *retNode = callNode->getRetICFGNode();
        outEdges.push_back(retNode);
    }

    return outEdges;
}

/**
 * @brief Get the next nodes of a cycle
 *
 * Returns the next nodes of cycle components that are outside the cycle.
 * Inner cycles are skipped since their next nodes cannot be outside the outer
 * cycle. And Inner cycles are handled in the outer cycle. Only nodes that point
 * outside the cycle are included in cycleNext.
 *
 * @param cycle The cycle to get the next nodes of
 * @return The next nodes of the cycle
 */
std::vector<const SVF::ICFGNode *>
VSA::getNextNodesOfCycle(const SVF::ICFGCycleWTO *cycle) const {
    SVF::Set<const SVF::ICFGNode *> cycleNodes;

    // Insert the head of the cycle and the heads of the inner cycles
    cycleNodes.insert(cycle->head()->getICFGNode());

    for (const SVF::ICFGWTOComp *comp : cycle->getWTOComponents()) {
        if (const SVF::ICFGSingletonWTO *singleton =
                SVF::SVFUtil::dyn_cast<SVF::ICFGSingletonWTO>(comp)) {
            cycleNodes.insert(singleton->getICFGNode());
        } else if (const SVF::ICFGCycleWTO *subCycle =
                       SVF::SVFUtil::dyn_cast<SVF::ICFGCycleWTO>(comp)) {
            cycleNodes.insert(subCycle->head()->getICFGNode());
        }
    }

    std::vector<const SVF::ICFGNode *> outEdges;
    std::vector<const SVF::ICFGNode *> nextNodes =
        getNextNodes(cycle->head()->getICFGNode());

    for (const SVF::ICFGNode *nextNode : nextNodes) {
        // Only nodes that point outside the cycle are included
        if (cycleNodes.find(nextNode) == cycleNodes.end()) {
            outEdges.push_back(nextNode);
        }
    }

    for (const SVF::ICFGWTOComp *comp : cycle->getWTOComponents()) {
        if (const SVF::ICFGSingletonWTO *singleton =
                SVF::SVFUtil::dyn_cast<SVF::ICFGSingletonWTO>(comp)) {
            std::vector<const SVF::ICFGNode *> nextNodes =
                getNextNodes(singleton->getICFGNode());

            // Only nodes that point outside the cycle are included
            for (const SVF::ICFGNode *nextNode : nextNodes) {
                if (cycleNodes.find(nextNode) == cycleNodes.end()) {
                    outEdges.push_back(nextNode);
                }
            }
        } else if (const SVF::ICFGCycleWTO *subCycle =
                       SVF::SVFUtil::dyn_cast<SVF::ICFGCycleWTO>(comp)) {
            // skip inner cycle inside the outer cycle, because 1) it will be
            // handled in the outer cycle.
            // 2) its next nodes won't be outside the outer cycle.
            continue;
        }
    }

    return outEdges;
}

/**
 * @brief  Propagate the states from predecessors to the current node and return
 * true if the control-flow is feasible
 *
 * This function attempts to propagate the execution state to a given block by
 * merging the states of its predecessor blocks. It handles two scenarios:
 * intra-block edges and call edges. Scenario 1: preblock -----(intraEdge)---->
 * block, join the preES of inEdges Scenario 2: preblock -----(callEdge)---->
 * block If the propagation is feasible, it updates the execution state and
 * returns true. Otherwise, it returns false.
 *
 * Assumes that the node being checked is at the beginning of a basic block.
 *
 * @param node The ICFG node for which the state propagation is
 * attempted
 * @return bool True if the state propagation is feasible and successful, false
 * otherwise
 */
bool VSA::mergeStatesFromPredecessors(const SVF::ICFGNode *node,
                                      AbstractStore &as) {
    u32_t inEdgeNum = 0;
    as = AbstractStore();

    // Iterate over all incoming edges of the given block
    for (auto &edge : node->getInEdges()) {
        // Check if the source node of the edge has a post-execution state
        // recorded
        if (this->postBasicBlock.find(edge->getSrcNode()) !=
            this->postBasicBlock.end()) {
            Snapshot temp = this->postBasicBlock[edge->getSrcNode()];
            // Regardless of whether the branch is feasible or not, the
            // `NEXT_PC` has to be the same
            this->nextPc = temp.nextPc;

            const SVF::IntraCFGEdge *intraCfgEdge =
                SVF::SVFUtil::dyn_cast<SVF::IntraCFGEdge>(edge);

            // If the edge is an intra-block edge and has a condition
            if (intraCfgEdge && intraCfgEdge->getCondition()) {
                // Check if the branch condition is feasible
                if (isBranchFeasible(intraCfgEdge, temp)) {
                    // Merge the state with the current state
                    as.joinWith(temp.abstractStore);
                    inEdgeNum++;
                }
                // If branch is not feasible, do nothing
            } else {
                // For non-conditional edges, directly merge the state
                as.joinWith(
                    this->postBasicBlock[edge->getSrcNode()].abstractStore);
                inEdgeNum++;
            }
        }
        // If no post-execution state is recorded for the source node, do
        // nothing
    }

    // If no incoming edges have feasible states, return false
    if (inEdgeNum == 0) {
        return false;
    } else {
        return true;
    }

    assert(false && "implement this part"); // This part should not be reached
}

/// @brief Check if we can actually go down this branch at this point
/// @param intraEdge transition edge
/// @param trace the abstract trace post-previous block
/// @return
bool VSA::isBranchFeasible(const SVF::IntraCFGEdge *intraEdge,
                           Snapshot &blockState) {
    const SVF::SVFVar *cmpVar = intraEdge->getCondition();
    assert(!cmpVar->getInEdges().empty() && "no in edges?");
    SVF::SVFStmt *cmpVarInStmt = *cmpVar->getInEdges().begin();

    // No switch statements in lifted binaries
    const SVF::CmpStmt *cmpStmt =
        SVF::SVFUtil::dyn_cast<SVF::CmpStmt>(cmpVarInStmt);
    return isCmpBranchFeasible(cmpStmt, intraEdge->getSuccessorCondValue(),
                               blockState.varState);
}

/// @brief
/// @param cmpStmt
/// @param succ
/// @param preds
/// @return
bool VSA::isCmpBranchFeasible(const SVF::CmpStmt *cmpStmt, SVF::s64_t succ,
                              SVFVarState &varState) {
    SVFVarState newVarState = varState;

    // get cmp stmt's op0, op1, and predicate
    SVF::NodeID op0 = cmpStmt->getOpVarID(0);
    SVF::NodeID op1 = cmpStmt->getOpVarID(1);
    SVF::NodeID res_id = cmpStmt->getResID();
    SVF::s32_t predicate = cmpStmt->getPredicate();

    // if op0 or op1 is undefined, return;
    // skip address compare
    if ((this->globalState.find(op0) == this->globalState.end() &&
         newVarState.find(op0) == newVarState.end()) ||
        (this->globalState.find(op1) == this->globalState.end() &&
         newVarState.find(op1) == newVarState.end())) {
        varState = newVarState;
        return true;
    }

    // get '%1 = load i32 s', and load inst may not exist
    auto getLoadOp = [](SVF::SVFVar *opVar) -> const SVF::LoadStmt * {
        if (!opVar->getInEdges().empty()) {
            SVF::SVFStmt *loadVar0InStmt = *opVar->getInEdges().begin();

            if (const SVF::LoadStmt *loadStmt =
                    SVF::SVFUtil::dyn_cast<SVF::LoadStmt>(loadVar0InStmt)) {
                return loadStmt;
            } else if (const SVF::CopyStmt *copyStmt =
                           SVF::SVFUtil::dyn_cast<SVF::CopyStmt>(
                               loadVar0InStmt)) {
                if (!copyStmt->getRHSVar()->getInEdges().empty()) {
                    SVF::SVFStmt *loadVar0InStmt2 =
                        *opVar->getInEdges().begin();

                    if (const SVF::LoadStmt *loadStmt =
                            SVF::SVFUtil::dyn_cast<SVF::LoadStmt>(
                                loadVar0InStmt2)) {
                        return loadStmt;
                    }
                }
            }
        }

        return nullptr;
    };

    const SVF::LoadStmt *load_op0 = getLoadOp(svfir->getGNode(op0));
    const SVF::LoadStmt *load_op1 = getLoadOp(svfir->getGNode(op1));

    // for const X const, we may get concrete resVal instantly
    // for var X const, we may get [0,1] if the intersection of var and const is
    // not empty set
    std::cout << "Result ID: " << res_id << std::endl;

    for (auto kv : newVarState) {
        std::cout << "Node " << kv.first << " has value "
                  << kv.second.getGlobal().toString() << std::endl;
    }

    RIC resVal = newVarState[res_id].getGlobal();
    std::cout << "Result RIC: " << resVal.toString() << std::endl;
    RIC succRic(succ);
    std::cout << "Successor RIC: " << succRic.toString() << std::endl;
    resVal.meetWith(succRic);
    std::cout << "Meet result: " << resVal.toString() << std::endl;

    // If Var X const generates bottom value, it means this branch path is not
    // feasible.
    if (resVal.isBottom()) {
        return false;
    }

    bool b0 = newVarState[op0].getGlobal().isConstant();
    bool b1 = newVarState[op1].getGlobal().isConstant();

    // if const X var, we should reverse op0 and op1.
    if (b0 && !b1) {
        std::swap(op0, op1);
        std::swap(load_op0, load_op1);
        predicate = _switch_lhsrhs_predicate[predicate];
    } else {
        // if var X var, we cannot preset the branch condition to infer the
        // intervals of var0,var1
        if (!b0 && !b1) {
            varState = newVarState;
            return true;
        }
        // if const X const, we can instantly get the resVal
        else if (b0 && b1) {
            varState = newVarState;
            return true;
        }
    }

    // if cmp is 'var X const == false', we should reverse predicate 'var X'
    // const == true' X' is reverse predicate of X
    if (succ == 0) {
        predicate = _reverse_predicate[predicate];
    } else {
    }

    // change interval range according to the compare predicate
    // AddressValue addrs;
    // if (load_op0 && newPreds.inVarToAddrsTable(load_op0->getRHSVarID()))
    //     addrs = newPreds[load_op0->getRHSVarID()].getAddrs();
    RIC lhs = newVarState[op0].getGlobal();
    RIC rhs = newVarState[op1].getGlobal();

    switch (predicate) {
    case SVF::CmpStmt::Predicate::ICMP_EQ:
    case SVF::CmpStmt::Predicate::FCMP_OEQ:
    case SVF::CmpStmt::Predicate::FCMP_UEQ: {
        // Var == Const, so [var.lb, var.ub].meet_with(const)
        lhs.meetWith(rhs);
        break;
    }
    /*
    case SVF::CmpStmt::Predicate::ICMP_NE:
    case SVF::CmpStmt::Predicate::FCMP_ONE:
    case SVF::CmpStmt::Predicate::FCMP_UNE:
        // Compliment set
        break;
    */
    case SVF::CmpStmt::Predicate::ICMP_UGT:
    case SVF::CmpStmt::Predicate::ICMP_SGT:
    case SVF::CmpStmt::Predicate::FCMP_OGT:
    case SVF::CmpStmt::Predicate::FCMP_UGT: {
        // Var > Const, so [var.lb, var.ub].meet_with([Const+1, +INF])
        RIC meetValue(1, rhs.lower() + 1, SVF::BoundedInt::plus_infinity(), 0);
        lhs.meetWith(meetValue);
        break;
    }
    /*
    case SVF::CmpStmt::Predicate::ICMP_UGE:
    case SVF::CmpStmt::Predicate::ICMP_SGE:
    case SVF::CmpStmt::Predicate::FCMP_OGE:
    case SVF::CmpStmt::Predicate::FCMP_UGE: {
        // Var >= Const, so [var.lb, var.ub].meet_with([Const, +INF])
        lhs.meet_with(IntervalValue(rhs.lb(), IntervalValue::plus_infinity()));
        break;
    }
    case SVF::CmpStmt::Predicate::ICMP_ULT:
    case SVF::CmpStmt::Predicate::ICMP_SLT:
    case SVF::CmpStmt::Predicate::FCMP_OLT:
    case SVF::CmpStmt::Predicate::FCMP_ULT: {
        // Var < Const, so [var.lb, var.ub].meet_with([-INF, const.ub-1])
        lhs.meet_with(
            IntervalValue(IntervalValue::minus_infinity(), rhs.ub() - 1));
        break;
    }
    */
    case SVF::CmpStmt::Predicate::ICMP_ULE:
    case SVF::CmpStmt::Predicate::ICMP_SLE:
    case SVF::CmpStmt::Predicate::FCMP_OLE:
    case SVF::CmpStmt::Predicate::FCMP_ULE: {
        // Var <= Const, so [var.lb, var.ub].meet_with([-INF, const.ub])
        RIC meetValue(1, SVF::BoundedInt::minus_infinity(), rhs.upper(), 0);
        lhs.meetWith(meetValue);
        break;
    }
        /*
        case SVF::CmpStmt::Predicate::FCMP_FALSE:
            break;
        case SVF::CmpStmt::Predicate::FCMP_TRUE:
            break;
        default:
            assert(false && "implement this part");
            abort();
        */
    }

    newVarState[op0].values[0] = lhs;

    /*
    for (const auto &addr : addrs) {
        SVF::NodeID objId = preds.getIDFromAddr(addr);
        if (newPreds.inAddrToValTable(objId)) {
            switch (predicate) {
            case SVF::CmpStmt::Predicate::ICMP_EQ:
            case SVF::CmpStmt::Predicate::FCMP_OEQ:
            case SVF::CmpStmt::Predicate::FCMP_UEQ: {
                newPreds.load(addr).meet_with(rhs);
                break;
            }
            case SVF::CmpStmt::Predicate::ICMP_NE:
            case SVF::CmpStmt::Predicate::FCMP_ONE:
            case SVF::CmpStmt::Predicate::FCMP_UNE:
                // Compliment set
                break;
            case SVF::CmpStmt::Predicate::ICMP_UGT:
            case SVF::CmpStmt::Predicate::ICMP_SGT:
            case SVF::CmpStmt::Predicate::FCMP_OGT:
            case SVF::CmpStmt::Predicate::FCMP_UGT:
                // Var > Const, so [var.lb, var.ub].meet_with([Const+1, +INF])
                newPreds.load(addr).meet_with(IntervalValue(
                    rhs.lb() + 1, IntervalValue::plus_infinity()));
                break;
            case SVF::CmpStmt::Predicate::ICMP_UGE:
            case SVF::CmpStmt::Predicate::ICMP_SGE:
            case SVF::CmpStmt::Predicate::FCMP_OGE:
            case SVF::CmpStmt::Predicate::FCMP_UGE: {
                // Var >= Const, so [var.lb, var.ub].meet_with([Const, +INF])
                newPreds.load(addr).meet_with(
                    IntervalValue(rhs.lb(), IntervalValue::plus_infinity()));
                break;
            }
            case SVF::CmpStmt::Predicate::ICMP_ULT:
            case SVF::CmpStmt::Predicate::ICMP_SLT:
            case SVF::CmpStmt::Predicate::FCMP_OLT:
            case SVF::CmpStmt::Predicate::FCMP_ULT: {
                // Var < Const, so [var.lb, var.ub].meet_with([-INF,
                // const.ub-1])
                newPreds.load(addr).meet_with(IntervalValue(
                    IntervalValue::minus_infinity(), rhs.ub() - 1));
                break;
            }
            case SVF::CmpStmt::Predicate::ICMP_ULE:
            case SVF::CmpStmt::Predicate::ICMP_SLE:
            case SVF::CmpStmt::Predicate::FCMP_OLE:
            case SVF::CmpStmt::Predicate::FCMP_ULE: {
                // Var <= Const, so [var.lb, var.ub].meet_with([-INF, const.ub])
                newPreds.load(addr).meet_with(
                    IntervalValue(IntervalValue::minus_infinity(), rhs.ub()));
                break;
            }
            case SVF::CmpStmt::Predicate::FCMP_FALSE:
                break;
            case SVF::CmpStmt::Predicate::FCMP_TRUE:
                break;
            default:
                assert(false && "implement this part");
                abort();
            }
        }
    }
    */

    varState = newVarState;
    return true;
}

bool VSA::isStartOfBasicBlock(const SVF::ICFGNode *node) {
    for (auto &edge : node->getInEdges()) {
        auto srcNode = edge->getSrcNode();
        auto srcStmts = srcNode->getSVFStmts();

        if (srcStmts.empty()) {
            return false;
        }

        // We only care about starts of basic blocks, i.e. nodes that
        // come from branch statements.
        if (!SVF::SVFUtil::isa<SVF::BranchStmt>(srcStmts.front())) {
            return false;
        }
    }

    return true;
}

bool VSA::isStartOfRetBlock(const SVF::ICFGNode *node) {
    if (!isStartOfBasicBlock(node)) {
        return false;
    }

    const SVF::ICFGNode *check = node;

    while (true) {
        auto nextNodes = getNextNodes(check);

        if (nextNodes.size() != 1 &&
            !SVF::SVFUtil::isa<SVF::CallICFGNode>(check)) {
            // If we have no next nodes, then we've reached the end of our
            // graph. If we have more than one next node, we've reached a
            // branch. There is an edgecase where the same `RetICFGNode` is
            // added *twice* for each external call, so that must be handled
            break;
        }

        check = nextNodes[0];
        if (SVF::SVFUtil::isa<SVF::FunExitICFGNode>(check)) {
            return true;
        }

        auto stmts = check->getSVFStmts();
        if (stmts.empty()) {
            continue;
        }

        if (SVF::SVFUtil::isa<SVF::BranchStmt>(stmts.front())) {
            break;
        }
    }

    return false;
}

const SVF::ICFGNode *VSA::getBlockEnd(const SVF::ICFGNode *start) {
    const SVF::ICFGNode *blockEnd = start;

    while (true) {
        auto stmts = blockEnd->getSVFStmts();
        if (stmts.empty()) {
            blockEnd = getNextNodes(blockEnd)[0];
            continue;
        }

        auto stmt = blockEnd->getSVFStmts().front();
        if (stmt->getEdgeKind() == SVF::SVFStmt::Branch) {
            break;
        }

        blockEnd = getNextNodes(blockEnd)[0];
    }

    return blockEnd;
}

const SVF::ICFGNode *VSA::skipBlocks(const SVF::ICFGNode *start, size_t n) {
    const SVF::ICFGNode *pastSkippedBlocks = start;
    size_t skippedBlocks = 0;

    while (skippedBlocks < n) {
        auto stmts = pastSkippedBlocks->getSVFStmts();
        if (stmts.empty()) {
            pastSkippedBlocks = getNextNodes(pastSkippedBlocks)[0];
            continue;
        }

        auto stmt = pastSkippedBlocks->getSVFStmts().front();

        if (stmt->getEdgeKind() == SVF::SVFStmt::Branch) {
            skippedBlocks++;
        }

        pastSkippedBlocks = getNextNodes(pastSkippedBlocks)[0];
    }

    return pastSkippedBlocks;
}

/// @brief Given the node which represents the beginning of the basic block
/// that sets RSP to an offset of RBP, update this->blockState to contain
/// this information
/// @param rspNode
void VSA::handleFunctionStart(const SVF::ICFGNode *rspNode) {
    // Remill lifting the `sub` instruction will always look like this:
    /*
     * %24 = load i64, ptr %NEXT_PC, align 8
     * store i64 %24, ptr %PC, align 8
     * %25 = add i64 %24, 4
     * store i64 %25, ptr %NEXT_PC, align 8
     * %26 = load i64, ptr %RSP, align 8
     * %27 = load ptr, ptr %MEMORY, align 8
     * %28 = sub i64 %26, 96
     * store i64 %28, ptr %RSP, align 8
     */
    // Thus, we skip the first 6 instructions
    const SVF::ICFGNode *subNode = rspNode;
    const size_t INSTRUCTIONS_TO_SKIP = 6;

    for (int i = 0; i < INSTRUCTIONS_TO_SKIP; i++) {
        subNode = getNextNodes(subNode)[0];
    }

    auto uncastSubStmt = subNode->getSVFStmts().front();
    auto subStmt = SVF::SVFUtil::dyn_cast<SVF::BinaryOPStmt>(uncastSubStmt);

    ValueSet stackSizeSet = this->globalState[subStmt->getOpVarID(1)];

    // Blocks 1, 2 and 3 are skipped, which contain 8 bytes total
    // Block 3, which handles the offset, has 4 bytes
    this->nextPc = this->pc + 12;

    this->blockState.procStartPc = this->pc;
    this->blockState.nextPc = this->nextPc;
    this->blockState.stackSize = stackSizeSet.getConstant();
}

void VSA::handleFunctionEnd() {}

void VSA::handleFunction(const SVF::ICFGNode *funEntry) {
    // We will need to skip past the first *four* basic blocks:
    // - Block 0 is generated by Remill to initialise our registers
    // - Block 1 is `endbr64`, which prevents ROP attacks - we don't care about
    // this for static analysis
    // - Block 2 is `push rbp`, which stores the previous stack pointer - since
    // stack pointers are treated as arbitrary under VSA, we also don't care
    // about this
    // - Block 3 is `mov rbp, rsp`, which does some stack adjusting
    const SVF::ICFGNode *pastSkippedBlocks = getNextNodes(funEntry)[0];
    pastSkippedBlocks = skipBlocks(pastSkippedBlocks, 4);

    // Now `pastSkippedBlocks` is at the start of the block where the stack
    // pointer offset is calculated - call `handleFunctionStart` here
    handleFunctionStart(pastSkippedBlocks);

    // Skip to the end of that block, to set its `this->postBasicBlock` state
    const SVF::ICFGNode *endPrevBlock = getBlockEnd(pastSkippedBlocks);

    Snapshot snapshot = this->blockState;
    std::cout << "Stack size: " << snapshot.stackSize << std::endl;
    this->postBasicBlock.insert({endPrevBlock, snapshot});

    pastSkippedBlocks = getNextNodes(endPrevBlock)[0];

    // Begin function analysis
    SVF::FIFOWorkList<const SVF::ICFGNode *> worklist;
    worklist.push(pastSkippedBlocks);

    while (!worklist.empty()) {
        const SVF::ICFGNode *node = worklist.pop();

        if (this->cycleHeadToCycle.find(node) != this->cycleHeadToCycle.end()) {
            // The current node is the "head" of a cycle
            const SVF::ICFGCycleWTO *cycle = this->cycleHeadToCycle[node];
            handleICFGCycle(cycle);

            std::vector<const SVF::ICFGNode *> cycleNextNodes =
                getNextNodesOfCycle(cycle);

            for (const SVF::ICFGNode *nextNode : cycleNextNodes) {
                worklist.push(nextNode);
            }
        } else {
            if (isStartOfRetBlock(node)) {
                handleFunctionEnd();
            }

            if (!handleICFGNode(node)) {
                SVF::SVFUtil::errs()
                    << "Fixpoint reached or infeasible for node "
                    << node->getId() << "\n";
                continue;
            }

            std::vector<const SVF::ICFGNode *> nextNodes = getNextNodes(node);

            for (const SVF::ICFGNode *nextNode : nextNodes) {
                worklist.push(nextNode);
            }
        }
    }
}

/**
 * @brief Handle ICFG nodes in a cycle using widening and narrowing operators
 *
 * This function implements abstract interpretation for cycles in the ICFG using
 * widening and narrowing operators to ensure termination. It processes all ICFG
 * nodes within a cycle and implements widening-narrowing iteration to reach
 * fixed points twice: once for widening (to ensure termination) and once for
 * narrowing (to improve precision).
 *
 * @param cycle The WTO cycle containing ICFG nodes to be processed
 * @return void
 */
void VSA::handleICFGCycle(const SVF::ICFGCycleWTO *cycle) {
    // Get execution states from in edges
    const SVF::ICFGNode *head = cycle->head()->getICFGNode();

    bool is_feasible = mergeStatesFromPredecessors(
        head, this->preBasicBlock[head].abstractStore);

    if (!is_feasible) {
        return;
    } else {
        // TODO: set to SVF::Options::WidenDelay() properly as we move past `pc`
        SVF::s32_t widen_delay = 1;

        // 1. Handle all nodes in cycle `widen_delay` times
        for (SVF::s32_t i = 0; i < widen_delay; i++) {
            handleICFGNode(head);

            // Handle all other nodes in cycle
            for (auto comp : cycle->getWTOComponents()) {
                if (comp->getKind() == SVF::ICFGCycleWTO::Node) {
                    handleICFGNode(
                        SVF::SVFUtil::dyn_cast<SVF::ICFGSingletonWTO>(comp)
                            ->getICFGNode());
                } else {
                    handleICFGCycle(
                        SVF::SVFUtil::dyn_cast<SVF::ICFGCycleWTO>(comp));
                }
            }
        }

        AbstractStore preAs = this->preBasicBlock[head].abstractStore;
        bool increasing = true;

        // 2. Check if increasing - if so, then widen, if not repeat step 2
        // 3. Narrow
        while (true) {
            AbstractStore curAs;
            mergeStatesFromPredecessors(head, curAs);

            if (increasing) {
                AbstractStore widened = preAs;
                widened.widenWith(curAs);
                curAs = widened;

                if (widened == preAs) {
                    // We've reached a fixed point!
                    increasing = false;
                    continue;
                }

                std::cout << "Abstract store widened" << std::endl;
            } else {
                // We now narrow
                std::cout << "Before narrowing: " << preAs.alocs[ALoc{1, 12, 4}].getGlobal().toString() << std::endl;
                AbstractStore narrowed = preAs;
                narrowed.narrowWith(curAs);
                curAs = narrowed;
                std::cout << "After narrowing: " << curAs.alocs[ALoc{1, 12, 4}].getGlobal().toString() << std::endl;

                if (narrowed == preAs) {
                    // We've reached the narrow fixed point
					break;
				}

                std::cout << "Abstract store narrowed" << std::endl;
            }

            // Handle head
            this->preBasicBlock[head].abstractStore = curAs;
            this->blockState.abstractStore = this->preBasicBlock[head].abstractStore;

            for (const SVF::SVFStmt *stmt : head->getSVFStmts()) {
                updateAbsState(stmt);
            }

            if (const SVF::CallICFGNode *callNode =
                    SVF::SVFUtil::dyn_cast<SVF::CallICFGNode>(head)) {
                handleCallSite(callNode);
            }

            // Handle all other nodes in cycle
            for (auto comp : cycle->getWTOComponents()) {
                if (comp->getKind() == SVF::ICFGCycleWTO::Node) {
                    handleICFGNode(
                        SVF::SVFUtil::dyn_cast<SVF::ICFGSingletonWTO>(comp)
                            ->getICFGNode());
                } else {
                    handleICFGCycle(
                        SVF::SVFUtil::dyn_cast<SVF::ICFGCycleWTO>(comp));
                }
            }

            preAs = curAs;
        }
    }
}

/**
 * @brief Handle a node in the ICFG
 *
 * This function handles a node in the ICFG by merging the abstract states of
 * its predecessors, updating the abstract state based on the node's statements,
 * and handling stub functions. It also checks if the abstract state has reached
 * a fixpoint and returns the result. Return true means the abstract state has
 * changed Return false means the abstract state has reached a fixpoint or is
 * infeasible
 *
 * @param node The node to be handled
 * @return True if it is feasible, false if it is infeasible
 */
bool VSA::handleICFGNode(const SVF::ICFGNode *node) {
    bool isStart = isStartOfBasicBlock(node);

    if (isStart) {
        std::cout << "Going through starting node " << node->getId() << std::endl;
        AbstractStore tmpEs;
        bool is_feasible = mergeStatesFromPredecessors(node, tmpEs);

        if (!is_feasible) {
            SVF::SVFUtil::errs()
                << "Infeasible for node " << node->getId() << "\n";
            return false;
        }

        this->preBasicBlock[node].abstractStore = tmpEs;
        this->blockState.abstractStore = this->preBasicBlock[node].abstractStore;

        for (const SVF::SVFStmt *stmt : node->getSVFStmts()) {
            updateAbsState(stmt);
        }

        if (const SVF::CallICFGNode *callNode =
                SVF::SVFUtil::dyn_cast<SVF::CallICFGNode>(node)) {
            handleCallSite(callNode);
        }
    } else {
        std::cout << "Going through node " << node->getId() << std::endl;
        // If we're not in the start of a basic block, then the abstract
        // state updating/fixpoint-checking doesn't apply
        for (const SVF::SVFStmt *stmt : node->getSVFStmts()) {
            updateAbsState(stmt);
        }

        if (const SVF::CallICFGNode *callNode =
                SVF::SVFUtil::dyn_cast<SVF::CallICFGNode>(node)) {
            handleCallSite(callNode);
        }
    }

    return true;
}

void VSA::handleRemillRead(SVF::NodeID retId, const SVF::FunObjVar *callee) {
    ValueSet vs1;
    vs1.values[1] = RIC(48);

    ValueSet vs10;
    vs10.values[0] = RIC(1, 0, 1, 0);

    ValueSet vs18;
    vs18.values[0] = RIC(1, 0, SVF::BoundedInt::plus_infinity(), 0);

    ValueSet vs26;
    vs26.values[0] = RIC(1, 0, 5, 0);

    ValueSet vs27;
    vs27.values[1] = RIC(48);

    const size_t TOTAL_READS = 33;
    ValueSet mockValueSets[TOTAL_READS] = {
        // mov rax, QWORD PTR fs:0x28
        ValueSet(0),
        // lea rax, [rbp-0x30]
        // mov QWORD PTR [rbp-0x48], rax
        // mov rax, QWORD PTR [rbp-0x48]
        vs1,
        // == First loop runthrough ==
        // cmp DWORD PTR [rbp-0x54], 0x4
        ValueSet(0),
        // mov rax, QWORD PTR [rbp-0x48]
        ValueSet(0),
        // mov edx, DWORD PTR [rbp-0x50]
        ValueSet(0),
        // mov rax, QWORD PTR [rbp-0x40]
        ValueSet(0),
        // mov edx, DWORD PTR [rbp-0x4c]
        ValueSet(0),
        // add QWORD PTR [rbp-0x48], 0x4
        ValueSet(0),
        // add QWORD PTR [rbp-0x40], 0x4
        ValueSet(0),
        // add DWORD PTR [rbp-0x54], 0x1
        ValueSet(0),
        // Reads 10 to 17 is where we widen
        vs10,
        ValueSet(0),
        ValueSet(0),
        ValueSet(0),
        ValueSet(0),
        ValueSet(0),
        ValueSet(0),
        ValueSet(0),
        // Reads 18 to 25 is where we narrow
        vs18,
        ValueSet(0),
        ValueSet(0),
        ValueSet(0),
        ValueSet(0),
        ValueSet(0),
        ValueSet(0),
        ValueSet(0),
        // For loop check
        vs26,
        // mov rax, QWORD PTR [rbp-0x38]
        vs27,
        // mov eax, DWORD PTR [rax]
        ValueSet(5),
        // mov rax, QWORD PTR fs:0x28
        // mov QWORD PTR [rbp-0x8], rax
        // ...
        // mov rdx, QWORD PTR [rbp-0x8]
        ValueSet(0),
        // sub rdx, QWORD PTR fs:0x28
        ValueSet(0),
        // leave
        ValueSet(0),
        // ret
        ValueSet(0),
    };

    this->blockState.varState[retId] = mockValueSets[this->readCalls];
    std::cout << "this->readCalls = " << this->readCalls << std::endl;
    this->readCalls++;
}

void VSA::handleRemillWrite(const SVF::FunObjVar *callee) {
    // For vsa.ll, represents address [rbp-54] of size 4,
    // which contains an incrementing index
    ALoc aloc_m54{1, 12, 4};

    // Initial ALocs
    std::map<ALoc, ValueSet> alocs4;
    alocs4.insert({aloc_m54, 0});

    // After one iteration of loop
    ValueSet vs1;
    vs1.values[0] = RIC(1, 0, 1, 0);
    std::map<ALoc, ValueSet> alocs11;
    alocs11.insert({aloc_m54, vs1});

    // After widening
    ValueSet vs5;
    vs5.values[0] = RIC(1, 0, 5, 0);
    std::map<ALoc, ValueSet> alocs16;
    alocs16.insert({aloc_m54, vs5});

    if (this->writeCalls == 4) {
        this->blockState.abstractStore.alocs = alocs4;
    } else if (this->writeCalls == 11) {
        this->blockState.abstractStore.alocs = alocs11;
    } else if (this->writeCalls == 16) {
        this->blockState.abstractStore.alocs = alocs16;
    }

    std::cout << "this->writeCalls = " << this->writeCalls << std::endl;
    this->writeCalls++;
}

/**
 * @brief Handle a call site in the control flow graph
 *
 * This function processes a call site by updating the abstract state, handling
 * the called function, and managing the call stack. It resumes the execution
 * state after the function call.
 *
 * @param node The call site node to be handled
 */
void VSA::handleCallSite(const SVF::CallICFGNode *callNode) {
    // Get the callee function associated with the call site
    const SVF::FunObjVar *callee = callNode->getCalledFunction();
    std::string fun_name = callee->getName();

    // TODO: implement *actual* read and write
    if (fun_name.rfind("__remill_read_memory", 0) == 0) {
        SVF::NodeID retId = callNode->getRetICFGNode()->getActualRet()->getId();
        handleRemillRead(retId, callee);
    } else if (fun_name.rfind("__remill_write_memory", 0) == 0) {
        handleRemillWrite(callee);
    } else if (SVF::SVFUtil::isExtCall(callee)) {
        // `@EXTERNAL.` calls
        updateStateOnExtCall(callNode);
    } else if (recursiveFuns.find(callee) != recursiveFuns.end()) {
        // skip recursive functions
        return;
    } else {
        // Handle the callee function
        handleFunction(svfir->getICFG()->getFunEntryICFGNode(callee));
    }
}

/**
 * @brief Handle state updates for each type of SVF statement
 *
 * This function updates the abstract state based on the type of SVF statement
 * provided. It dispatches the handling of each specific statement type to the
 * corresponding update function.
 *
 * @param stmt The SVF statement for which the state needs to be updated
 */
void VSA::updateAbsState(const SVF::SVFStmt *stmt) {
    // Handle address statements
    if (const SVF::AddrStmt *addr =
            SVF::SVFUtil::dyn_cast<SVF::AddrStmt>(stmt)) {
        updateStateOnAddr(addr);
    }
    // Handle binary operation statements
    else if (const SVF::BinaryOPStmt *binary =
                 SVF::SVFUtil::dyn_cast<SVF::BinaryOPStmt>(stmt)) {
        updateStateOnBinary(binary);
    }
    // Handle comparison statements
    else if (const SVF::CmpStmt *cmp =
                 SVF::SVFUtil::dyn_cast<SVF::CmpStmt>(stmt)) {
        updateStateOnCmp(cmp);
    }
    // Handle load statements
    else if (const SVF::LoadStmt *load =
                 SVF::SVFUtil::dyn_cast<SVF::LoadStmt>(stmt)) {
        updateStateOnLoad(load);
    }
    // Handle store statements
    else if (const SVF::StoreStmt *store =
                 SVF::SVFUtil::dyn_cast<SVF::StoreStmt>(stmt)) {
        updateStateOnStore(store);
    }
    // Handle copy statements
    else if (const SVF::CopyStmt *copy =
                 SVF::SVFUtil::dyn_cast<SVF::CopyStmt>(stmt)) {
        updateStateOnCopy(copy);
    }
    // Handle GEP (GetElementPtr) statements
    else if (const SVF::GepStmt *gep =
                 SVF::SVFUtil::dyn_cast<SVF::GepStmt>(stmt)) {
        // The only useful thing that GEP statements are used for is to extract
        // register values. However, the rgisters are already named for us, so
        // we don't actually need to handle them
    }
    // Handle phi statements
    else if (const SVF::PhiStmt *phi =
                 SVF::SVFUtil::dyn_cast<SVF::PhiStmt>(stmt)) {
        // We will not run into phi statements in lifted binaries
    }
    // Handle call procedure entries
    else if (const SVF::CallPE *callPE =
                 SVF::SVFUtil::dyn_cast<SVF::CallPE>(stmt)) {
        updateStateOnCall(callPE);
    }
    // Handle return procedure entries
    else if (const SVF::RetPE *retPE =
                 SVF::SVFUtil::dyn_cast<SVF::RetPE>(stmt)) {
        updateStateOnRet(retPE);
    }
    // Handle select statements
    else if (const SVF::SelectStmt *select =
                 SVF::SVFUtil::dyn_cast<SVF::SelectStmt>(stmt)) {
        updateStateOnSelect(select);
    }
    // Handle branch statements
    else if (const SVF::BranchStmt *branch =
                 SVF::SVFUtil::dyn_cast<SVF::BranchStmt>(stmt)) {
        updateStateOnBranch(branch);
    }
    // Handle unary operations and branch statements (no action needed)
    else if (SVF::SVFUtil::isa<SVF::UnaryOPStmt>(stmt)) {
        // Nothing needs to be done for branch statements specifically
    }
    // Assert false for unsupported statement types
    else {
        assert(false && "implement this part");
    }
}

/// Abstract state updates on an AddrStmt
void VSA::updateStateOnAddr(const SVF::AddrStmt *addr) {
    if (addr->getRHSVarID() == 3) {
        SVF::ValVar *valVar =
            SVF::SVFUtil::cast<SVF::ValVar>(addr->getLHSVar());
        this->blockState.initSVFVar(valVar);
    }
}

/// Abstract state updates on an CallPE
void VSA::updateStateOnCall(const SVF::CallPE *call) {
    /*
    const SVF::ICFGNode *node = call->getICFGNode();
    AbstractState &as = getAbsStateFromTrace(node);
    SVF::NodeID lhs = call->getLHSVarID();
    SVF::NodeID rhs = call->getRHSVarID();
    as[lhs] = as[rhs];
    */
}

/// Abstract state updates on an RetPE
void VSA::updateStateOnRet(const SVF::RetPE *retPE) {
    /*
    const SVF::ICFGNode *node = retPE->getICFGNode();
    AbstractState &as = getAbsStateFromTrace(node);
    SVF::NodeID lhs = retPE->getLHSVarID();
    SVF::NodeID rhs = retPE->getRHSVarID();
    as[lhs] = as[rhs];
    */
}

void VSA::updateStateOnCopy(const SVF::CopyStmt *copy) {
    /*
    const SVF::ICFGNode *node = copy->getICFGNode();
    AbstractState &as = getAbsStateFromTrace(node);
    as[copy->getLHSVarID()] = as[copy->getRHSVarID()];
    */
}

/// Find the comparison predicates in "class SVF::BinaryOPStmt:OpCode" under
/// SVF/svf/include/SVFIR/SVFStatements.h You are required to handle predicates
/// (The program is assumed to have signed ints and also
/// interger-overflow-free), including Add, FAdd, Sub, FSub, Mul, FMul, SDiv,
/// FDiv, UDiv, SRem, FRem, URem, Xor, And, Or, AShr, Shl, LShr
void VSA::updateStateOnBinary(const SVF::BinaryOPStmt *binary) {
    /*
    const SVF::ICFGNode *node = binary->getICFGNode();
    AbstractState &as = getAbsStateFromTrace(node);

    auto resID = binary->getResID();
    auto left = binary->getOpVarID(0);
    auto right = binary->getOpVarID(1);

    switch (binary->getOpcode()) {
    case SVF::BinaryOPStmt::Add:
    case SVF::BinaryOPStmt::FAdd:
        as[resID] = as[left].getInterval() + as[right].getInterval();
        break;
    case SVF::BinaryOPStmt::Sub:
    case SVF::BinaryOPStmt::FSub:
        as[resID] = as[left].getInterval() - as[right].getInterval();
        break;
    case SVF::BinaryOPStmt::Mul:
    case SVF::BinaryOPStmt::FMul:
        as[resID] = as[left].getInterval() * as[right].getInterval();
        break;
    case SVF::BinaryOPStmt::SDiv:
    case SVF::BinaryOPStmt::FDiv:
    case SVF::BinaryOPStmt::UDiv:
        as[resID] = as[left].getInterval() / as[right].getInterval();
        break;
    case SVF::BinaryOPStmt::SRem:
    case SVF::BinaryOPStmt::FRem:
    case SVF::BinaryOPStmt::URem:
        as[resID] = as[left].getInterval() % as[right].getInterval();
        break;
    case SVF::BinaryOPStmt::Xor:
        as[resID] = as[left].getInterval() ^ as[right].getInterval();
        break;
    case SVF::BinaryOPStmt::Or:
        as[resID] = as[left].getInterval() | as[right].getInterval();
        break;
    case SVF::BinaryOPStmt::AShr:
    case SVF::BinaryOPStmt::LShr:
        as[resID] = as[left].getInterval() >> as[right].getInterval();
        break;
    case SVF::BinaryOPStmt::Shl:
        as[resID] = as[left].getInterval() << as[right].getInterval();
        break;
    }
    */
    auto resID = binary->getResID();
    auto left = binary->getOpVarID(0);
    auto right = binary->getOpVarID(1);

    switch (binary->getOpcode()) {
    case SVF::BinaryOPStmt::Add:
    case SVF::BinaryOPStmt::FAdd: {
        // Adding is always only done on variables
        // TODO: assuming that RHS is always a constant, change if this
        // is not the case
        ValueSet vs = this->blockState.getSVFVarSet(left);
        auto rightVar = binary->getOpVar(1);

        int rhsConst =
            this->getSVFVarSet(right, this->blockState).getConstant();
        vs.adjust(rhsConst);

        this->blockState.varState[resID] = vs;
        break;
    }
    case SVF::BinaryOPStmt::Sub:
    case SVF::BinaryOPStmt::FSub: {
        ValueSet vs = this->blockState.getSVFVarSet(left);
        auto rightVar = binary->getOpVar(1);

        int rhsConst =
            this->getSVFVarSet(right, this->blockState).getConstant();
        vs.adjust(-rhsConst);

        this->blockState.varState[resID] = vs;
        break;
    }
    }
}

/// Abstract state updates on an SVF::CmpStmt
void VSA::updateStateOnCmp(const SVF::CmpStmt *cmp) {
    const SVF::ICFGNode *node = cmp->getICFGNode();

    u32_t op0 = cmp->getOpVarID(0);
    u32_t op1 = cmp->getOpVarID(1);
    u32_t res = cmp->getResID();

    RIC resVal;
    RIC lhs = this->getSVFVarSet(op0, this->blockState).getGlobal(),
        rhs = this->getSVFVarSet(op1, this->blockState).getGlobal();

    // AbstractValue
    auto predicate = cmp->getPredicate();

    switch (predicate) {
    case SVF::CmpStmt::ICMP_EQ:
    case SVF::CmpStmt::FCMP_OEQ:
    case SVF::CmpStmt::FCMP_UEQ:
        resVal = lhs.eq(rhs);
        break;
    /*
    case SVF::CmpStmt::ICMP_NE:
    case SVF::CmpStmt::FCMP_ONE:
    case SVF::CmpStmt::FCMP_UNE:
        resVal = (lhs != rhs);
        break;
    case SVF::CmpStmt::ICMP_UGT:
    case SVF::CmpStmt::ICMP_SGT:
    case SVF::CmpStmt::FCMP_OGT:
    case SVF::CmpStmt::FCMP_UGT:
        resVal = (lhs > rhs);
        break;
    case SVF::CmpStmt::ICMP_UGE:
    case SVF::CmpStmt::ICMP_SGE:
    case SVF::CmpStmt::FCMP_OGE:
    case SVF::CmpStmt::FCMP_UGE:
        resVal = (lhs >= rhs);
        break;
    case SVF::CmpStmt::ICMP_ULT:
    case SVF::CmpStmt::ICMP_SLT:
    case SVF::CmpStmt::FCMP_OLT:
    case SVF::CmpStmt::FCMP_ULT:
        resVal = (lhs < rhs);
        break;
    */
    case SVF::CmpStmt::ICMP_ULE:
    case SVF::CmpStmt::ICMP_SLE:
    case SVF::CmpStmt::FCMP_OLE:
    case SVF::CmpStmt::FCMP_ULE:
        resVal = lhs.le(rhs);
        break;
        /*
        case SVF::CmpStmt::FCMP_FALSE:
            resVal = IntervalValue(0, 0);
            break;
        case SVF::CmpStmt::FCMP_TRUE:
            resVal = IntervalValue(1, 1);
            break;
        default: {
            assert(false && "undefined compare: ");
        }
        */
    }

    this->blockState.varState[res].values[0] = resVal;

    /*
    if (as.inVarToValTable(op0) && as.inVarToValTable(op1)) {
        as[res] = resVal;
    }
    */
    /*
    else if (as.inVarToAddrsTable(op0) && as.inVarToAddrsTable(op1)) {
        IntervalValue resVal;
        AbstractValue &lhs = as[op0], &rhs = as[op1];
        auto predicate = cmp->getPredicate();
        switch (predicate) {
        case SVF::CmpStmt::ICMP_EQ:
        case SVF::CmpStmt::FCMP_OEQ:
        case SVF::CmpStmt::FCMP_UEQ: {
            if (lhs.getAddrs().size() == 1 && rhs.getAddrs().size() == 1) {
                resVal = IntervalValue(lhs.equals(rhs));
            } else {
                if (lhs.getAddrs().hasIntersect(rhs.getAddrs())) {
                    resVal = IntervalValue::top();
                } else {
                    resVal = IntervalValue(0);
                }
            }
            break;
        }
        case SVF::CmpStmt::ICMP_NE:
        case SVF::CmpStmt::FCMP_ONE:
        case SVF::CmpStmt::FCMP_UNE: {
            if (lhs.getAddrs().size() == 1 && rhs.getAddrs().size() == 1) {
                resVal = IntervalValue(!lhs.equals(rhs));
            } else {
                if (lhs.getAddrs().hasIntersect(rhs.getAddrs())) {
                    resVal = IntervalValue::top();
                } else {
                    resVal = IntervalValue(1);
                }
            }
            break;
        }
        case SVF::CmpStmt::ICMP_UGT:
        case SVF::CmpStmt::ICMP_SGT:
        case SVF::CmpStmt::FCMP_OGT:
        case SVF::CmpStmt::FCMP_UGT: {
            if (lhs.getAddrs().size() == 1 && rhs.getAddrs().size() == 1) {
                resVal = IntervalValue(*lhs.getAddrs().begin() >
                                       *rhs.getAddrs().begin());
            } else {
                resVal = IntervalValue::top();
            }
            break;
        }
        case SVF::CmpStmt::ICMP_UGE:
        case SVF::CmpStmt::ICMP_SGE:
        case SVF::CmpStmt::FCMP_OGE:
        case SVF::CmpStmt::FCMP_UGE: {
            if (lhs.getAddrs().size() == 1 && rhs.getAddrs().size() == 1) {
                resVal = IntervalValue(*lhs.getAddrs().begin() >=
                                       *rhs.getAddrs().begin());
            } else {
                resVal = IntervalValue::top();
            }
            break;
        }
        case SVF::CmpStmt::ICMP_ULT:
        case SVF::CmpStmt::ICMP_SLT:
        case SVF::CmpStmt::FCMP_OLT:
        case SVF::CmpStmt::FCMP_ULT: {
            if (lhs.getAddrs().size() == 1 && rhs.getAddrs().size() == 1) {
                resVal = IntervalValue(*lhs.getAddrs().begin() <
                                       *rhs.getAddrs().begin());
            } else {
                resVal = IntervalValue::top();
            }
            break;
        }
        case SVF::CmpStmt::ICMP_ULE:
        case SVF::CmpStmt::ICMP_SLE:
        case SVF::CmpStmt::FCMP_OLE:
        case SVF::CmpStmt::FCMP_ULE: {
            if (lhs.getAddrs().size() == 1 && rhs.getAddrs().size() == 1) {
                resVal = IntervalValue(*lhs.getAddrs().begin() <=
                                       *rhs.getAddrs().begin());
            } else {
                resVal = IntervalValue::top();
            }
            break;
        }
        case SVF::CmpStmt::FCMP_FALSE:
            resVal = IntervalValue(0, 0);
            break;
        case SVF::CmpStmt::FCMP_TRUE:
            resVal = IntervalValue(1, 1);
            break;
        default: {
            assert(false && "undefined compare: ");
        }
        }
        as[res] = resVal;
    }
    */
}

/// @brief In Remill-lifted binaries, the only store statements are ones
/// which modify a register - the actual store operation that requires heavy
/// manipulation of abstract stores is the intrinsic function(s)
/// `@__remill_write`.
/// @param load
void VSA::updateStateOnStore(const SVF::StoreStmt *store) {
    auto lhs = store->getLHSVar();
    auto rhs = store->getRHSVar();

    // PC and NEXT_PC treated as constants
    if (lhs->getName() == "PC") {
        ValueSet rhsSet = this->blockState.getSVFVarSet(rhs->getId());
        this->pc = rhsSet.getConstant();
    } else if (lhs->getName() == "NEXT_PC") {
        ValueSet rhsSet = this->blockState.getSVFVarSet(rhs->getId());
        this->nextPc = rhsSet.getConstant();
    } else if (lhs->getName() == "RETURN_PC") {
        ValueSet rhsSet = this->blockState.getSVFVarSet(rhs->getId());
        this->returnPc = rhsSet.getConstant();
    }
    // Store value to register
    else if (this->blockState.isRegister(lhs->getName())) {
        ValueSet rhsSet = this->blockState.getSVFVarSet(rhs->getId());
        this->blockState.abstractStore.registers[lhs->getName()] = rhsSet;

        for (auto kv : this->blockState.getRegisterSet(lhs->getName()).values) {
            std::cout << "Register " << lhs->getName() << " has RIC "
                      << kv.second.toString() << " at memory region "
                      << kv.first << std::endl;
        }
    }
}

/// @brief In Remill-lifted binaries, the only load statements are ones
/// which modify a register - the actual load operation that requires heavy
/// manipulation of abstract stores is the intrinsic function(s)
/// `@__remill_read`.
/// @param load
void VSA::updateStateOnLoad(const SVF::LoadStmt *load) {
    auto lhs = load->getLHSVar();
    auto rhs = load->getRHSVar();

    // PC and NEXT_PC treated as constants
    if (rhs->getName() == "PC") {
        this->blockState.varState.insert({lhs->getId(), ValueSet(this->pc)});
    } else if (rhs->getName() == "NEXT_PC") {
        this->blockState.varState.insert(
            {lhs->getId(), ValueSet(this->nextPc)});
    } else if (rhs->getName() == "RETURN_PC") {
        this->blockState.varState.insert(
            {lhs->getId(), ValueSet(this->returnPc)});
    }
    // RBP treated as offset of memory region corresponding to procedure
    else if (rhs->getName() == "RBP") {
        ValueSet vs;
        vs.values[1] = RIC(this->blockState.stackSize);
        this->blockState.varState.insert({lhs->getId(), vs});
    }
    // Load value from register
    else if (this->blockState.isRegister(rhs->getName())) {
        this->blockState.varState.insert(
            {lhs->getId(), this->blockState.getRegisterSet(rhs->getName())});
    }
}

void VSA::updateStateOnExtCall(const SVF::CallICFGNode *extCallNode) {}

void VSA::updateStateOnSelect(const SVF::SelectStmt *select) {
    const SVF::ICFGNode *node = select->getICFGNode();

    u32_t res = select->getResID();
    u32_t tval = select->getTrueValue()->getId();
    u32_t fval = select->getFalseValue()->getId();
    u32_t cond = select->getCondition()->getId();

    ValueSet condVs = this->getSVFVarSet(cond, this->blockState);

    if (condVs.getGlobal().isConstant()) {
        int condConst = condVs.getConstant();
        this->blockState.varState[res] =
            (condConst == 0) ? this->getSVFVarSet(fval, this->blockState)
                             : this->getSVFVarSet(tval, this->blockState);
    } else {
        ValueSet vs;
        vs.joinWith(this->getSVFVarSet(fval, this->blockState));
        vs.joinWith(this->getSVFVarSet(tval, this->blockState));
        this->blockState.varState[res] = vs;
    }
}

void VSA::updateStateOnBranch(const SVF::BranchStmt *branch) {
    // Branch is the end of a basic block, so we store our accumulated info
    // into `postBasicBlock`
    this->postBasicBlock[branch->getICFGNode()] = this->blockState;
    this->postBasicBlock[branch->getICFGNode()].nextPc = this->nextPc;

    std::cout << "A-loc at position [rbp-0x54] now has value "
              << this->blockState.abstractStore.alocs[ALoc{1, 12, 4}].getGlobal().toString()
              << " at branch" << std::endl;

    // Clear all local variables
    this->blockState.varState.clear();
}
