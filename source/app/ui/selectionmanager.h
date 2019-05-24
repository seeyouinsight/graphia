#ifndef SELECTIONMANAGER_H
#define SELECTIONMANAGER_H

#include "shared/ui/iselectionmanager.h"
#include "shared/utils/container.h"

#include <QObject>

#include <memory>

class GraphModel;

class SelectionManager : public QObject, public ISelectionManager
{
    Q_OBJECT
public:
    explicit SelectionManager(const GraphModel& graphModel);

    NodeIdSet selectedNodes() const override;
    NodeIdSet unselectedNodes() const override;

    bool selectNode(NodeId nodeId) override;
    bool selectNodes(const NodeIdSet& nodeIds) override;
    bool selectNodes(const std::vector<NodeId>& nodeIds);

    bool deselectNode(NodeId nodeId) override;
    bool deselectNodes(const NodeIdSet& nodeIds) override;
    bool deselectNodes(const std::vector<NodeId>& nodeIds);

    bool toggleNode(NodeId nodeId);

    bool nodeIsSelected(NodeId nodeId) const override;

    bool selectAllNodes() override;
    bool clearNodeSelection() override;
    void invertNodeSelection() override;

    void setNodesMask(const NodeIdSet& nodeIds, bool applyMask = true);
    void setNodesMask(const std::vector<NodeId>& nodeIds, bool applyMask = true);
    void clearNodesMask() { _nodeIdsMask.clear(); emit nodesMaskChanged(); }
    bool nodesMaskActive() const { return !_nodeIdsMask.empty(); }

    int numNodesSelected() const { return static_cast<int>(_selectedNodeIds.size()); }
    QString numNodesSelectedAsString() const;

    void suppressSignals();

private:
    const GraphModel* _graphModel = nullptr;

    NodeIdSet _selectedNodeIds;

    // Temporary storage for NodeIds that have been deleted
    std::vector<NodeId> _deletedNodes;

    NodeIdSet _nodeIdsMask;

    bool _suppressSignals = false;

    bool signalsSuppressed();

    template<typename Fn>
    bool callFnAndMaybeEmit(Fn&& fn)
    {
        bool selectionWillChange = fn();

        if(!signalsSuppressed() && selectionWillChange)
            emit selectionChanged(this);

        return selectionWillChange;
    }

signals:
    void selectionChanged(const SelectionManager* selectionManager) const;
    void nodesMaskChanged() const;
};

#endif // SELECTIONMANAGER_H
