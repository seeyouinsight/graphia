#ifndef DELETENODESCOMMAND_H
#define DELETENODESCOMMAND_H

#include "shared/commands/icommand.h"

#include "graph/graph.h"

#include <vector>

class GraphModel;
class SelectionManager;

class DeleteNodesCommand : public ICommand
{
private:
    GraphModel* _graphModel = nullptr;
    SelectionManager* _selectionManager = nullptr;

    bool _multipleNodes = false;
    const NodeIdSet _selectedNodeIds;
    const NodeIdSet _nodeIds;
    std::vector<Edge> _edges;

public:
    DeleteNodesCommand(GraphModel* graphModel,
                       SelectionManager* selectionManager,
                       NodeIdSet nodeIds);

    QString description() const override;
    QString verb() const override;
    QString pastParticiple() const override;

    bool execute() override;
    void undo() override;
};

#endif // DELETENODESCOMMAND_H
