#include <iostream>
#include <vector>

#include "Graphs/SVFG.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "Util/Options.h"
#include "WPA/Andersen.h"

#include <static/asi/ASI.hpp>
#include <static/vsa/VSA.hpp>

std::map<ALoc, ASIType *> reconstructTypes(SVF::ICFG *icfg) {
    // Return types...
    /// VSA analysis
    std::vector<ALoc> alocs = {ALoc{1, 12, 4}, ALoc{1, 16, 16}, ALoc{1, 32, 8},
                               ALoc{1, 40, 8}};

    VSA vsa(icfg);
    vsa.setALocs(alocs);
    vsa.analyse();

    auto accesses = vsa.getDataAccesses();

    for (auto kv : accesses) {
        std::cout << "Data access at node " << kv.first << ": "
                  << kv.second.first.toString() << ", accessing "
                  << kv.second.second << " bytes " << std::endl;
    }

    // ASI analysis
    ASI asi(accesses, alocs);
    asi.analyse();

    return asi.getTypes();
}

int main(int argc, char *argv[]) {
    std::vector<std::string> moduleNameVec;
    moduleNameVec =
        OptionBase::parseOptions(argc, argv, "Whole Program Points-to Analysis",
                                 "[options] <input-bitcode...>");

    if (SVF::Options::WriteAnder() == "ir_annotator") {
        SVF::LLVMModuleSet::preProcessBCs(moduleNameVec);
    }

    SVF::LLVMModuleSet::buildSVFModule(moduleNameVec);

    /// Build Program Assignment Graph (SVFIR)
    SVF::SVFIRBuilder builder;
    SVF::SVFIR *pag = builder.build();

    /// Create Andersen's pointer analysis
    SVF::Andersen *ander = SVF::AndersenWaveDiff::createAndersenWaveDiff(pag);
    ander->disablePrintStat();

    /// Call Graph
    SVF::CallGraph *callgraph = ander->getCallGraph();

    /// ICFG
    SVF::ICFG *icfg = pag->getICFG();
    icfg->dump(pag->getModuleIdentifier() + ".icfg");

    /// Static analysis
    auto types = reconstructTypes(icfg);

    for (auto kv : types) {
        ALoc aloc = kv.first;
        std::cout << aloc.toString() << " has type " << kv.second->toString()
                  << std::endl;
    }

    // clean up memory
    SVF::AndersenWaveDiff::releaseAndersenWaveDiff();
    SVF::SVFIR::releaseSVFIR();

    SVF::LLVMModuleSet::getLLVMModuleSet()->dumpModulesToFile(".svf.bc");
    SVF::LLVMModuleSet::releaseLLVMModuleSet();

    llvm::llvm_shutdown();
    return 0;
}
