#pragma once

#include <maya/MPxNode.h>
#include <maya/MPxCommand.h>

// ========================================================
// NODE DECLARATION
// ========================================================
class CurveLengthPreserveNode : public MPxNode {
public:
    static void* creator();
    static MStatus initialize();
    virtual MStatus compute(const MPlug& plug, MDataBlock& dataBlock) override;

    static MTypeId id;
    static MObject aInputCurves;
    static MObject aGoalCurves;
    static MObject aOutputCurves;
    
    // Controls when multi-threading kicks in
    static MObject aParallelThreshold;
};

// ========================================================
// COMMAND DECLARATION
// ========================================================
class CreateLengthPreserveCmd : public MPxCommand {
public:
    static void* creator();
    virtual MStatus doIt(const MArgList& args) override;
};