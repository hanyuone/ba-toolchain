#include <iostream>
#include <vector>

#include "Graphs/SVFG.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "Util/Options.h"
#include "WPA/Andersen.h"

#include <vsa/VSA.hpp>

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

    /// VSA analysis
    VSA vsa(icfg);
    vsa.analyse();

    /// Value-Flow Graph (VFG)
    SVF::VFG *vfg = new SVF::VFG(callgraph);

    /// Sparse value-flow graph (SVFG)
    SVF::SVFGBuilder svfBuilder;
    SVF::SVFG *svfg = svfBuilder.buildFullSVFG(ander);

    // clean up memory
    delete vfg;
    SVF::AndersenWaveDiff::releaseAndersenWaveDiff();
    SVF::SVFIR::releaseSVFIR();

    SVF::LLVMModuleSet::getLLVMModuleSet()->dumpModulesToFile(".svf.bc");
    SVF::LLVMModuleSet::releaseLLVMModuleSet();

    llvm::llvm_shutdown();
    return 0;
}
