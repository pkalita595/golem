/*
 * Copyright (c) 2022, Martin Blicha <martin.blicha@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef GOLEM_BASICTRANSFORMATIONPIPELINES_H
#define GOLEM_BASICTRANSFORMATIONPIPELINES_H

#include "FalseClauseRemoval.h"
#include "MultiEdgeMerger.h"
#include "NonLoopEliminator.h"
#include "TransformationPipeline.h"

namespace Transformations {

inline TransformationPipeline towardsTransitionSystems() {
    TransformationPipeline::pipeline_t stages;
    stages.push_back(std::make_unique<MultiEdgeMerger>());
    stages.push_back(std::make_unique<NonLoopEliminator>());
    stages.push_back(std::make_unique<FalseClauseRemoval>());
    stages.push_back(std::make_unique<MultiEdgeMerger>());
    TransformationPipeline pipeline(std::move(stages));
    return pipeline;
}

}

#endif //GOLEM_BASICTRANSFORMATIONPIPELINES_H
