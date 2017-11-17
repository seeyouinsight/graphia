#ifndef BASEGENERICPLUGIN_H
#define BASEGENERICPLUGIN_H

#include "baseplugin.h"

#include "shared/plugins/usernodedata.h"
#include "shared/plugins/nodeattributetablemodel.h"

#include "shared/graph/grapharray.h"

#include <memory>

// This implements some basic functionality that will be common to plugins
// that load from generic graph file formats

class BaseGenericPluginInstance : public BasePluginInstance
{
    Q_OBJECT

    Q_PROPERTY(QString selectedNodeNames READ selectedNodeNames NOTIFY selectedNodeNamesChanged)
    Q_PROPERTY(QAbstractTableModel* nodeAttributeTableModel READ nodeAttributeTableModel CONSTANT)

private:
    std::unique_ptr<EdgeArray<float>> _edgeWeights;

    UserNodeData _userNodeData;

    NodeAttributeTableModel _nodeAttributeTableModel;
    QAbstractTableModel* nodeAttributeTableModel() { return &_nodeAttributeTableModel; }

public:
    BaseGenericPluginInstance();

    std::unique_ptr<IParser> parserForUrlTypeName(const QString& urlTypeName) override;

    void setEdgeWeight(EdgeId edgeId, float weight);

    QByteArray save(IMutableGraph&, const ProgressFn&) const override;
    bool load(const QByteArray&, int, IMutableGraph&, const ProgressFn&) override;

private:
    void initialise(const IPlugin* plugin, IDocument* document,
                    const IParserThread* parserThread) override;

    QString selectedNodeNames() const;

private slots:
    void onLoadSuccess();

    void onSelectionChanged(const ISelectionManager* selectionManager);

signals:
    void selectedNodeNamesChanged();
};

class BaseGenericPlugin : public BasePlugin
{
    Q_OBJECT

public:
    BaseGenericPlugin();

    QStringList identifyUrl(const QUrl& url) const override;

    bool editable() const override { return true; }
};

#endif // BASEGENERICPLUGIN_H
