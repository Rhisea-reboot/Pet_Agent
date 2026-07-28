#ifndef VPET_AGENT_AGENT_RUNTIME_H
#define VPET_AGENT_AGENT_RUNTIME_H

#include "vpet/agent/agent_context.h"
#include "vpet/agent/agent_async_bridge.h"
#include "vpet/agent/agent_graph_executor.h"
#include "vpet/agent/agent_node_registry.h"
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
     * @brief 登记一个等待异步回调的节点
     * @param[in] node 等待回调的节点定义
     * @param[in] context 节点挂起时的独立上下文
     * @param[in] invocationId 节点所属执行轮次标识
     * @param[in] branchId 节点所属图分支标识
     * @param[out] errorMessage 错误描述
     * @return 登记成功返回 true
     */
    bool RegisterPendingNode(const _tagAgentDagNode &node,
                             const AgentContext &context,
                             quint64 invocationId,
                             const QString &branchId,
                             QString &errorMessage);

    /**
     * @brief 构建图执行器回调集合
     * @return 不持有运行时反向引用的单次调用回调集合
     */
    AgentGraphExecutor::_tagCallbacks BuildGraphCallbacks();

    /**
     * @brief 根据异步客户端来源和请求 ID 构造 pending 表键
     * @param[in] clientType 异步客户端类型
     * @param[in] requestId 客户端请求 ID
     * @return pending 表键
     */
    QString BuildPendingRequestKey(const QString &clientType, int requestId) const;

    /**
     * @brief 恢复指定异步节点并继续在线调度
     * @param[in] pendingKey 异步请求关联键
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
     * @param[in] pendingKey 异步请求关联键
     * @param[in] requestId 异步请求标识
     * @param[in] invocationId 请求登记时的执行轮次标识
     */
    void HandlePendingRequestTimeout(const QString &pendingKey,
                                     int requestId,
                                     quint64 invocationId);

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
     * @param[in] context 完成当前输出的运行时上下文
     */
    void EmitAgentOutputReady(int requestId,
                              const QString &content,
                              const AgentContext &context);

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
    AgentContext m_context;               ///< 当前执行视图或最近一次结果上下文
    AgentContext m_sessionContext;        ///< 跨调用持久化的会话基座
    LlmClient *m_llmClient;                ///< 文本 LLM 客户端
    VisionLlmClient *m_visionLlmClient;    ///< 视觉 LLM 客户端
    AgentNodeRegistry m_nodeRegistry;     ///< 节点注册与别名执行组件
    AgentGraphExecutor m_graphExecutor;   ///< DAG 与单轮调度组件
    AgentAsyncBridge m_asyncBridge;       ///< 异步请求关联组件
    InvocationQueuePolicy m_invocationQueue; ///< 跨轮触发排队策略
    QString m_lastPerceptionFrameHash;     ///< 最近已接受视觉帧内容指纹
    bool m_isLoaded;                      ///< 是否已加载配置
    bool m_contextWasQueued;              ///< 最近一次上下文是否已由入口加入 FIFO
};

} // namespace vpet

#endif // VPET_AGENT_AGENT_RUNTIME_H
