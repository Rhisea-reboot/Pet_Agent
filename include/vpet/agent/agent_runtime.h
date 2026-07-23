#ifndef VPET_AGENT_AGENT_RUNTIME_H
#define VPET_AGENT_AGENT_RUNTIME_H

#include "vpet/agent/agent_context.h"
#include "vpet/agent/agent_dag_graph.h"

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QSize>
#include <QString>
#include <QVector>

#include <functional>

namespace vpet
{

class LlmClient;
class VisionLlmClient;

/**
 * @brief Agent DAG 运行时启动器
 *
 * 负责加载 Agent DAG 配置、输出拓扑序，并按节点类型执行当前可用节点。
 */
class AgentRuntime
    : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Agent 节点处理器类型
     */
    using NodeHandler = std::function<bool(const _tagAgentDagNode &, AgentContext &, QString &)>;

    /**
     * @brief 构造函数
     * @param[in] parent 父对象
     */
    explicit AgentRuntime(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~AgentRuntime() override;

    /**
     * @brief 加载 Agent DAG 配置并准备运行时
     * @param[in] configPath Agent DAG 配置文件路径
     * @param[out] errorMessage 错误描述
     * @return 加载成功返回 true
     */
    bool Load(const QString &configPath, QString &errorMessage);

    /**
     * @brief 按拓扑序执行 Agent 节点
     * @param[out] errorMessage 错误描述
     * @return 执行成功返回 true
     */
    bool Execute(QString &errorMessage);

    /**
     * @brief 写入用户输入并按拓扑序执行 Agent 节点
     * @param[in] userInput 用户输入文本
     * @param[out] errorMessage 错误描述
     * @return 执行成功返回 true
     */
    bool ExecuteWithUserInput(const QString &userInput, QString &errorMessage);

    /**
     * @brief 写入最新视觉感知帧到运行时上下文
     * @param[in] encodedData 编码后的图像数据
     * @param[in] frameId 帧序号
     * @param[in] frameSize 帧尺寸
     * @param[in] modality 模态名称
     * @param[out] errorMessage 错误描述
     * @return 写入成功返回 true
     */
    bool UpdatePerceptionFrame(const QByteArray &encodedData,
                               int frameId,
                               const QSize &frameSize,
                               const QString &modality,
                               QString &errorMessage);

    /**
     * @brief 从指定文件加载文本 LLM 配置
     * @param[in] configPath LLM 配置文件路径
     * @param[out] errorMessage 错误描述
     * @return 加载成功返回 true
     */
    bool LoadLlmConfig(const QString &configPath, QString &errorMessage);

    /**
     * @brief 自动查找并加载文本 LLM 配置
     * @param[out] errorMessage 错误描述
     * @return 加载成功返回 true
     */
    bool LoadDefaultLlmConfig(QString &errorMessage);

    /**
     * @brief 从指定文件加载视觉 LLM 配置
     * @param[in] configPath 视觉 LLM 配置文件路径
     * @param[out] errorMessage 错误描述
     * @return 加载成功返回 true
     */
    bool LoadVisionLlmConfig(const QString &configPath, QString &errorMessage);

    /**
     * @brief 自动查找并加载视觉 LLM 配置
     * @param[out] errorMessage 错误描述
     * @return 加载成功返回 true
     */
    bool LoadDefaultVisionLlmConfig(QString &errorMessage);

    /**
     * @brief 判断文本 LLM 是否可用
     * @return 可用返回 true
     */
    bool IsLlmConfigured() const;

    /**
     * @brief 判断视觉 LLM 是否可用
     * @return 可用返回 true
     */
    bool IsVisionLlmConfigured() const;

    /**
     * @brief 判断运行时是否有异步请求等待回调
     * @return 有等待中的异步请求返回 true
     */
    bool HasPendingAsyncRequest() const;

    /**
     * @brief 加载配置并立即执行 Agent DAG
     * @param[in] configPath Agent DAG 配置文件路径
     * @param[out] errorMessage 错误描述
     * @return 启动并执行成功返回 true
     */
    bool Start(const QString &configPath, QString &errorMessage);

    /**
     * @brief 获取当前拓扑执行顺序
     * @return 节点名称列表
     */
    QVector<QString> GetExecutionOrder() const;

    /**
     * @brief 获取运行时上下文
     * @return 运行时上下文引用
     */
    AgentContext &GetContext();

    /**
     * @brief 获取只读运行时上下文
     * @return 运行时上下文只读引用
     */
    const AgentContext &GetContext() const;

    /**
     * @brief 设置运行时上下文
     * @param[in] context 外部上下文对象
     */
    void SetContext(const AgentContext &context);

    /**
     * @brief 注册节点处理器
     * @param[in] nodeType 节点类型
     * @param[in] handler 节点处理器
     * @return 注册成功返回 true
     */
    bool RegisterNodeHandler(const QString &nodeType, const NodeHandler &handler);

signals:
    /**
     * @brief Agent 日志信号
     * @param[in] message 日志内容
     */
    void LogMessage(const QString &message);

    /**
     * @brief LLM 回复完成信号
     * @param[in] requestId 请求 ID
     * @param[in] content 回复文本
     */
    void LlmResponseReceived(int requestId, const QString &content);

    /**
     * @brief LLM 请求失败信号
     * @param[in] requestId 请求 ID
     * @param[in] message 错误描述
     * @param[in] statusCode HTTP 状态码
     */
    void LlmRequestFailed(int requestId, const QString &message, int statusCode);

private slots:
    /**
     * @brief 处理文本 LLM 回复完成
     * @param[in] requestId 请求 ID
     * @param[in] content 回复文本
     */
    void OnLlmChatCompleted(int requestId, const QString &content);

    /**
     * @brief 处理文本 LLM 请求失败
     * @param[in] requestId 请求 ID
     * @param[in] message 错误描述
     * @param[in] statusCode HTTP 状态码
     */
    void OnLlmChatFailed(int requestId, const QString &message, int statusCode);

    /**
     * @brief 处理视觉 LLM 识别完成
     * @param[in] requestId 请求 ID
     * @param[in] content 视觉识别文本
     */
    void OnVisionAnalysisCompleted(int requestId, const QString &content);

    /**
     * @brief 处理视觉 LLM 识别失败
     * @param[in] requestId 请求 ID
     * @param[in] message 错误描述
     * @param[in] statusCode HTTP 状态码
     */
    void OnVisionAnalysisFailed(int requestId, const QString &message, int statusCode);

private:
    /**
     * @brief 执行单个 Agent 节点
     * @param[in] node 节点定义
     * @param[in,out] context 运行时上下文
     * @param[out] errorMessage 错误描述
     * @return 执行成功返回 true
     */
    bool ExecuteNode(const _tagAgentDagNode &node,
                     AgentContext &context,
                     QString &errorMessage);

    /**
     * @brief 执行节点前补齐语义别名输入
     * @param[in] node 节点定义
     * @param[in,out] context 运行时上下文
     * @return 处理成功返回 true
     */
    bool PrepareNodeInputAliases(const _tagAgentDagNode &node, AgentContext &context);

    /**
     * @brief 执行节点后同步语义别名输出
     * @param[in] node 节点定义
     * @param[in,out] context 运行时上下文
     * @return 处理成功返回 true
     */
    bool SyncNodeOutputAliases(const _tagAgentDagNode &node, AgentContext &context);

    /**
     * @brief 从指定拓扑序位置继续执行 Agent 节点
     * @param[in] startIndex 起始执行下标
     * @param[in] shouldPrepareInput 是否准备本轮输入上下文
     * @param[out] errorMessage 错误描述
     * @return 执行成功返回 true
     */
    bool ExecuteFromIndex(int startIndex,
                          bool shouldPrepareInput,
                          QString &errorMessage);

    /**
     * @brief 准备基础文本输入上下文
     * @param[in,out] context 运行时上下文
     * @param[out] errorMessage 错误描述
     * @return 准备成功返回 true
     */
    bool PrepareTextInputContext(AgentContext &context, QString &errorMessage);

    /**
     * @brief 执行视觉输入节点
     * @param[in] node 节点定义
     * @param[in,out] context 运行时上下文
     * @param[out] errorMessage 错误描述
     * @return 执行成功返回 true
     */
    bool ExecuteVisionInputNode(const _tagAgentDagNode &node,
                                AgentContext &context,
                                QString &errorMessage);

    /**
     * @brief 执行视觉 LLM 节点
     * @param[in] node 节点定义
     * @param[in,out] context 运行时上下文
     * @param[out] errorMessage 错误描述
     * @return 执行成功返回 true
     */
    bool ExecuteVisionLlmNode(const _tagAgentDagNode &node,
                              AgentContext &context,
                              QString &errorMessage);

    /**
     * @brief 执行文本 LLM 节点
     * @param[in] node 节点定义
     * @param[in,out] context 运行时上下文
     * @param[out] errorMessage 错误描述
     * @return 执行成功返回 true
     */
    bool ExecuteLlmChatNode(const _tagAgentDagNode &node,
                            AgentContext &context,
                            QString &errorMessage);

    /**
     * @brief 执行输出格式化节点
     * @param[in] node 节点定义
     * @param[in,out] context 运行时上下文
     * @param[out] errorMessage 错误描述
     * @return 执行成功返回 true
     */
    bool ExecuteOutputFormatNode(const _tagAgentDagNode &node,
                                  AgentContext &context,
                                  QString &errorMessage);

    /**
     * @brief 记录用户输入和最终输出到最近对话历史
     * @param[in,out] context 运行时上下文
     * @param[in] outputText 最终输出文本
     * @param[out] errorMessage 错误描述
     * @return 记录成功返回 true
     */
    bool AppendConversationHistory(AgentContext &context,
                                   const QString &outputText,
                                   QString &errorMessage);

    /**
     * @brief 清理运行时异步等待状态
     * @param[in,out] context 运行时上下文
     */
    void ClearAsyncPendingState(AgentContext &context);

    /**
     * @brief 执行尚未实现的透传节点
     * @param[in] node 节点定义
     * @param[in,out] context 运行时上下文
     * @param[out] errorMessage 错误描述
     * @return 执行成功返回 true
     */
    bool ExecutePassThroughNode(const _tagAgentDagNode &node,
                                AgentContext &context,
                                QString &errorMessage);

    /**
     * @brief 注册默认节点处理器
     */
    void RegisterDefaultNodeHandlers();

    /**
     * @brief 查找默认文本 LLM 配置文件
     * @return 配置文件绝对路径；未找到返回空字符串
     */
    QString FindDefaultLlmConfigPath() const;

    /**
     * @brief 查找默认视觉 LLM 配置文件
     * @return 配置文件绝对路径；未找到返回空字符串
     */
    QString FindDefaultVisionLlmConfigPath() const;

    /**
     * @brief 将用户输入提交到文本 LLM
     * @param[in] userInput 用户输入文本
     * @param[out] errorMessage 错误描述
     * @return 发送成功返回 true
     */
    bool SendUserInputToLlm(const QString &userInput, QString &errorMessage);

private:
    AgentDagGraph m_dagGraph;             ///< Agent DAG 图结构
    AgentContext m_context;               ///< Agent 运行时上下文
    LlmClient *m_llmClient;                ///< 文本 LLM 客户端
    VisionLlmClient *m_visionLlmClient;    ///< 视觉 LLM 客户端
    QHash<QString, NodeHandler> m_nodeHandlers; ///< 节点类型到处理器的映射
    QVector<QString> m_executionOrder;    ///< 拓扑执行顺序
    int m_pendingResumeIndex;             ///< 异步 LLM 返回后的续跑下标
    QString m_pendingNodeType;            ///< 当前等待回调的节点类型
    int m_pendingRequestId;               ///< 当前等待回调的请求 ID
    bool m_isLoaded;                      ///< 是否已加载配置
};

} // namespace vpet

#endif // VPET_AGENT_AGENT_RUNTIME_H
