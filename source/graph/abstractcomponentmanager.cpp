#include "abstractcomponentmanager.h"
#include "../graph/grapharray.h"

#include <QtGlobal>

AbstractComponentManager::AbstractComponentManager(Graph& graph) :
    _debug(false)
{
    if(qgetenv("COMPONENTS_DEBUG").toInt())
        _debug = true;

    connect(&graph, &Graph::graphChanged, this, &AbstractComponentManager::onGraphChanged, Qt::DirectConnection);

    connect(this, &AbstractComponentManager::componentAdded, &graph, &Graph::componentAdded, Qt::DirectConnection);
    connect(this, &AbstractComponentManager::componentWillBeRemoved, &graph, &Graph::componentWillBeRemoved, Qt::DirectConnection);
    connect(this, &AbstractComponentManager::componentSplit, &graph, &Graph::componentSplit, Qt::DirectConnection);
    connect(this, &AbstractComponentManager::componentsWillMerge, &graph, &Graph::componentsWillMerge, Qt::DirectConnection);
}

AbstractComponentManager::~AbstractComponentManager()
{
    // Let the ComponentArrays know that we're going away
    for(auto componentArray : _componentArrayList)
        componentArray->invalidate();
}

std::vector<NodeId> ComponentSplitSet::nodeIds() const
{
    std::vector<NodeId> nodeIds;

    for(auto componentId : _splitters)
    {
        auto component = _graph->componentById(componentId);
        nodeIds.insert(nodeIds.end(), component->nodeIds().begin(), component->nodeIds().end());
    }

    return nodeIds;
}

std::vector<NodeId> ComponentMergeSet::nodeIds() const
{
    std::vector<NodeId> nodeIds;

    for(auto componentId : _mergers)
    {
        auto component = _graph->componentById(componentId);
        nodeIds.insert(nodeIds.end(), component->nodeIds().begin(), component->nodeIds().end());
    }

    return nodeIds;
}
