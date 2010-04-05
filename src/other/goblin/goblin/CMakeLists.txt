set(GOBLIN_SOURCES
abstractBalanced.cpp
abstractBigraph.cpp
abstractDigraph.cpp
abstractGraph.cpp
abstractMixedGraph.cpp
abstractSubgraph.cpp
attributePool.cpp
auxiliaryNetwork.cpp
balancedDigraph.cpp
balancedToBalanced.cpp
basicHeap.cpp
bigraphToDigraph.cpp
binaryHeap.cpp
branchMIP.cpp
branchScheme.cpp
canvasBuilder.cpp
complement.cpp
denseBigraph.cpp
denseDigraph.cpp
denseGraph.cpp
denseRepresentation.cpp
dictionary.cpp
digraphToDigraph.cpp
disjointFamily.cpp
doxygen.cpp
dynamicQueue.cpp
dynamicStack.cpp
exportToDot.cpp
exportToTk.cpp
exportToXFig.cpp
fibonacciHeap.cpp
fileExport.cpp
fileImport.cpp
globals.cpp
goblinController.cpp
graphDisplayProxy.cpp
graphRepresentation.cpp
graphToBalanced.cpp
hashTable.cpp
ilpCompose.cpp
ilpWrapper.cpp
incrementalGeometry.cpp
indexSet.cpp
itAuxiliaryNetwork.cpp
itGraph.cpp
itSurfaceGraph.cpp
layoutForceDirected.cpp
layoutKandinsky.cpp
layoutLayered.cpp
layoutOrthogonal.cpp
libConvert.cpp
libImport.cpp
libLayout.cpp
libPerfect.cpp
libPlanarity.cpp
listOfAuthors.cpp
listOfModules.cpp
listOfReferences.cpp
listOfTimers.cpp
lpBasis.cpp
lpRepresentation.cpp
lpSolver.cpp
managedObject.cpp
matrix.cpp
mixedGraph.cpp
moduleGuard.cpp
movingLineModel.cpp
nestedFamily.cpp
networkSimplex.cpp
orthogonalGrid.cpp
poolTables.cpp
segment.cpp
seriesParallel.cpp
shrinkingNetwork.cpp
solveAsyTSP.cpp
solveColour.cpp
solveComponents.cpp
solveMCBalFlow.cpp
solveMCFlow.cpp
solveMatching.cpp
solveMaxBalFlow.cpp
solveMaxCut.cpp
solveMaxFlow.cpp
solveMinCut.cpp
solveMinTree.cpp
solvePlanarity.cpp
solvePostman.cpp
solveSPTree.cpp
solveStable.cpp
solveSteiner.cpp
solveSymmTSP.cpp
solveTJoin.cpp
solveTreePkg.cpp
sparseBigraph.cpp
sparseDigraph.cpp
sparseGraph.cpp
sparseRepresentation.cpp
staticQueue.cpp
staticStack.cpp
stripeDissectionModel.cpp
subgraph.cpp
surfaceGraph.cpp
timers.cpp
treeView.cpp
)

file(GLOB GOBLIN_HEADERS *.h)

include_directories(
    ../../../include
)

add_definitions(
   -DON_DLL_EXPORTS
)

if(MSVC)
    add_definitions(
        -DUNICODE
    )

endif(MSVC)

add_library(goblin SHARED ${GOBLIN_SOURCES})

target_link_libraries(goblin ${GOBLIN_LINKLIBRARIES})

set_property(TARGET goblin PROPERTY LINK_INTERFACE_LIBRARIES "")

install(TARGETS goblin 
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)

install(FILES ${GOBLIN_HEADERS} DESTINATION include)
