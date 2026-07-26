#include "vpet/agent/invocation_queue_policy.h"
#include "vpet/agent/agent_context_keys.h"

namespace vpet
{

bool InvocationQueuePolicy::Enqueue(const AgentContext &context,
                                    const AgentContext &sessionContext)
{
    _tagEntry entry;
    QVariant triggerValue;

    if (context.GetValue(AgentContextKeys::RUNTIME_TRIGGER_TYPE, triggerValue))
    {
        entry.trigger = triggerValue.toString().trimmed();
    }

    if (!context.BuildDelta(sessionContext, entry.local, entry.removedKeys))
    {
        return false;
    }

    if (entry.trigger == QStringLiteral("vision"))
    {
        for (int index = m_entries.size() - 1; index >= 0; --index)
        {
            if (m_entries.at(index).trigger == QStringLiteral("vision"))
            {
                m_entries[index] = entry;
                return true;
            }
        }
    }

    m_entries.enqueue(entry);
    return true;
}

bool InvocationQueuePolicy::Dequeue(_tagEntry &entry)
{
    if (m_entries.isEmpty())
    {
        return false;
    }

    entry = m_entries.dequeue();
    return true;
}

bool InvocationQueuePolicy::IsEmpty() const
{
    return m_entries.isEmpty();
}

} // namespace vpet
