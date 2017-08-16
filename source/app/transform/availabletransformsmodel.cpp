#include "availabletransformsmodel.h"

#include "application.h"
#include "graph/graphmodel.h"

AvailableTransformsModel::AvailableTransformsModel(const GraphModel& graphModel,
                                                   QObject* parent) :
    QAbstractListModel(parent),
    _graphModel(&graphModel)
{
    _transformNames = graphModel.availableTransformNames();
}

QVariant AvailableTransformsModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid() || index.row() >= _transformNames.size())
        return {};

    auto transformName = _transformNames.at(index.row());

    if(role != Qt::DisplayRole)
    {
        auto transform = _graphModel->transformFactory(transformName);

        if(transform == nullptr)
            return {};

        switch(role)
        {
        case Roles::TransformTypeRole:
        {
            if(!transform->declaredAttributes().empty())
                return tr("Analyses");

            return tr("Transforms");
        }
        default:
            return {};
        }
    }

    return transformName;
}

int AvailableTransformsModel::rowCount(const QModelIndex& /*parentIndex*/) const
{
    return _transformNames.size();
}

QVariant AvailableTransformsModel::get(const QModelIndex& index) const
{
    return data(index, Qt::DisplayRole).toString();
}

QHash<int, QByteArray> AvailableTransformsModel::roleNames() const
{
    auto names = QAbstractItemModel::roleNames();

    names[Roles::TransformTypeRole] = "type";

    return names;
}

void registerAvailableTransformsModelType()
{
    qmlRegisterInterface<AvailableTransformsModel>("AvailableTransformsModel");
}

Q_COREAPP_STARTUP_FUNCTION(registerAvailableTransformsModelType)