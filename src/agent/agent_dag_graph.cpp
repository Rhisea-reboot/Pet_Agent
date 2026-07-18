#include "vpet/agent/agent_dag_graph.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QQueue>

namespace vpet
{

AgentDagGraph::AgentDagGraph()
    : m_adjacentList()
    , m_inDegree()
    , m_nodeNames()
    , m_nodeIndexMap()
{
}

bool AgentDagGraph::LoadFromJsonFile(const QString &configPath, QString &errorMessage)
{
    if (configPath.trimmed().isEmpty())
    {
        errorMessage = QStringLiteral("Agent DAG config path is empty.");
        return false;
    }

    QFile file(configPath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        errorMessage = QStringLiteral("Failed to open Agent DAG config file.");
        return false;
    }

    const QByteArray jsonData = file.readAll();
    file.close();

    return LoadFromJsonData(jsonData, errorMessage);
}

bool AgentDagGraph::LoadFromJsonData(const QByteArray &jsonData, QString &errorMessage)
{
    if (jsonData.isEmpty())
    {
        errorMessage = QStringLiteral("Agent DAG config data is empty.");
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        errorMessage = QStringLiteral("Agent DAG JSON parse error: %1").arg(
                           parseError.errorString());
        return false;
    }

    if (!document.isObject())
    {
        errorMessage = QStringLiteral("Agent DAG root is not a JSON object.");
        return false;
    }

    const QJsonObject rootObject = document.object();
    const QJsonValue nodesValue = rootObject.value(QStringLiteral("nodes"));
    const QJsonValue edgesValue = rootObject.value(QStringLiteral("edges"));

    if (!nodesValue.isArray())
    {
        errorMessage = QStringLiteral("Agent DAG nodes field is missing or invalid.");
        return false;
    }

    if (!edgesValue.isArray())
    {
        errorMessage = QStringLiteral("Agent DAG edges field is missing or invalid.");
        return false;
    }

    Clear();

    const QJsonArray nodesArray = nodesValue.toArray();

    if (nodesArray.isEmpty())
    {
        errorMessage = QStringLiteral("Agent DAG node list is empty.");
        return false;
    }

    Reset(nodesArray.size());

    for (const QJsonValue &nodeValue : nodesArray)
    {
        if (!nodeValue.isString())
        {
            Clear();
            errorMessage = QStringLiteral("Agent DAG node entry is not a string.");
            return false;
        }

        if (!AddNode(nodeValue.toString(), errorMessage))
        {
            Clear();
            return false;
        }
    }

    const QJsonArray edgesArray = edgesValue.toArray();

    for (const QJsonValue &edgeValue : edgesArray)
    {
        if (!edgeValue.isObject())
        {
            Clear();
            errorMessage = QStringLiteral("Agent DAG edge entry is not an object.");
            return false;
        }

        const QJsonObject edgeObject = edgeValue.toObject();
        const QString fromNode = edgeObject.value(QStringLiteral("from")).toString();
        const QString toNode = edgeObject.value(QStringLiteral("to")).toString();

        if (!AddEdge(fromNode, toNode, errorMessage))
        {
            Clear();
            return false;
        }
    }

    QVector<QString> order;

    if (!TopologicalSort(order, errorMessage))
    {
        Clear();
        return false;
    }

    return true;
}

bool AgentDagGraph::TopologicalSort(QVector<QString> &order, QString &errorMessage) const
{
    order.clear();

    if (m_nodeNames.isEmpty())
    {
        errorMessage = QStringLiteral("Agent DAG graph is empty.");
        return false;
    }

    QVector<int> inDegree = m_inDegree;
    QQueue<int> zeroInDegreeQueue;

    for (int index = 0; index < inDegree.size(); index += 1)
    {
        if (inDegree.at(index) == 0)
        {
            zeroInDegreeQueue.enqueue(index);
        }
    }

    while (!zeroInDegreeQueue.isEmpty())
    {
        const int currentIndex = zeroInDegreeQueue.dequeue();
        order.append(m_nodeNames.at(currentIndex));

        for (const _tagAgentDagEdge &edge : m_adjacentList.at(currentIndex))
        {
            const int targetIndex = edge.targetIndex;

            if ((targetIndex < 0) || (targetIndex >= inDegree.size()))
            {
                errorMessage = QStringLiteral("Agent DAG edge target index is invalid.");
                order.clear();
                return false;
            }

            inDegree[targetIndex] -= 1;

            if (inDegree.at(targetIndex) == 0)
            {
                zeroInDegreeQueue.enqueue(targetIndex);
            }
        }
    }

    if (order.size() != m_nodeNames.size())
    {
        errorMessage = QStringLiteral("Agent DAG contains a cycle.");
        order.clear();
        return false;
    }

    return true;
}

bool AgentDagGraph::IsEmpty() const
{
    return m_nodeNames.isEmpty();
}

int AgentDagGraph::GetNodeCount() const
{
    return m_nodeNames.size();
}

QVector<QString> AgentDagGraph::GetNodeNames() const
{
    return m_nodeNames;
}

void AgentDagGraph::Clear()
{
    m_adjacentList.clear();
    m_inDegree.clear();
    m_nodeNames.clear();
    m_nodeIndexMap.clear();
}

void AgentDagGraph::Reset(int nodeCount)
{
    if (nodeCount <= 0)
    {
        Clear();
        return;
    }

    m_adjacentList.clear();
    m_adjacentList.resize(nodeCount);
    m_inDegree.clear();
    m_inDegree.resize(nodeCount);
    m_nodeNames.clear();
    m_nodeNames.reserve(nodeCount);
    m_nodeIndexMap.clear();
}

bool AgentDagGraph::AddNode(const QString &nodeName, QString &errorMessage)
{
    const QString normalizedNodeName = nodeName.trimmed();

    if (normalizedNodeName.isEmpty())
    {
        errorMessage = QStringLiteral("Agent DAG node name is empty.");
        return false;
    }

    if (m_nodeIndexMap.contains(normalizedNodeName))
    {
        errorMessage = QStringLiteral("Agent DAG contains duplicate node: %1").arg(
                           normalizedNodeName);
        return false;
    }

    const int nodeIndex = m_nodeNames.size();

    if (nodeIndex >= m_adjacentList.size())
    {
        errorMessage = QStringLiteral("Agent DAG node count exceeds declared size.");
        return false;
    }

    m_nodeNames.append(normalizedNodeName);
    m_nodeIndexMap.insert(normalizedNodeName, nodeIndex);

    return true;
}

bool AgentDagGraph::AddEdge(const QString &fromNode,
                            const QString &toNode,
                            QString &errorMessage)
{
    const QString normalizedFromNode = fromNode.trimmed();
    const QString normalizedToNode = toNode.trimmed();

    if (normalizedFromNode.isEmpty() || normalizedToNode.isEmpty())
    {
        errorMessage = QStringLiteral("Agent DAG edge endpoint is empty.");
        return false;
    }

    if (!m_nodeIndexMap.contains(normalizedFromNode))
    {
        errorMessage = QStringLiteral("Agent DAG edge source node is unknown: %1").arg(
                           normalizedFromNode);
        return false;
    }

    if (!m_nodeIndexMap.contains(normalizedToNode))
    {
        errorMessage = QStringLiteral("Agent DAG edge target node is unknown: %1").arg(
                           normalizedToNode);
        return false;
    }

    const int fromIndex = m_nodeIndexMap.value(normalizedFromNode);
    const int toIndex = m_nodeIndexMap.value(normalizedToNode);

    if (fromIndex == toIndex)
    {
        errorMessage = QStringLiteral("Agent DAG self edge is not allowed: %1").arg(
                           normalizedFromNode);
        return false;
    }

    for (const _tagAgentDagEdge &edge : m_adjacentList.at(fromIndex))
    {
        if (edge.targetIndex == toIndex)
        {
            errorMessage = QStringLiteral("Agent DAG contains duplicate edge: %1 -> %2").arg(
                               normalizedFromNode,
                               normalizedToNode);
            return false;
        }
    }

    _tagAgentDagEdge edge;
    edge.targetIndex = toIndex;
    m_adjacentList[fromIndex].append(edge);
    m_inDegree[toIndex] += 1;

    return true;
}

} // namespace vpet
