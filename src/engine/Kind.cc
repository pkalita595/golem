/*
 * Copyright (c) 2022, Martin Blicha <martin.blicha@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "Kind.h"

#include "QuantifierElimination.h"
#include "TermUtils.h"
#include "transformers/BasicTransformationPipelines.h"
#include "TransformationUtils.h"

VerificationResult Kind::solve(ChcDirectedHyperGraph & graph) {
    auto pipeline = Transformations::towardsTransitionSystems();
    auto transformationResult = pipeline.transform(std::make_unique<ChcDirectedHyperGraph>(graph));
    auto transformedGraph = std::move(transformationResult.first);
    auto translator = std::move(transformationResult.second);
    if (transformedGraph->isNormalGraph()) {
        auto normalGraph = transformedGraph->toNormalGraph();
        auto res = solve(*normalGraph);
        return computeWitness ? translator->translate(std::move(res)) : res;
    }
    return VerificationResult(VerificationAnswer::UNKNOWN);
}

VerificationResult Kind::solve(ChcDirectedGraph const & system) {
    if (isTransitionSystem(system)) {
        auto ts = toTransitionSystem(system, logic);
        return solveTransitionSystem(*ts, system);
    }
    return VerificationResult(VerificationAnswer::UNKNOWN);
}

VerificationResult Kind::solveTransitionSystem(TransitionSystem const & system, ChcDirectedGraph const & graph) {
    std::size_t maxK = std::numeric_limits<std::size_t>::max();
    PTRef init = system.getInit();
    PTRef query = system.getQuery();
    PTRef transition = system.getTransition();
    PTRef backwardTransition = TransitionSystem::reverseTransitionRelation(system);

    // Base step: Init(x0) and Tr^k(x0,xk) and Query(xk), if SAT -> return UNSAFE
    // Inductive step forward:
    // ~Query(x0) and Tr(x0,x1) and ~Query(x1) and Tr(x1,x2) ... and ~Query(x_{k-1}) and Tr(x_{k-1},x_k) => ~Query(x_k), is valid ->  return SAFE
    // Inductive step backward:
    // ~Init(x0) <= Tr(x0,x1) and ~Init(x1) and ... and Tr(x_{k-1},x_k) and ~Init(xk), is valid -> return SAFE

    SMTConfig configBase;
    SMTConfig configStepForward;
    SMTConfig configStepBackward;
    MainSolver solverBase(logic, configBase, "KIND-base");
    MainSolver solverStepForward(logic, configStepForward, "KIND-stepForward");
    MainSolver solverStepBackward(logic, configStepBackward, "KIND-stepBackward");

    PTRef negQuery = logic.mkNot(query);
    PTRef negInit = logic.mkNot(init);
    // starting point
    solverBase.insertFormula(init);
    solverStepBackward.insertFormula(init);
    solverStepForward.insertFormula(query);
    { // Check for system with empty initial states
        auto res = solverBase.check();
        if (res == s_False) {
            return VerificationResult{VerificationAnswer::SAFE};
        }
    }

    TimeMachine tm{logic};
    for (std::size_t k = 0; k < maxK; ++k) {
        PTRef versionedQuery = tm.sendFlaThroughTime(query, k);
        // Base case
        solverBase.push();
        solverBase.insertFormula(versionedQuery);
        auto res = solverBase.check();
        if (res == s_True) {
            if (verbosity > 0) {
                 std::cout << "; KIND: Bug found in depth: " << k << std::endl;
            }
            if (computeWitness) {
                return VerificationResult(VerificationAnswer::UNSAFE, InvalidityWitness::fromTransitionSystem(graph, k));
            } else {
                return VerificationResult(VerificationAnswer::UNSAFE);
            }
        }
        if (verbosity > 1) {
            std::cout << "; KIND: No path of length " << k << " found!" << std::endl;
        }
        solverBase.pop();
        PTRef versionedTransition = tm.sendFlaThroughTime(transition, k);
//        std::cout << "Adding transition: " << logic.pp(versionedTransition) << std::endl;
        solverBase.insertFormula(versionedTransition);

        // step forward
        res = solverStepForward.check();
        if (res == s_False) {
            if (verbosity > 0) {
                std::cout << "; KIND: Found invariant with forward induction, which is " << k << "-inductive" << std::endl;
            }
            if (computeWitness) {
                return VerificationResult(VerificationAnswer::SAFE, witnessFromForwardInduction(graph, system, k));
            } else {
                return VerificationResult(VerificationAnswer::SAFE);
            }
        }
        PTRef versionedBackwardTransition = tm.sendFlaThroughTime(backwardTransition, k);
        solverStepForward.push();
        solverStepForward.insertFormula(versionedBackwardTransition);
        solverStepForward.insertFormula(tm.sendFlaThroughTime(negQuery,k+1));

        // step backward
        res = solverStepBackward.check();
        if (res == s_False) {
            if (verbosity > 0) {
                std::cout << "; KIND: Found invariant with backward induction, which is " << k << "-inductive" << std::endl;
            }
            if (computeWitness) {
                return VerificationResult(VerificationAnswer::SAFE, witnessFromBackwardInduction(graph, system, k));
            } else {
                return VerificationResult(VerificationAnswer::SAFE);
            }
        }
        solverStepBackward.push();
        solverStepBackward.insertFormula(versionedTransition);
        solverStepBackward.insertFormula(tm.sendFlaThroughTime(negInit, k+1));
    }
    return VerificationResult(VerificationAnswer::UNKNOWN);
}

ValidityWitness Kind::witnessFromForwardInduction(ChcDirectedGraph const & graph,
                                                  TransitionSystem const & transitionSystem, unsigned long k) const {
    PTRef kinductiveInvariant = logic.mkNot(transitionSystem.getQuery());
    PTRef inductiveInvariant = kinductiveToInductive(kinductiveInvariant, k, transitionSystem);
    return ValidityWitness::fromTransitionSystem(logic, graph, transitionSystem, inductiveInvariant);
}

ValidityWitness Kind::witnessFromBackwardInduction(ChcDirectedGraph const & graph,
                                                   TransitionSystem const & transitionSystem, unsigned long k) const {
    auto reversedSystem = TransitionSystem::reverse(transitionSystem);
    PTRef kinductiveInvariant = logic.mkNot(reversedSystem.getQuery());
    PTRef inductiveInvariant = kinductiveToInductive(kinductiveInvariant, k, reversedSystem);
    PTRef originalInvariant = logic.mkNot(inductiveInvariant);
    return ValidityWitness::fromTransitionSystem(logic, graph, transitionSystem, originalInvariant);
}