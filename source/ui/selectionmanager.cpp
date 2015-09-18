#include "selectionmanager.h"

#include <algorithm>
#include <utility>

//#define EXPENSIVE_DEBUG_CHECKS

SelectionManager::SelectionManager(const GraphModel& graphModel) :
    QObject(),
    _graphModel(graphModel)
{}

NodeIdSet SelectionManager::selectedNodes() const
{
#ifdef EXPENSIVE_DEBUG_CHECKS
    // Assertion that our selection doesn't contain things that aren't in the graph
    Q_ASSERT(std::all_of(_selectedNodes.begin(), _selectedNodes.end(),
        [this](ElementId<NodeId> nodeId)
        {
            auto& nodeIds = _graphModel.graph().nodeIds();
            return u::contains(nodeIds, nodeId);
        }));
#endif

    return _selectedNodeIds;
}

NodeIdSet SelectionManager::unselectedNodes() const
{
    auto& nodeIds = _graphModel.graph().nodeIds();
    auto unselectedNodeIds = NodeIdSet(nodeIds.begin(), nodeIds.end());
    unselectedNodeIds.erase(_selectedNodeIds.begin(), _selectedNodeIds.end());

    return unselectedNodeIds;
}

bool SelectionManager::selectNode(NodeId nodeId)
{
    auto result = _selectedNodeIds.insert(nodeId);

    if(result.second)
        emit selectionChanged(this);

    return result.second;
}

bool SelectionManager::deselectNode(NodeId nodeId)
{
    bool selectionWillChange = _selectedNodeIds.erase(nodeId) > 0;

    if(selectionWillChange)
        emit selectionChanged(this);

    return selectionWillChange;
}

void SelectionManager::toggleNode(NodeId nodeId)
{
    if(nodeIsSelected(nodeId))
        deselectNode(nodeId);
    else
        selectNode(nodeId);
}

bool SelectionManager::nodeIsSelected(NodeId nodeId) const
{
#ifdef EXPENSIVE_DEBUG_CHECKS
    Q_ASSERT(u::contains(_graphModel.graph().nodeIds(), nodeId));
#endif
    return u::contains(_selectedNodeIds, nodeId);
}

bool SelectionManager::selectAllNodes()
{
    auto& nodeIds = _graphModel.graph().nodeIds();
    return selectNodes(nodeIds.begin(), nodeIds.end());
}

bool SelectionManager::clearNodeSelection()
{
    bool selectionWillChange = !_selectedNodeIds.empty();
    _selectedNodeIds.clear();

    if(selectionWillChange)
        emit selectionChanged(this);

    return selectionWillChange;
}

void SelectionManager::invertNodeSelection()
{
    auto& nodeIds = _graphModel.graph().nodeIds();
    toggleNodes(nodeIds.begin(), nodeIds.end());
}

const QString SelectionManager::numNodesSelectedAsString() const
{
    int selectionSize = static_cast<int>(selectedNodes().size());

    if(selectionSize == 1)
    {
        auto nodeId = *selectedNodes().begin();
        auto& nodeName = _graphModel.nodeNames()[nodeId];

        if(!nodeName.isEmpty())
            return QString(tr("Selected %1")).arg(nodeName);

        return tr("1 Node Selected");
    }
    else if(selectionSize > 1)
        return QString(tr("%1 Nodes Selected")).arg(selectionSize);

    return QString();
}
