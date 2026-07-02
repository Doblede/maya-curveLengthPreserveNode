# Maya Curve Length Preserve Deformer

An Autodesk Maya C++ deformer plugin that implements length-preserving constraints on NURBS curves, maintaining rest-shape control hull length during deformation.

---

## 📖 Overview
This plugin provides a specialized Maya dependency node designed to maintain the structural integrity of NURBS curves when subjected to deformation[cite: 1, 2, 3]. By calculating and enforcing distances between control vertices (CVs) based on a reference rest shape, the `curveLengthPreserveNode` prevents the unwanted stretching or shrinking artifacts often encountered in standard curve animation workflows.

## ✨ Core Features

* **Length-Preserving Constraints**: Enforces constant segment lengths based on the rest state, ensuring the curve preserves its volume and silhouette regardless of deformation[cite: 1].
* **High-Performance Multithreading**: Built with Intel TBB (`tbb::parallel_for`), enabling efficient processing of large curve sets[cite: 1].
* **Dynamic Multi-threading**: Includes a `parallelThreshold` attribute to dynamically toggle between sequential and parallel execution based on the number of curves in the scene, with a default threshold of 50.
* **Array-Based Architecture**: Efficiently handles multiple curves within a single DG node instance through array-based input/output attributes[cite: 1, 2].
* **Automated Rigging Workflow**: Comes with a dedicated MEL command (`createLengthPreserve`) that automatically handles scene setup, including rest-shape duplication and DG connection wiring[cite: 1, 3].


## 🚀 Quick Start

### Installation
1. Ensure the plugin is compiled for your specific version of Maya[cite: 1].
2. Load the plugin via the **Plug-in Manager** in Maya (`Windows > Settings/Preferences > Plug-in Manager`)[cite: 3].

### Usage
1. Select the NURBS curve(s) you wish to preserve[cite: 1].
2. Run the following command in the Command Line or Script Editor:
   ```mel
   createLengthPreserve;