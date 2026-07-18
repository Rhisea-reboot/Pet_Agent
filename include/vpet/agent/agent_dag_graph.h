#ifndef VPET_AGENT_AGENT_DAG_GRAPH_H
#define VPET_AGENT_AGENT_DAG_GRAPH_H

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVector>

namespace vpet
{

/**
 * @brief Agent DAG 边定义
 */
struct _tagAgentDagEdge
{
    int targetIndex = -1; ///< 目标节点索引
};

/**
 * @brief Agent DAG 图加载与拓扑排序器
 *
 * 仅负责节点与边的结构解析、校验、环检测和拓扑排序，不承载节点执行逻辑。
 */
class AgentDagGraph
{
public:
    /**
     * @brief 构造函数
     */
    AgentDagGraph();

    /**
     * @brief 从 JSON 文件加载 DAG 结构
     * @param[in] configPath DAG 配置文件路径
     * @param[out] errorMessage 错误描述
     * @return 加载成功返回 true
     */
    bool LoadFromJsonFile(const QString &configPath, QString &errorMessage);

    /**
     * @brief 从 JSON 字节加载 DAG 结构
     * @param[in] jsonData DAG 配置 JSON 字节
     * @param[out] errorMessage 错误描述
     * @return 加载成功返回 true
     */
    bool LoadFromJsonData(const QByteArray &jsonData, QString &errorMessage);

    /**
     * @brief 执行拓扑排序
     * @param[out] order 输出拓扑序节点名称
     * @param[out] errorMessage 错误描述
     * @return 排序成功返回 true；存在环时返回 false
     */
    bool TopologicalSort(QVector<QString> &order, QString &errorMessage) const;

    /**
     * @brief 判断图是否为空
     * @return 空图返回 true
     */
    bool IsEmpty() const;

    /**
     * @brief 获取节点数量
     * @return 节点数量
     */
    int GetNodeCount() const;

    /**
     * @brief 获取节点名称列表
     * @return 节点名称列表
     */
    QVector<QString> GetNodeNames() const;

private:
    /**
     * @brief 清空图结构
     */
    void Clear();

    /**
     * @brief 初始化指定节点数量的存储结构
     * @param[in] nodeCount 节点数量
     */
    void Reset(int nodeCount);

    /**
     * @brief 添加节点
     * @param[in] nodeName 节点名称
     * @param[out] errorMessage 错误描述
     * @return 添加成功返回 true
     */
    bool AddNode(const QString &nodeName, QString &errorMessage);

    /**
     * @brief 添加边
     * @param[in] fromNode 起始节点名称
     * @param[in] toNode 目标节点名称
     * @param[out] errorMessage 错误描述
     * @return 添加成功返回 true
     */
    bool AddEdge(const QString &fromNode, const QString &toNode, QString &errorMessage);

private:
    QVector<QVector<_tagAgentDagEdge>> m_adjacentList; ///< 邻接表
    QVector<int> m_inDegree;                           ///< 节点入度
    QVector<QString> m_nodeNames;                      ///< 节点名称表
    QHash<QString, int> m_nodeIndexMap;                ///< 节点名称到索引的映射
};

} // namespace vpet

#endif // VPET_AGENT_AGENT_DAG_GRAPH_H
