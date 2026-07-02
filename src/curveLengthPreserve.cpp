#include "CurveLengthPreserve.h"

#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnNurbsCurve.h>
#include <maya/MFnNurbsCurveData.h>
#include <maya/MArrayDataBuilder.h>
#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>
#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MDGModifier.h>
#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <maya/MVector.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <algorithm>
#include <vector>

// ========================================================
// NODE IMPLEMENTATION
// ========================================================
MTypeId CurveLengthPreserveNode::id(0x8880B);
MObject CurveLengthPreserveNode::aInputCurves;
MObject CurveLengthPreserveNode::aGoalCurves;
MObject CurveLengthPreserveNode::aOutputCurves;
MObject CurveLengthPreserveNode::aParallelThreshold;

void* CurveLengthPreserveNode::creator() { 
    return new CurveLengthPreserveNode(); 
}

MStatus CurveLengthPreserveNode::initialize() {
    MFnTypedAttribute typedAttr;
    MFnNumericAttribute numAttr;
    MStatus stat;

    // --- PARALLEL THRESHOLD ---
    aParallelThreshold = numAttr.create("parallelThreshold", "parallelThreshold", MFnNumericData::kInt, 50, &stat);
    numAttr.setMin(0);          
    numAttr.setKeyable(true);   
    numAttr.setStorable(true);  
    addAttribute(aParallelThreshold);

    // --- INPUT CURVES ---
    aInputCurves = typedAttr.create("inputCurves", "inputCurves", MFnData::kNurbsCurve, MObject::kNullObj, &stat);
    typedAttr.setHidden(true);
    typedAttr.setArray(true);
    addAttribute(aInputCurves);

    // --- GOAL CURVES (Rest Shapes) ---
    aGoalCurves = typedAttr.create("goalCurves", "goalCurves", MFnData::kNurbsCurve, MObject::kNullObj, &stat);
    typedAttr.setHidden(true);
    typedAttr.setArray(true);
    addAttribute(aGoalCurves);

    // --- OUTPUT CURVES ---
    aOutputCurves = typedAttr.create("outputCurves", "outputCurves", MFnData::kNurbsCurve, MObject::kNullObj, &stat);
    typedAttr.setArray(true);
    typedAttr.setStorable(false);
    addAttribute(aOutputCurves);

    // --- ATTRIBUTE AFFECTS ---
    attributeAffects(aInputCurves, aOutputCurves);
    attributeAffects(aGoalCurves, aOutputCurves);
    attributeAffects(aParallelThreshold, aOutputCurves); 

    return MS::kSuccess;
}

MStatus CurveLengthPreserveNode::compute(const MPlug& plug, MDataBlock& dataBlock) {
    if (plug != aOutputCurves) return MS::kUnknownParameter;

    MArrayDataHandle inCurvesData = dataBlock.inputArrayValue(aInputCurves);
    MArrayDataHandle goalCurvesData = dataBlock.inputArrayValue(aGoalCurves);
    MArrayDataHandle outputArray = dataBlock.outputArrayValue(aOutputCurves);

    unsigned int numberCurves = std::min(inCurvesData.elementCount(), goalCurvesData.elementCount());
    if (numberCurves == 0) return MS::kSuccess;

    // 1. GATHER (Sequential Data Extraction)
    std::vector<MObject> inCurves(numberCurves);
    std::vector<MObject> goalCurves(numberCurves);
    std::vector<MObject> outCurves(numberCurves);

    MFnNurbsCurveData dataCreator;
    MArrayDataBuilder builder(&dataBlock, aOutputCurves, numberCurves);

    for (unsigned int i = 0; i < numberCurves; ++i) {
        inCurvesData.jumpToElement(i);
        goalCurvesData.jumpToElement(i);

        inCurves[i] = inCurvesData.inputValue().asNurbsCurve();
        goalCurves[i] = goalCurvesData.inputValue().asNurbsCurve();
        
        outCurves[i] = dataCreator.create();
        MDataHandle outHandle = builder.addElement(i);
        outHandle.setMObject(outCurves[i]);
    }

    // 2. THE MATH LOGIC (Packaged as a Lambda)
    auto computeCurveTask = [&](unsigned int i) {
        if (inCurves[i].isNull() || goalCurves[i].isNull()) return;

        MFnNurbsCurve inputCurveFn(inCurves[i]);
        MFnNurbsCurve goalCurveFn(goalCurves[i]);
        MFnNurbsCurve outCurvesFn;
        
        outCurvesFn.copy(inCurves[i], outCurves[i]);

        MPointArray inputCVs, goalCVs;
        inputCurveFn.getCVs(inputCVs, MSpace::kObject);
        goalCurveFn.getCVs(goalCVs, MSpace::kObject);

        unsigned int numCVs = std::min(inputCVs.length(), goalCVs.length());

        if (numCVs > 1) {
            MPointArray outCVs(numCVs);
            outCVs[0] = inputCVs[0]; // Root never moves

            // Find absolute edge distances along GOAL control hull
            std::vector<double> goalHullDistances(numCVs, 0.0);
            double currentGoalDist = 0.0;
            for (unsigned int j = 1; j < numCVs; ++j) {
                currentGoalDist += goalCVs[j].distanceTo(goalCVs[j-1]);
                goalHullDistances[j] = currentGoalDist;
            }

            // Find edge distances along INPUT control hull
            std::vector<double> inputHullDistances(numCVs, 0.0);
            double currentInputDist = 0.0;
            for (unsigned int j = 1; j < numCVs; ++j) {
                currentInputDist += inputCVs[j].distanceTo(inputCVs[j-1]);
                inputHullDistances[j] = currentInputDist;
            }

            if (inputHullDistances[numCVs - 1] > 1e-5) {
                for (unsigned int cvIndex = 1; cvIndex < numCVs; ++cvIndex) {
                    double targetDist = goalHullDistances[cvIndex];

                    // Extrapolate if stretched beyond rest length
                    if (targetDist >= inputHullDistances[numCVs - 1]) {
                        double excess = targetDist - inputHullDistances[numCVs - 1];
                        MVector lastDir = inputCVs[numCVs - 1] - inputCVs[numCVs - 2];
                        lastDir.normalize();
                        outCVs[cvIndex] = inputCVs[numCVs - 1] + (lastDir * excess);
                        continue;
                    }

                    // Otherwise interpolate along the input hull segments
                    for (unsigned int j = 1; j < numCVs; ++j) {
                        if (targetDist <= inputHullDistances[j] + 1e-6) {
                            double segmentLength = inputHullDistances[j] - inputHullDistances[j-1];
                            double segmentT = (segmentLength > 0.0) ? (targetDist - inputHullDistances[j-1]) / segmentLength : 0.0;
                            MVector dir = inputCVs[j] - inputCVs[j-1];
                            outCVs[cvIndex] = inputCVs[j-1] + (dir * segmentT);
                            break;
                        }
                    }
                }
                outCurvesFn.setCVs(outCVs, MSpace::kObject);
            }
        }
    };

    // 3. EXECUTE: DYNAMIC THRESHOLD
    int thresholdVal = dataBlock.inputValue(aParallelThreshold).asInt();
    unsigned int activeThreshold = std::max(0, thresholdVal);

    if (numberCurves >= activeThreshold && activeThreshold > 0) {
        // Run in Parallel using Intel TBB
        tbb::parallel_for(tbb::blocked_range<unsigned int>(0, numberCurves),
            [&](const tbb::blocked_range<unsigned int>& r) {
                for (unsigned int i = r.begin(); i != r.end(); ++i) {
                    computeCurveTask(i);
                }
            });
    } else {
        // Run Sequentially
        for (unsigned int i = 0; i < numberCurves; ++i) {
            computeCurveTask(i);
        }
    }

    // 4. SCATTER (Apply Results)
    outputArray.set(builder);
    outputArray.setAllClean();
    return MS::kSuccess;
}

// ========================================================
// COMMAND IMPLEMENTATION
// ========================================================
void* CreateLengthPreserveCmd::creator() { 
    return new CreateLengthPreserveCmd(); 
}

MStatus CreateLengthPreserveCmd::doIt(const MArgList& args) {
    MSelectionList sel;
    MGlobal::getActiveSelectionList(sel);
    if (sel.length() == 0) {
        MGlobal::displayError("Please select at least one NURBS curve.");
        return MS::kFailure;
    }

    MDGModifier dgMod;
    MObject nodeObj = dgMod.createNode("CurveLengthPreserveNode");
    dgMod.doIt(); 

    MFnDependencyNode nodeFn(nodeObj);
    MPlug inputPlugArr = nodeFn.findPlug("inputCurves", false);
    MPlug goalPlugArr = nodeFn.findPlug("goalCurves", false);
    MPlug outputPlugArr = nodeFn.findPlug("outputCurves", false);

    int logicalIndex = 0;
    for (unsigned int i = 0; i < sel.length(); ++i) {
        MDagPath curvePath;
        sel.getDagPath(i, curvePath);
        curvePath.extendToShape();
        if (!curvePath.node().hasFn(MFn::kNurbsCurve)) continue;

        MFnDagNode originalCurveFn(curvePath);
        
        MObject restShapeTransform = originalCurveFn.duplicate();
        MFnDagNode restTransformFn(restShapeTransform);
        restTransformFn.setName(originalCurveFn.name() + "_restShape");
        MDagPath restPath;
        restTransformFn.getPath(restPath);
        restPath.extendToShape();
        MFnDagNode restShapeFn(restPath);

        MObject outShapeTransform = originalCurveFn.duplicate();
        MFnDagNode outTransformFn(outShapeTransform);
        outTransformFn.setName(originalCurveFn.name() + "_preservedShape");
        MDagPath outPath;
        outTransformFn.getPath(outPath);
        outPath.extendToShape();
        MFnDagNode outShapeFn(outPath);

        MPlug srcLocal = originalCurveFn.findPlug("local", false);
        MPlug restLocal = restShapeFn.findPlug("local", false);
        MPlug outCreate = outShapeFn.findPlug("create", false);

        MPlug inElement = inputPlugArr.elementByLogicalIndex(logicalIndex);
        MPlug goalElement = goalPlugArr.elementByLogicalIndex(logicalIndex);
        MPlug outElement = outputPlugArr.elementByLogicalIndex(logicalIndex);

        dgMod.connect(srcLocal, inElement);
        dgMod.connect(restLocal, goalElement);
        dgMod.connect(outElement, outCreate);
        logicalIndex++;
    }

    dgMod.doIt();
    MGlobal::displayInfo("CurveLengthPreserve setup complete.");
    return MS::kSuccess;
}