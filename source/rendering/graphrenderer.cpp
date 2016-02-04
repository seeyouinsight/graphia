#include "graphrenderer.h"

#include "graphcomponentscene.h"
#include "graphoverviewscene.h"

#include "../ui/graphcomponentinteractor.h"
#include "../ui/graphoverviewinteractor.h"

#include "../graph/graphmodel.h"

#include "../ui/graphquickitem.h"
#include "../ui/selectionmanager.h"

#include "../commands/command.h"

#include "../utils/cpp1x_hacks.h"

#include <QObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLDebugLogger>
#include <QColor>
#include <QQuickWindow>
#include <QEvent>
#include <QNativeGestureEvent>

#include <cstddef>

static bool loadShaderProgram(QOpenGLShaderProgram& program, const QString& vertexShader, const QString& fragmentShader)
{
    if(!program.addShaderFromSourceFile(QOpenGLShader::Vertex, vertexShader))
    {
        qCritical() << QObject::tr("Could not compile vertex shader. Log:") << program.log();
        return false;
    }

    if(!program.addShaderFromSourceFile(QOpenGLShader::Fragment, fragmentShader))
    {
        qCritical() << QObject::tr("Could not compile fragment shader. Log:") << program.log();
        return false;
    }

    if(!program.link())
    {
        qCritical() << QObject::tr("Could not link shader program. Log:") << program.log();
        return false;
    }

    return true;
}

void GraphInitialiser::initialiseFromGraph(const Graph *graph)
{
    for(auto componentId : graph->componentIds())
        onComponentAdded(graph, componentId, false);

    onGraphChanged(graph);
}

GPUGraphData::GPUGraphData()
{
    resolveOpenGLFunctions();
}

void GPUGraphData::initialise(QOpenGLShaderProgram& nodesShader,
                              QOpenGLShaderProgram& edgesShader)
{
    _sphere.setRadius(1.0f);
    _sphere.setRings(16);
    _sphere.setSlices(16);
    _sphere.create(nodesShader);

    _cylinder.setRadius(1.0f);
    _cylinder.setLength(1.0f);
    _cylinder.setSlices(8);
    _cylinder.create(edgesShader);

    prepareVertexBuffers();
    prepareNodeVAO(nodesShader);
    prepareEdgeVAO(edgesShader);
}

void GPUGraphData::prepareVertexBuffers()
{
    _nodeVBO.create();
    _nodeVBO.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    _edgeVBO.create();
    _edgeVBO.setUsagePattern(QOpenGLBuffer::DynamicDraw);
}

void GPUGraphData::prepareNodeVAO(QOpenGLShaderProgram& shader)
{
    _sphere.vertexArrayObject()->bind();
    shader.bind();

    _nodeVBO.bind();
    shader.enableAttributeArray("nodePosition");
    shader.enableAttributeArray("component");
    shader.enableAttributeArray("size");
    shader.enableAttributeArray("color");
    shader.enableAttributeArray("outlineColor");
    shader.setAttributeBuffer("nodePosition", GL_FLOAT, offsetof(NodeData, _position),     3,         sizeof(NodeData));
    glVertexAttribIPointer(shader.attributeLocation("component"),                          1, GL_INT, sizeof(NodeData),
                          reinterpret_cast<const void*>(offsetof(NodeData, _component)));
    shader.setAttributeBuffer("size",         GL_FLOAT, offsetof(NodeData, _size),         1,         sizeof(NodeData));
    shader.setAttributeBuffer("color",        GL_FLOAT, offsetof(NodeData, _color),        3,         sizeof(NodeData));
    shader.setAttributeBuffer("outlineColor", GL_FLOAT, offsetof(NodeData, _outlineColor), 3,         sizeof(NodeData));
    glVertexAttribDivisor(shader.attributeLocation("nodePosition"), 1);
    glVertexAttribDivisor(shader.attributeLocation("component"), 1);
    glVertexAttribDivisor(shader.attributeLocation("size"), 1);
    glVertexAttribDivisor(shader.attributeLocation("color"), 1);
    glVertexAttribDivisor(shader.attributeLocation("outlineColor"), 1);
    _nodeVBO.release();

    shader.release();
    _sphere.vertexArrayObject()->release();
}

void GPUGraphData::prepareEdgeVAO(QOpenGLShaderProgram& shader)
{
    _cylinder.vertexArrayObject()->bind();
    shader.bind();

    _edgeVBO.bind();
    shader.enableAttributeArray("sourcePosition");
    shader.enableAttributeArray("targetPosition");
    shader.enableAttributeArray("component");
    shader.enableAttributeArray("size");
    shader.enableAttributeArray("color");
    shader.enableAttributeArray("outlineColor");
    shader.setAttributeBuffer("sourcePosition", GL_FLOAT, offsetof(EdgeData, _sourcePosition), 3,         sizeof(EdgeData));
    shader.setAttributeBuffer("targetPosition", GL_FLOAT, offsetof(EdgeData, _targetPosition), 3,         sizeof(EdgeData));
    glVertexAttribIPointer(shader.attributeLocation("component"),                              1, GL_INT, sizeof(EdgeData),
                            reinterpret_cast<const void*>(offsetof(EdgeData, _component)));
    shader.setAttributeBuffer("size",           GL_FLOAT, offsetof(EdgeData, _size),           1,         sizeof(EdgeData));
    shader.setAttributeBuffer("color",          GL_FLOAT, offsetof(EdgeData, _color),          3,         sizeof(EdgeData));
    shader.setAttributeBuffer("outlineColor",   GL_FLOAT, offsetof(EdgeData, _outlineColor),   3,         sizeof(EdgeData));
    glVertexAttribDivisor(shader.attributeLocation("sourcePosition"), 1);
    glVertexAttribDivisor(shader.attributeLocation("targetPosition"), 1);
    glVertexAttribDivisor(shader.attributeLocation("component"), 1);
    glVertexAttribDivisor(shader.attributeLocation("size"), 1);
    glVertexAttribDivisor(shader.attributeLocation("color"), 1);
    glVertexAttribDivisor(shader.attributeLocation("outlineColor"), 1);
    _edgeVBO.release();

    shader.release();
    _cylinder.vertexArrayObject()->release();
}

void GPUGraphData::reset()
{
    _nodeData.clear();
    _edgeData.clear();
}

void GPUGraphData::upload()
{
    _nodeVBO.bind();
    _nodeVBO.allocate(_nodeData.data(), static_cast<int>(_nodeData.size()) * sizeof(NodeData));
    _nodeVBO.release();

    _edgeVBO.bind();
    _edgeVBO.allocate(_edgeData.data(), static_cast<int>(_edgeData.size()) * sizeof(EdgeData));
    _edgeVBO.release();
}

int GPUGraphData::numNodes() const
{
    return static_cast<int>(_nodeData.size());
}

int GPUGraphData::numEdges() const
{
    return static_cast<int>(_edgeData.size());
}

GraphRenderer::GraphRenderer(std::shared_ptr<GraphModel> graphModel,
                             CommandManager& commandManager,
                             std::shared_ptr<SelectionManager> selectionManager) :
    QObject(),
    OpenGLFunctions(),
    _graphModel(graphModel),
    _selectionManager(selectionManager),
    _componentRenderers(_graphModel->graph()),
    _hiddenNodes(_graphModel->graph()),
    _hiddenEdges(_graphModel->graph()),
    _layoutChanged(true)
{
    resolveOpenGLFunctions();

    loadShaderProgram(_screenShader, ":/shaders/screen.vert", ":/shaders/screen.frag");
    loadShaderProgram(_selectionShader, ":/shaders/screen.vert", ":/shaders/selection.frag");

    loadShaderProgram(_nodesShader, ":/shaders/instancednodes.vert", ":/shaders/ads.frag");
    loadShaderProgram(_edgesShader, ":/shaders/instancededges.vert", ":/shaders/ads.frag");

    loadShaderProgram(_selectionMarkerShader, ":/shaders/2d.vert", ":/shaders/selectionMarker.frag");
    loadShaderProgram(_debugLinesShader, ":/shaders/debuglines.vert", ":/shaders/debuglines.frag");

    prepareSelectionMarkerVAO();
    prepareQuad();

    _gpuGraphData.initialise(_nodesShader, _edgesShader);
    _gpuGraphDataAlpha.initialise(_nodesShader, _edgesShader);

    prepareComponentDataTexture();

    auto graph = &_graphModel->graph();

    connect(graph, &Graph::nodeAdded, this, &GraphRenderer::onNodeAdded, Qt::DirectConnection);
    connect(graph, &Graph::edgeAdded, this, &GraphRenderer::onEdgeAdded, Qt::DirectConnection);
    connect(graph, &Graph::nodeAddedToComponent, this, &GraphRenderer::onNodeAddedToComponent, Qt::DirectConnection);
    connect(graph, &Graph::edgeAddedToComponent, this, &GraphRenderer::onEdgeAddedToComponent, Qt::DirectConnection);

    connect(graph, &Graph::graphChanged, this, &GraphRenderer::onGraphChanged, Qt::DirectConnection);
    connect(graph, &Graph::componentAdded, this, &GraphRenderer::onComponentAdded, Qt::DirectConnection);

    _graphOverviewScene = new GraphOverviewScene(this);
    _graphComponentScene = new GraphComponentScene(this);

    connect(graph, &Graph::componentWillBeRemoved, this, &GraphRenderer::onComponentWillBeRemoved, Qt::DirectConnection);

    _graphOverviewInteractor = new GraphOverviewInteractor(_graphModel, _graphOverviewScene, commandManager, _selectionManager, this);
    _graphComponentInteractor = new GraphComponentInteractor(_graphModel, _graphComponentScene, commandManager, _selectionManager, this);

    initialiseFromGraph(graph);
    _graphOverviewScene->initialiseFromGraph(graph);
    _graphComponentScene->initialiseFromGraph(graph);

    if(graph->componentIds().size() == 1)
        switchToComponentMode(false);
    else
        switchToOverviewMode(false);

    connect(_selectionManager.get(), &SelectionManager::selectionChanged, this, &GraphRenderer::onSelectionChanged, Qt::DirectConnection);

    enableSceneUpdate();
}

GraphRenderer::~GraphRenderer()
{
    if(_componentDataTBO != 0)
    {
        glDeleteBuffers(1, &_componentDataTBO);
        _componentDataTBO = 0;
    }

    if(_componentDataTexture != 0)
    {
        glDeleteTextures(1, &_componentDataTexture);
        _componentDataTexture = 0;
    }

    if(_visualFBO != 0)
    {
        glDeleteFramebuffers(1, &_visualFBO);
        _visualFBO = 0;
    }

    _FBOcomplete = false;

    if(_colorTexture != 0)
    {
        glDeleteTextures(1, &_colorTexture);
        _colorTexture = 0;
    }

    if(_selectionTexture != 0)
    {
        glDeleteTextures(1, &_selectionTexture);
        _selectionTexture = 0;
    }

    if(_depthTexture != 0)
    {
        glDeleteTextures(1, &_depthTexture);
        _depthTexture = 0;
    }
}

void GraphRenderer::resize(int width, int height)
{
    _width = width;
    _height = height;
    _resized = true;

    if(width > 0 && height > 0)
        _FBOcomplete = prepareRenderBuffers(width, height);

    GLfloat w = static_cast<GLfloat>(_width);
    GLfloat h = static_cast<GLfloat>(_height);
    GLfloat quadData[] =
    {
        0, 0,
        w, 0,
        w, h,

        w, h,
        0, h,
        0, 0,
    };

    _screenQuadDataBuffer.bind();
    _screenQuadDataBuffer.allocate(quadData, static_cast<int>(sizeof(quadData)));
    _screenQuadDataBuffer.release();
}

void GraphRenderer::updateGPUDataIfRequired()
{
    if(!_gpuDataRequiresUpdate)
        return;

    _gpuDataRequiresUpdate = false;

    std::unique_lock<std::recursive_mutex> lock(_graphModel->nodePositions().mutex());

    int componentIndex = 0;

    auto& nodePositions = _graphModel->nodePositions();
    auto& nodeVisuals = _graphModel->nodeVisuals();
    auto& edgeVisuals = _graphModel->edgeVisuals();
    const QColor selectedOutLineColor = Qt::GlobalColor::white;
    const QColor deselectedOutLineColor = Qt::GlobalColor::black;

    _gpuGraphData.reset();
    _gpuGraphDataAlpha.reset();

    NodeArray<QVector3D> scaledAndSmoothedNodePositions(_graphModel->graph());

    for(auto& componentRendererRef : _componentRenderers)
    {
        GraphComponentRenderer* componentRenderer = componentRendererRef;
        if(!componentRenderer->visible())
            continue;

        auto& gpuGraphData = componentRenderer->fading() ? _gpuGraphDataAlpha : _gpuGraphData;

        for(auto nodeId : componentRenderer->nodeIds())
        {
            if(_hiddenNodes.get(nodeId))
                continue;

            const QVector3D nodePosition = nodePositions.getScaledAndSmoothed(nodeId);
            scaledAndSmoothedNodePositions[nodeId] = nodePosition;

            GPUGraphData::NodeData nodeData;
            nodeData._position[0] = nodePosition.x();
            nodeData._position[1] = nodePosition.y();
            nodeData._position[2] = nodePosition.z();
            nodeData._component = componentIndex;
            nodeData._size = nodeVisuals[nodeId]._size;
            nodeData._color[0] = nodeVisuals[nodeId]._color.redF();
            nodeData._color[1] = nodeVisuals[nodeId]._color.greenF();
            nodeData._color[2] = nodeVisuals[nodeId]._color.blueF();

            QColor outlineColor = _selectionManager && _selectionManager->nodeIsSelected(nodeId) ?
                selectedOutLineColor :
                deselectedOutLineColor;

            nodeData._outlineColor[0] = outlineColor.redF();
            nodeData._outlineColor[1] = outlineColor.greenF();
            nodeData._outlineColor[2] = outlineColor.blueF();

            gpuGraphData._nodeData.push_back(nodeData);
        }

        for(auto& edge : componentRenderer->edges())
        {
            if(_hiddenEdges.get(edge.id()) || _hiddenNodes.get(edge.sourceId()) || _hiddenNodes.get(edge.targetId()))
                continue;

            const QVector3D& sourcePosition = scaledAndSmoothedNodePositions[edge.sourceId()];
            const QVector3D& targetPosition = scaledAndSmoothedNodePositions[edge.targetId()];

            GPUGraphData::EdgeData edgeData;
            edgeData._sourcePosition[0] = sourcePosition.x();
            edgeData._sourcePosition[1] = sourcePosition.y();
            edgeData._sourcePosition[2] = sourcePosition.z();
            edgeData._targetPosition[0] = targetPosition.x();
            edgeData._targetPosition[1] = targetPosition.y();
            edgeData._targetPosition[2] = targetPosition.z();
            edgeData._component = componentIndex;
            edgeData._size = edgeVisuals[edge.id()]._size;
            edgeData._color[0] = edgeVisuals[edge.id()]._color.redF();
            edgeData._color[1] = edgeVisuals[edge.id()]._color.greenF();
            edgeData._color[2] = edgeVisuals[edge.id()]._color.blueF();
            edgeData._outlineColor[0] = deselectedOutLineColor.redF();
            edgeData._outlineColor[1] = deselectedOutLineColor.greenF();
            edgeData._outlineColor[2] = deselectedOutLineColor.blueF();

            gpuGraphData._edgeData.push_back(edgeData);
        }

        componentIndex++;
    }

    _gpuGraphData.upload();
    _gpuGraphDataAlpha.upload();
}

void GraphRenderer::updateGPUData(GraphRenderer::When when)
{
    _gpuDataRequiresUpdate = true;

    if(when == When::Now)
        updateGPUDataIfRequired();
}

void GraphRenderer::updateComponentGPUData()
{
    //FIXME this doesn't necessarily need to be entirely regenerated and rebuffered
    // every frame, so it makes sense to do partial updates as and when required.
    // OTOH, it's probably not ever going to be masses of data, so maybe we should
    // just suck it up; need to get a profiler on it and see how long we're spending
    // here transfering the buffer, when there are lots of components
    std::vector<GLfloat> componentData;

    for(auto& componentRendererRef : _componentRenderers)
    {
        GraphComponentRenderer* componentRenderer = componentRendererRef;
        if(!componentRenderer->visible())
            continue;

        for(int i = 0; i < 16; i++)
            componentData.push_back(componentRenderer->modelViewMatrix().data()[i]);

        for(int i = 0; i < 16; i++)
            componentData.push_back(componentRenderer->projectionMatrix().data()[i]);

        componentData.push_back(componentRenderer->alpha());

        // Padding
        for(int i = 0; i < 3; i++)
            componentData.push_back(0.0f);
    }

    glBindBuffer(GL_TEXTURE_BUFFER, _componentDataTBO);
    glBufferData(GL_TEXTURE_BUFFER, componentData.size() * sizeof(GLfloat), componentData.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_TEXTURE_BUFFER, 0);
}

void GraphRenderer::clear()
{
    glViewport(0, 0, _width, _height);
    glBindFramebuffer(GL_FRAMEBUFFER, _visualFBO);

    // Color buffer
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Selection buffer
    glDrawBuffer(GL_COLOR_ATTACHMENT1);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void GraphRenderer::setScene(Scene* scene)
{
    if(_scene != nullptr)
    {
        _scene->setVisible(false);
        _scene->onHide();
    }

    _scene = scene;

    _scene->setVisible(true);
    _scene->onShow();

    _scene->setViewportSize(_width, _height);
}

GraphRenderer::Mode GraphRenderer::mode() const
{
    return _mode;
}

void GraphRenderer::setMode(Mode mode)
{
    if(mode != _mode)
    {
        _mode = mode;
        emit modeChanged();
    }
}

void GraphRenderer::resetTime()
{
    _time.start();
    _lastTime = 0.0f;
}

float GraphRenderer::secondsElapsed()
{
    float time = _time.elapsed() / 1000.0f;
    float dTime = time - _lastTime;
    _lastTime = time;

    return dTime;
}

bool GraphRenderer::transitionActive() const
{
    return _transition.active() || _scene->transitionActive() || _modeTransitionInProgress;
}

//FIXME this reference counting thing is rubbish, and gives rise to hacks
void GraphRenderer::rendererStartedTransition()
{
    Q_ASSERT(_numTransitioningRenderers >= 0);

    if(_numTransitioningRenderers == 0)
    {
        emit userInteractionStarted();
        resetTime();
    }

    _numTransitioningRenderers++;
}

void GraphRenderer::rendererFinishedTransition()
{
    _numTransitioningRenderers--;

    Q_ASSERT(_numTransitioningRenderers >= 0);

    if(_numTransitioningRenderers == 0)
        emit userInteractionFinished();
}

void GraphRenderer::sceneFinishedTransition()
{
    clearHiddenElements();
    updateGPUData(When::Later);
}

void GraphRenderer::executeOnRendererThread(DeferredExecutor::TaskFn task, const QString& description)
{
    _preUpdateExecutor.enqueue(task, description);
    emit taskAddedToExecutor();
}

void GraphRenderer::onNodeAdded(const Graph*, const Node* node)
{
    _hiddenNodes.set(node->id(), true);
}

void GraphRenderer::onEdgeAdded(const Graph*, const Edge* edge)
{
    _hiddenEdges.set(edge->id(), true);
}

void GraphRenderer::onNodeAddedToComponent(const Graph*, NodeId nodeId, ComponentId)
{
    _hiddenNodes.set(nodeId, true);
}

void GraphRenderer::onEdgeAddedToComponent(const Graph*, EdgeId edgeId, ComponentId)
{
    _hiddenEdges.set(edgeId, true);
}

void GraphRenderer::finishTransitionToOverviewMode()
{
    setScene(_graphOverviewScene);
    setInteractor(_graphOverviewInteractor);

    if(_modeTransitionInProgress)
    {
        // When we first change to overview mode we want all
        // the renderers to be in their reset state
        for(auto componentId : _graphModel->graph().componentIds())
        {
            auto renderer = componentRendererForId(componentId);
            renderer->resetView();
        }

        _graphOverviewScene->startTransitionFromComponentMode(_graphComponentScene->componentId(),
                                                              0.3f, Transition::Type::EaseInEaseOut);
    }

    updateGPUData(When::Later);
}

void GraphRenderer::finishTransitionToComponentMode()
{
    setScene(_graphComponentScene);
    setInteractor(_graphComponentInteractor);

    if(!_graphComponentScene->savedViewIsReset())
    {
        // Go back to where we were before
        if(_modeTransitionInProgress)
            _graphComponentScene->startTransition(0.3f, Transition::Type::EaseInEaseOut);

        _graphComponentScene->restoreViewData();
    }

    updateGPUData(When::Later);
}

void GraphRenderer::finishModeTransition()
{
    if(_modeTransitionInProgress)
    {
        switch(mode())
        {
            case GraphRenderer::Mode::Overview:
                finishTransitionToOverviewMode();
                break;

            case GraphRenderer::Mode::Component:
                finishTransitionToComponentMode();
                break;

            default:
                break;
        }

        _modeTransitionInProgress = false;
    }
}

void GraphRenderer::switchToOverviewMode(bool doTransition)
{
    executeOnRendererThread([this, doTransition]
    {
        // Refuse to switch to overview mode if there is nothing to display
        if(_graphModel->graph().numComponents() == 0)
            return;

        // So that we can return to the current view parameters later
        _graphComponentScene->saveViewData();

        if(mode() != GraphRenderer::Mode::Overview && doTransition &&
           _graphComponentScene->componentRenderer() != nullptr)
        {
            if(!_graphComponentScene->viewIsReset())
            {
                if(!_transition.active())
                    rendererStartedTransition(); // Partner to * below

                _graphComponentScene->startTransition(0.3f, Transition::Type::EaseInEaseOut,
                [this]
                {
                    _modeTransitionInProgress = true;
                    setMode(GraphRenderer::Mode::Overview);
                    rendererFinishedTransition(); // *
                });

                _graphComponentScene->resetView(false);
            }
            else
            {
                _modeTransitionInProgress = true;
                setMode(GraphRenderer::Mode::Overview);
            }
        }
        else
        {
            setMode(GraphRenderer::Mode::Overview);
            finishTransitionToOverviewMode();
        }

    }, "GraphRenderer::switchToOverviewMode");
}

void GraphRenderer::switchToComponentMode(bool doTransition, ComponentId componentId)
{
    executeOnRendererThread([this, componentId, doTransition]
    {
        _graphComponentScene->setComponentId(componentId);

        if(mode() != GraphRenderer::Mode::Component && doTransition)
        {
            if(!_transition.active())
                rendererStartedTransition();

            _graphOverviewScene->startTransitionToComponentMode(_graphComponentScene->componentId(),
                                                                0.3f, Transition::Type::EaseInEaseOut,
            [this]
            {
                _modeTransitionInProgress = true;
                setMode(GraphRenderer::Mode::Component);
                rendererFinishedTransition();
            });
        }
        else
        {
            setMode(GraphRenderer::Mode::Component);
            finishTransitionToComponentMode();
        }

    }, "GraphRenderer::switchToComponentMode");
}

void GraphRenderer::onGraphChanged(const Graph* graph)
{
    _numComponents = graph->numComponents();

    //FIXME: this makes me feel dirty
    // This is a slight hack to prevent there being a gap in which
    // layout can occur, inbetween the graph change and user
    // interaction phases
    rendererStartedTransition();

    executeOnRendererThread([this]
    {
        for(ComponentId componentId : _graphModel->graph().componentIds())
        {
            componentRendererForId(componentId)->initialise(_graphModel, componentId,
                                                            _selectionManager, this);
        }

        updateGPUData(When::Later);

        // Partner to the hack described above
        rendererFinishedTransition();
    }, "GraphRenderer::onGraphChanged update");
}

void GraphRenderer::onComponentAdded(const Graph*, ComponentId componentId, bool)
{
    executeOnRendererThread([this, componentId]
    {
        componentRendererForId(componentId)->initialise(_graphModel, componentId,
                                                        _selectionManager, this);
    }, "GraphRenderer::onComponentAdded");
}

void GraphRenderer::onComponentWillBeRemoved(const Graph*, ComponentId componentId, bool)
{
    executeOnRendererThread([this, componentId]
    {
        componentRendererForId(componentId)->cleanup();
    }, QString("GraphRenderer::onComponentWillBeRemoved (cleanup) component %1").arg(static_cast<int>(componentId)));
}

void GraphRenderer::onSelectionChanged(const SelectionManager*)
{
    executeOnRendererThread([this]
    {
        updateGPUData(When::Later);
    }, "GraphRenderer::onSelectionChanged");
}

void GraphRenderer::onCommandWillExecuteAsynchronously(const Command*)
{
    disableSceneUpdate();
}

void GraphRenderer::onCommandCompleted(const Command*, const QString&)
{
    enableSceneUpdate();
    update();
}

void GraphRenderer::onLayoutChanged()
{
    _layoutChanged = true;
}

void GraphRenderer::onComponentFadingChanged(ComponentId)
{
    updateGPUData(When::Later);
}

void GraphRenderer::onComponentCleanup(ComponentId)
{
    updateGPUData(When::Later);
}

void GraphRenderer::resetView()
{
    if(_scene != nullptr)
        _scene->resetView();
}

bool GraphRenderer::viewIsReset() const
{
    if(_scene != nullptr)
        return _scene->viewIsReset();

    return true;
}

static void setShaderADSParameters(QOpenGLShaderProgram& program)
{
    struct Light
    {
        Light() {}
        Light(const QVector4D& _position, const QVector3D& _intensity) :
            position(_position), intensity(_intensity)
        {}

        QVector4D position;
        QVector3D intensity;
    };

    std::vector<Light> lights;
    lights.emplace_back(QVector4D(-20.0f, 0.0f, 3.0f, 1.0f), QVector3D(0.6f, 0.6f, 0.6f));
    lights.emplace_back(QVector4D(0.0f, 0.0f, 0.0f, 1.0f), QVector3D(0.2f, 0.2f, 0.2f));
    lights.emplace_back(QVector4D(10.0f, -10.0f, -10.0f, 1.0f), QVector3D(0.4f, 0.4f, 0.4f));

    int numberOfLights = static_cast<int>(lights.size());

    program.setUniformValue("numberOfLights", numberOfLights);

    for(int i = 0; i < numberOfLights; i++)
    {
        QByteArray positionId = QString("lights[%1].position").arg(i).toLatin1();
        program.setUniformValue(positionId.data(), lights[i].position);

        QByteArray intensityId = QString("lights[%1].intensity").arg(i).toLatin1();
        program.setUniformValue(intensityId.data(), lights[i].intensity);
    }

    program.setUniformValue("material.ks", QVector3D(1.0f, 1.0f, 1.0f));
    program.setUniformValue("material.ka", QVector3D(0.1f, 0.1f, 0.1f));
    program.setUniformValue("material.shininess", 50.0f);
}

void GraphRenderer::renderNodes(GPUGraphData& gpuGraphData)
{
    _nodesShader.bind();
    setShaderADSParameters(_nodesShader);

    gpuGraphData._nodeVBO.bind();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_BUFFER, _componentDataTexture);
    _nodesShader.setUniformValue("componentData", 0);

    gpuGraphData._sphere.vertexArrayObject()->bind();
    glDrawElementsInstanced(GL_TRIANGLES, gpuGraphData._sphere.indexCount(),
                            GL_UNSIGNED_INT, 0, gpuGraphData.numNodes());
    gpuGraphData._sphere.vertexArrayObject()->release();

    glBindTexture(GL_TEXTURE_BUFFER, 0);
    gpuGraphData._nodeVBO.release();
    _nodesShader.release();
}

void GraphRenderer::renderEdges(GPUGraphData& gpuGraphData)
{
    _edgesShader.bind();
    setShaderADSParameters(_edgesShader);

    gpuGraphData._edgeVBO.bind();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_BUFFER, _componentDataTexture);
    _edgesShader.setUniformValue("componentData", 0);

    gpuGraphData._cylinder.vertexArrayObject()->bind();
    glDrawElementsInstanced(GL_TRIANGLES, gpuGraphData._cylinder.indexCount(),
                            GL_UNSIGNED_INT, 0, gpuGraphData.numEdges());
    gpuGraphData._cylinder.vertexArrayObject()->release();

    glBindTexture(GL_TEXTURE_BUFFER, 0);
    gpuGraphData._edgeVBO.release();
    _edgesShader.release();
}

void GraphRenderer::renderGraph(GPUGraphData& gpuGraphData)
{
    GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, drawBuffers);
    glClear(GL_DEPTH_BUFFER_BIT);

    renderNodes(gpuGraphData);
    renderEdges(gpuGraphData);
}

void GraphRenderer::renderScene()
{
    ifSceneUpdateEnabled([this]
    {
        finishModeTransition();
        _preUpdateExecutor.execute();
    });

    if(_scene == nullptr)
        return;

    if(_resized)
    {
        _scene->setViewportSize(_width, _height);
        _resized = false;
    }

    ifSceneUpdateEnabled([this]
    {
        // _synchronousLayoutChanged can only ever be (atomically) true in this scope
        _synchronousLayoutChanged = _layoutChanged.exchange(false);

        // If there is a transition active then we'll need another
        // frame once we're finished with this one
        if(transitionActive())
            update(); // QQuickFramebufferObject::Renderer::update

        float dTime = secondsElapsed();
        _transition.update(dTime);
        _scene->update(dTime);

        if(layoutChanged())
            updateGPUData(When::Later);

        updateGPUDataIfRequired();
        updateComponentGPUData();

        _synchronousLayoutChanged = false;
    });

    if(!_FBOcomplete)
    {
        qWarning() << "Attempting to render without a complete FBO";
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    glBindFramebuffer(GL_FRAMEBUFFER, _visualFBO);

    renderGraph(_gpuGraphDataAlpha); // Alpha blended first...
    renderGraph(_gpuGraphData);      // ...then opaque
}

void GraphRenderer::render2D()
{
    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, _width, _height);

    QMatrix4x4 m;
    m.ortho(0.0f, _width, 0.0f, _height, -1.0f, 1.0f);

    if(!_selectionRect.isNull())
    {
        const QColor color(Qt::GlobalColor::white);

        QRect r;
        r.setLeft(_selectionRect.left());
        r.setRight(_selectionRect.right());
        r.setTop(_height - _selectionRect.top());
        r.setBottom(_height - _selectionRect.bottom());

        std::vector<GLfloat> quadData;

        quadData.push_back(r.left()); quadData.push_back(r.bottom());
        quadData.push_back(color.redF()); quadData.push_back(color.blueF()); quadData.push_back(color.greenF());
        quadData.push_back(r.right()); quadData.push_back(r.bottom());
        quadData.push_back(color.redF()); quadData.push_back(color.blueF()); quadData.push_back(color.greenF());
        quadData.push_back(r.right()); quadData.push_back(r.top());
        quadData.push_back(color.redF()); quadData.push_back(color.blueF()); quadData.push_back(color.greenF());

        quadData.push_back(r.right()); quadData.push_back(r.top());
        quadData.push_back(color.redF()); quadData.push_back(color.blueF()); quadData.push_back(color.greenF());
        quadData.push_back(r.left());  quadData.push_back(r.top());
        quadData.push_back(color.redF()); quadData.push_back(color.blueF()); quadData.push_back(color.greenF());
        quadData.push_back(r.left());  quadData.push_back(r.bottom());
        quadData.push_back(color.redF()); quadData.push_back(color.blueF()); quadData.push_back(color.greenF());

        glDrawBuffer(GL_COLOR_ATTACHMENT1);

        _selectionMarkerDataBuffer.bind();
        _selectionMarkerDataBuffer.allocate(quadData.data(), static_cast<int>(quadData.size()) * sizeof(GLfloat));

        _selectionMarkerShader.bind();
        _selectionMarkerShader.setUniformValue("projectionMatrix", m);

        _selectionMarkerDataVAO.bind();
        glDrawArrays(GL_TRIANGLES, 0, 6);
        _selectionMarkerDataVAO.release();

        _selectionMarkerShader.release();
        _selectionMarkerDataBuffer.release();
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
}

QOpenGLFramebufferObject* GraphRenderer::createFramebufferObject(const QSize& size)
{
    // Piggy back our FBO resize on to Qt's
    resize(size.width(), size.height());

    return new QOpenGLFramebufferObject(size);
}

void GraphRenderer::render()
{
    if(!_FBOcomplete)
    {
        qWarning() << "Attempting to render incomplete FBO";
        return;
    }

    clear();
    renderScene();
    render2D();
    finishRender();

    std::unique_lock<std::mutex> lock(_resetOpenGLStateMutex);
    resetOpenGLState();
}

void GraphRenderer::synchronize(QQuickFramebufferObject* item)
{
    if(!resetOpenGLState)
    {
        resetOpenGLState = [item]
        {
            if(item->window() != nullptr)
                item->window()->resetOpenGLState();
        };

        connect(item, &QObject::destroyed, this, [this]
        {
            std::unique_lock<std::mutex> lock(_resetOpenGLStateMutex);
            resetOpenGLState = []{};
        }, Qt::DirectConnection);
    }

    auto graphQuickItem = qobject_cast<GraphQuickItem*>(item);

    if(graphQuickItem->viewResetPending())
        resetView();

    if(graphQuickItem->overviewModeSwitchPending())
        switchToOverviewMode();

    ifSceneUpdateEnabled([this, &graphQuickItem]
    {
        if(_scene != nullptr)
        {
            //FIXME try delivering these events by queued connection instead
            while(graphQuickItem->eventsPending())
            {
                auto e = std::move(graphQuickItem->nextEvent());
                auto mouseEvent = dynamic_cast<QMouseEvent*>(e.get());
                auto wheelEvent = dynamic_cast<QWheelEvent*>(e.get());
                auto nativeGestureEvent = dynamic_cast<QNativeGestureEvent*>(e.get());

                switch(e->type())
                {
                case QEvent::Type::MouseButtonPress:    _interactor->mousePressEvent(mouseEvent);               break;
                case QEvent::Type::MouseButtonRelease:  _interactor->mouseReleaseEvent(mouseEvent);             break;
                case QEvent::Type::MouseMove:           _interactor->mouseMoveEvent(mouseEvent);                break;
                case QEvent::Type::MouseButtonDblClick: _interactor->mouseDoubleClickEvent(mouseEvent);         break;
                case QEvent::Type::Wheel:               _interactor->wheelEvent(wheelEvent);                    break;
                case QEvent::Type::NativeGesture:       _interactor->nativeGestureEvent(nativeGestureEvent);    break;
                default: break;
                }
            }
        }
    });

    // Tell the QuickItem what we're doing
    graphQuickItem->setViewIsReset(viewIsReset());
    graphQuickItem->setCanEnterOverviewMode(mode() != Mode::Overview && _numComponents > 1);
}

void GraphRenderer::finishRender()
{
    if(!framebufferObject()->bind())
        qWarning() << "QQuickFrameBufferobject::Renderer FBO not bound";

    glViewport(0, 0, framebufferObject()->width(), framebufferObject()->height());

    glClearColor(0.75f, 0.75f, 0.75f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    glDisable(GL_DEPTH_TEST);

    QMatrix4x4 m;
    m.ortho(0, _width, 0, _height, -1.0f, 1.0f);

    _screenQuadDataBuffer.bind();

    _screenQuadVAO.bind();
    glActiveTexture(GL_TEXTURE0);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
    glEnable(GL_BLEND);

    // Color texture
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, _colorTexture);

    _screenShader.bind();
    _screenShader.setUniformValue("projectionMatrix", m);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    _screenShader.release();

    if(!transitionActive())
    {
        // Selection texture
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, _selectionTexture);

        _selectionShader.bind();
        _selectionShader.setUniformValue("projectionMatrix", m);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        _selectionShader.release();
    }

    _screenQuadVAO.release();
    _screenQuadDataBuffer.release();
}

GraphComponentRenderer* GraphRenderer::componentRendererForId(ComponentId componentId) const
{
    if(componentId.isNull())
        return nullptr;

    GraphComponentRenderer* renderer = _componentRenderers.at(componentId);
    Q_ASSERT(renderer != nullptr);
    return renderer;
}

bool GraphRenderer::prepareRenderBuffers(int width, int height)
{
    bool valid;

    // Color texture
    if(_colorTexture == 0)
        glGenTextures(1, &_colorTexture);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, _colorTexture);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, NUM_MULTISAMPLES, GL_RGBA, width, height, GL_FALSE);
    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAX_LEVEL, 0);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

    // Selection texture
    if(_selectionTexture == 0)
        glGenTextures(1, &_selectionTexture);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, _selectionTexture);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, NUM_MULTISAMPLES, GL_RGBA, width, height, GL_FALSE);
    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAX_LEVEL, 0);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

    // Depth texture
    if(_depthTexture == 0)
        glGenTextures(1, &_depthTexture);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, _depthTexture);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, NUM_MULTISAMPLES, GL_DEPTH_COMPONENT, width, height, GL_FALSE);
    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAX_LEVEL, 0);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

    // Visual FBO
    if(_visualFBO == 0)
        glGenFramebuffers(1, &_visualFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, _visualFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, _colorTexture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D_MULTISAMPLE, _selectionTexture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, _depthTexture, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    valid = (status == GL_FRAMEBUFFER_COMPLETE);

    Q_ASSERT(valid);
    return valid;
}

void GraphRenderer::prepareSelectionMarkerVAO()
{
    _selectionMarkerDataVAO.create();

    _selectionMarkerDataVAO.bind();
    _selectionMarkerShader.bind();

    _selectionMarkerDataBuffer.create();
    _selectionMarkerDataBuffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    _selectionMarkerDataBuffer.bind();

    _selectionMarkerShader.enableAttributeArray("position");
    _selectionMarkerShader.enableAttributeArray("color");
    _selectionMarkerShader.disableAttributeArray("texCoord");
    _selectionMarkerShader.setAttributeBuffer("position", GL_FLOAT, 0, 2, 5 * sizeof(GLfloat));
    _selectionMarkerShader.setAttributeBuffer("color", GL_FLOAT, 2 * sizeof(GLfloat), 3, 5 * sizeof(GLfloat));

    _selectionMarkerDataBuffer.release();
    _selectionMarkerDataVAO.release();
    _selectionMarkerShader.release();
}

void GraphRenderer::prepareQuad()
{
    if(!_screenQuadVAO.isCreated())
        _screenQuadVAO.create();

    _screenQuadVAO.bind();

    _screenQuadDataBuffer.create();
    _screenQuadDataBuffer.bind();
    _screenQuadDataBuffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    _screenShader.bind();
    _screenShader.enableAttributeArray("position");
    _screenShader.setAttributeBuffer("position", GL_FLOAT, 0, 2, 2 * sizeof(GLfloat));
    _screenShader.setUniformValue("frameBufferTexture", 0);
    _screenShader.setUniformValue("multisamples", NUM_MULTISAMPLES);
    _screenShader.release();

    _selectionShader.bind();
    _selectionShader.enableAttributeArray("position");
    _selectionShader.setAttributeBuffer("position", GL_FLOAT, 0, 2, 2 * sizeof(GLfloat));
    _selectionShader.setUniformValue("frameBufferTexture", 0);
    _selectionShader.setUniformValue("multisamples", NUM_MULTISAMPLES);
    _selectionShader.release();

    _screenQuadDataBuffer.release();
    _screenQuadVAO.release();
}

void GraphRenderer::prepareComponentDataTexture()
{
    if(_componentDataTexture == 0)
        glGenTextures(1, &_componentDataTexture);

    if(_componentDataTBO == 0)
        glGenBuffers(1, &_componentDataTBO);

    glBindTexture(GL_TEXTURE_BUFFER, _componentDataTexture);
    glBindBuffer(GL_TEXTURE_BUFFER, _componentDataTBO);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, _componentDataTBO);
    glBindBuffer(GL_TEXTURE_BUFFER, 0);
    glBindTexture(GL_TEXTURE_BUFFER, 0);
}

void GraphRenderer::enableSceneUpdate()
{
    std::unique_lock<std::mutex> lock(_sceneUpdateMutex);
    _sceneUpdateEnabled = true;
    resetTime();
}

void GraphRenderer::disableSceneUpdate()
{
    std::unique_lock<std::mutex> lock(_sceneUpdateMutex);
    _sceneUpdateEnabled = false;
}

void GraphRenderer::ifSceneUpdateEnabled(const std::function<void ()>& f)
{
    std::unique_lock<std::mutex> lock(_sceneUpdateMutex);
    if(_sceneUpdateEnabled)
        f();
}

void GraphRenderer::clearHiddenElements()
{
    _hiddenNodes.resetElements();
    _hiddenEdges.resetElements();
}
