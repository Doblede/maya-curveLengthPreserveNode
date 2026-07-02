#include "CurveLengthPreserve.h"
#include <maya/MFnPlugin.h>

MStatus initializePlugin(MObject obj) {
    MFnPlugin plugin(obj, "David De Juan", "1.0", "Any");
    MStatus stat;

    stat = plugin.registerNode(
        "CurveLengthPreserveNode", 
        CurveLengthPreserveNode::id, 
        CurveLengthPreserveNode::creator, 
        CurveLengthPreserveNode::initialize, 
        MPxNode::kDependNode
    );
    if (!stat) {
        stat.perror("Failed to register node: CurveLengthPreserveNode");
        return stat;
    }

    stat = plugin.registerCommand(
        "createLengthPreserve", 
        CreateLengthPreserveCmd::creator
    );
    if (!stat) {
        stat.perror("Failed to register command: createLengthPreserve");
        return stat;
    }

    return MS::kSuccess;
}

MStatus uninitializePlugin(MObject obj) {
    MFnPlugin plugin(obj);
    MStatus stat;

    stat = plugin.deregisterNode(CurveLengthPreserveNode::id);
    if (!stat) stat.perror("Failed to deregister node: CurveLengthPreserveNode");

    stat = plugin.deregisterCommand("createLengthPreserve");
    if (!stat) stat.perror("Failed to deregister command: createLengthPreserve");

    return stat;
}