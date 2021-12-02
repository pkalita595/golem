//
// Created by Martin Blicha on 10.08.20.
//

#ifndef OPENSMT_TRANSFORMATIONUTILS_H
#define OPENSMT_TRANSFORMATIONUTILS_H

#include "memory"
#include "ChcGraph.h"
#include "TransitionSystem.h"

bool isTransitionSystem(ChcDirectedGraph const & graph);

bool isTransitionSystemChain(ChcDirectedGraph const & graph);

std::unique_ptr<TransitionSystem> toTransitionSystem(ChcDirectedGraph const & graph, Logic& logic);

struct EdgeVariables {
    vec<PTRef> stateVars;
    vec<PTRef> nextStateVars;
    vec<PTRef> auxiliaryVars;
};

EdgeVariables getVariablesFromEdge(Logic & logic, ChcDirectedGraph const & graph, EId eid);

#endif //OPENSMT_TRANSFORMATIONUTILS_H
