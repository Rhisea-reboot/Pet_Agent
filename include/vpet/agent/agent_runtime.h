#ifndef VPET_AGENT_AGENT_RUNTIME_H
#define VPET_AGENT_AGENT_RUNTIME_H

#include "vpet/agent/agent_context.h"
#include "vpet/agent/agent_dag_graph.h"
#include "vpet/agent/invocation_queue_policy.h"
#include "vpet/agent/agent_output_policy.h"
#include "vpet/llm/vision_llm_client.h"

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QSize>
#include <QString>
#include <QVector>

#include <functional>

namespace vpet
{

class LlmClient;

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
     * @brief 按在线就绪队列执行 Agent 节点
     * @param[out] errorMessage 错误描述
     * @return 执行成功返回 true
     */
    bool Execute(QString &errorMessage);

    /**
     * @brief 写入用户输入并按在线就绪队列执行 Agent 节点
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
     * @brief 设置当前视觉 LLM 模型档位
     * @param[in] profile 目标模型档位
     * @return 切换成功返回 true
     */
    bool SetActiveVisionLlmProfile(VISION_LLM_MODEL_PROFILE profile);

    /**
     * @brief 获取当前视觉 LLM 模型档位
     * @return 当前模型档位
     */
    VISION_LLM_MODEL_PROFILE GetActiveVisionLlmProfile() const;

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
     * @brief Agent 最终输出就绪信号
     * @param[in] requestId 触发最终输出的请求 ID
     * @param[in] content 经过拓扑链路处理后的最终输出文本
     * @param[in] source 输出来源，允许值为 user_response 或 vision_proactive
     */
    void AgentOutputReady(int requestId, const QString &content, const QString &source);

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
     * @brief 初始化当前一轮在线调度状态
     * @param[out] errorMessage 错误描述
     * @return 初始化成功返回 true
     */
    bool BeginInvocation(QString &errorMessage);

    /**
     * @brief 将新的触发上下文加入等待队列
     * @param[in] context 新 invocation 的输入快照
     * @return 入队成功返回 true
     */
    bool EnqueueInvocation(const AgentContext &context);

    /**
     * @brief 启动等待队列中的下一轮 invocation
     * @param[out] errorMessage 错误描述
     * @return 启动成功或队列为空返回 true
     */
    bool StartNextQueuedInvocation(QString &errorMessage);

    /**
     * @brief 推进当前一轮的所有已就绪节点
     * @param[in] shouldPrepareInput 是否准备本轮输入上下文
     * @param[out] errorMessage 错误描述
     * @return 推进成功返回 true；遇到异步节点时安全暂停并返回 true
     */
    bool PumpReadyQueue(bool shouldPrepareInput, QString &errorMessage);

    /**
     * @brief 标记节点完成并降低所有直接后继的剩余入度
     * @param[in] nodeId 已完成节点标识
     * @param[out] errorMessage 错误描述
     * @return 更新成功返回 true
     */
    bool CompleteNode(const QString &nodeId, QString &errorMessage);

    /**
     * @brief 按节点声明顺序插入一个就绪节点
     * @param[in] nodeId 就绪节点标识
     * @param[out] errorMessage 错误描述
     * @return 插入成功返回 true
     */
    bool EnqueueReadyNode(const QString &nodeId, QString &errorMessage);

    /**
     * @brief 为节点构建只读基座与分支本地数据叠加的执行视图
     * @param[in] nodeId 节点标识
     * @param[out] context 节点可读写执行视图
     * @param[out] errorMessage 错误描述
     * @return 构建成功返回 true
     */
    bool BuildExecutionView(const QString &nodeId,
                            AgentContext &context,
                            QString &errorMessage) const;

    /**
     * @brief 将节点执行视图相对执行前快照的增量写回所属分支
     * @param[in] nodeId 节点标识
     * @param[in] afterContext 节点执行后视图
     * @param[out] errorMessage 错误描述
     * @return 保存成功返回 true
    */
    bool SaveNodeResult(const QString &nodeId,
                        const AgentContext &afterContext,
                        QString &errorMessage);

    /**
     * @brief 为一个单父后继创建继承父分支数据的新分支
     * @param[in] parentNodeId 已完成父节点标识
     * @param[in] childNodeId 即将就绪的子节点标识
     * @param[out] errorMessage 错误描述
     * @return 创建成功返回 true
     */
    bool CreateChildBranch(const QString &parentNodeId,
                           const QString &childNodeId,
                           QString &errorMessage);

    /**
     * @brief 合并所有直接父节点结果并创建 fan-in 节点分支
     * @param[in] childNodeId 即将就绪的 fan-in 节点标识
     * @param[out] errorMessage 错误描述
     * @return 合并成功返回 true
     */
    bool CreateJoinBranch(const QString &childNodeId, QString &errorMessage);

    /**
     * @brief 将一个上下文键按 join 规则合并到目标分支
     * @param[in] joinNodeId join 节点标识
     * @param[in] key 待合并上下文键
     * @param[in] predecessors 直接父节点标识
     * @param[in] mergeRules 按键配置的合并规则
     * @param[in,out] local join 分支本地上下文
     * @param[in,out] removedKeys join 分支删除键集合
     * @param[out] errorMessage 错误描述
     * @return 合并成功返回 true
     */
    bool MergeJoinKey(const QString &joinNodeId,
                      const QString &key,
                      const QVector<QString> &predecessors,
                      const QJsonObject &mergeRules,
                      AgentContext &local,
                      QSet<QString> &removedKeys,
                      QString &errorMessage);

    /**
     * @brief 将成功调用的持久化结果提交到会话基座
     * @param[out] errorMessage 错误描述
     * @return 提交成功返回 true
     */
    bool CommitInvocationResult(QString &errorMessage);

    /**
     * @brief 登记一个等待异步回调的节点
     * @param[in] node 等待回调的节点定义
     * @param[in] context 节点挂起时的独立上下文
     * @param[out] errorMessage 错误描述
     * @return 登记成功返回 true
     */
    bool RegisterPendingNode(const _tagAgentDagNode &node,
                             const AgentContext &context,
                             QString &errorMessage);

    /**
     * @brief 根据异步客户端来源和请求 ID 构造 pending 表键
     * @param[in] clientType 异步客户端类型
     * @param[in] requestId 客户端请求 ID
     * @return pending 表键
     */
    QString BuildPendingRequestKey(const QString &clientType, int requestId) const;

    /**
     * @brief 恢复指定异步节点并继续在线调度
     * @param[in] requestId 异步请求标识
     * @param[in] context 回调结果写入后的节点上下文
     * @param[out] errorMessage 错误描述
     * @return 恢复成功返回 true
     */
    bool ResumePendingNode(const QString &pendingKey,
                           int requestId,
                           const AgentContext &context,
                           QString &errorMessage);

    /**
     * @brief 处理异步请求超时并终止所属执行轮次
     * @param[in] requestId 异步请求标识
     * @param[in] invocationId 请求登记时的执行轮次标识
     */
    void HandlePendingRequestTimeout(const QString &pendingKey,
                                     int requestId,
                                     quint64 invocationId);

    /**
     * @brief 清理当前一轮在线调度状态
     */
    void ClearInvocationState();

    /**
     * @brief 准备基础文本输入上下文
     * @param[in,out] context 运行时上下文
     * @param[out] errorMessage 错误描述
     * @return 准备成功返回 true
     */
    bool PrepareTextInputContext(AgentContext &context, QString &errorMessage);

    /**
     * @brief 清理当前一轮执行的输入和触发来源
     * @param[in,out] context 运行时上下文
     */
    void ClearInvocationInputState(AgentContext &context);

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
     * @brief 根据触发类型解析并写入最终输出来源
     * @param[in,out] context 运行时上下文
     * @param[out] errorMessage 错误描述
     * @return 写入成功返回 true
     */
    bool WriteOutputSource(AgentContext &context, QString &errorMessage);

    /**
     * @brief 读取最终输出来源
     * @param[in] context 运行时上下文
     * @return 输出来源字符串；缺失时返回 user_response
     */
    QString ReadOutputSource(const AgentContext &context) const;

    /**
     * @brief 发射最终输出就绪信号
     * @param[in] requestId 触发最终输出的请求 ID
     * @param[in] content 最终输出文本
     */
    void EmitAgentOutputReady(int requestId, const QString &content);

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
     * @brief 统一解除异步执行阻塞并清理本轮输入
     * @param[in,out] context 运行时上下文
     */
    void ResetAsyncExecutionState(AgentContext &context);

    /**
     * @brief 设置运行时异步等待状态
     * @param[in] node 等待回调的节点定义
     * @param[in,out] context 运行时上下文
     * @param[in] requestId 等待回调的请求 ID
     * @param[out] errorMessage 错误描述
     * @return 设置成功返回 true
     */
    bool SetAsyncPendingState(const _tagAgentDagNode &node,
                              AgentContext &context,
                              int requestId,
                              QString &errorMessage);

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
    /**
     * @brief 单轮在线调度状态
     */
    struct _tagInvocationState
    {
        struct _tagBranchState
        {
            QString branchId;                         ///< 分支唯一标识
            QString sourceNodeId;                     ///< 分支起始源节点标识
            QString sourceTrigger;                    ///< 源节点声明的触发来源
            AgentContext local;                       ///< 本分支可写本地数据
            QSet<QString> removedKeys;                ///< 本地视图相对基座删除的键
        };

        struct _tagNodeResult
        {
            QString branchId;                         ///< 产生结果的分支标识
            QString sourceNodeId;                     ///< 结果所属分支的起始源节点
            QString sourceTrigger;                    ///< 结果所属源节点的触发来源
            AgentContext local;                       ///< 节点完成后的本地数据快照
            QSet<QString> removedKeys;                ///< 节点完成后的删除键集合
        };

        struct _tagPendingRequest
        {
            int requestId = -1;                       ///< 外部异步请求标识
            QString clientType;                       ///< 异步客户端类型
            quint64 invocationId = 0;                 ///< 请求所属执行轮次
            QString nodeId;                           ///< 挂起节点标识
            QString branchId;                         ///< 挂起节点所属分支
            QString nodeType;                         ///< 挂起节点类型
            AgentContext context;                     ///< 节点挂起时的独立上下文
        };

        quint64 invocationId = 0;                     ///< 当前执行轮次唯一标识
        bool isActive = false;                        ///< 当前是否存在运行中的执行轮次
        bool hasFailed = false;                       ///< 当前执行轮次是否已失败
        QString trigger;                              ///< 当前执行轮次触发来源
        QString failureMessage;                       ///< 当前执行轮次失败原因
        QHash<QString, int> remainingInDegree;        ///< 节点尚未完成的前驱数量
        QHash<QString, int> nodeDeclarationOrder;     ///< 节点标识到声明顺序的映射
        QVector<QString> readyQueue;                  ///< 按节点声明顺序排序的就绪节点
        QSet<QString> completedNodeIds;               ///< 已成功完成的节点标识
        QHash<QString, bool> nodeExecutionResults;    ///< 节点执行成功状态
        QHash<QString, _tagBranchState> branches;     ///< 分支标识到本地状态的映射
        QHash<QString, QString> nodeBranchIds;        ///< 节点标识到所属分支的映射
        QHash<QString, _tagNodeResult> nodeResults;   ///< 已完成节点的输出结果
        QHash<QString, _tagPendingRequest> pendingByRequestId; ///< 客户端和请求标识到挂起节点的映射
        QSet<QString> activeNodeIds;                  ///< 当前 trigger 裁剪后的节点集合
    };

    AgentDagGraph m_dagGraph;             ///< Agent DAG 图结构
    AgentContext m_context;               ///< 当前执行视图或最近一次结果上下文
    AgentContext m_sessionContext;        ///< 跨调用持久化的会话基座
    LlmClient *m_llmClient;                ///< 文本 LLM 客户端
    VisionLlmClient *m_visionLlmClient;    ///< 视觉 LLM 客户端
    QHash<QString, NodeHandler> m_nodeHandlers; ///< 节点类型到处理器的映射
    QVector<QString> m_executionOrder;    ///< 拓扑执行顺序
    _tagInvocationState m_invocationState; ///< 当前一轮在线调度状态
    InvocationQueuePolicy m_invocationQueue; ///< 跨轮触发排队策略
    quint64 m_nextInvocationId;            ///< 下一轮执行的唯一标识
    QSet<int> m_directRequestIds;          ///< 未通过 DAG 发起的直接 LLM 请求
    QString m_lastPerceptionFrameHash;     ///< 最近已接受视觉帧内容指纹
    bool m_isLoaded;                      ///< 是否已加载配置
    bool m_contextWasQueued;              ///< 最近一次上下文是否已由入口加入 FIFO
};

} // namespace vpet

#endif // VPET_AGENT_AGENT_RUNTIME_H
