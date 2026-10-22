#pragma once

#include <QObject>
#include <QString>
#include <QFuture>
#include <QtConcurrent>
#include <QMutex>
#include <QDateTime>
#include <functional>
#include <memory>
#include <atomic>
#include <map>

namespace UdooKey {

enum class TaskState {
    Pending,
    Running,
    Finished,
    Failed,
    Cancelled
};

class CancellationToken {
public:
    CancellationToken() : m_cancelled(false) {}
    void cancel() { m_cancelled.store(true, std::memory_order_relaxed); }
    bool isCancelled() const { return m_cancelled.load(std::memory_order_relaxed); }
    void reset() { m_cancelled.store(false, std::memory_order_relaxed); }
private:
    std::atomic<bool> m_cancelled;
};

struct TaskInfo {
    uint64_t id = 0;
    QString name;
    QString description;
    TaskState state = TaskState::Pending;
    int progressPercent = 0;
    QString statusMessage;
    QDateTime startTime;
    QDateTime finishTime;
    std::shared_ptr<CancellationToken> token;
};

class TaskManager : public QObject {
    Q_OBJECT

public:
    using TaskFunc = std::function<bool(std::shared_ptr<CancellationToken>, std::function<void(int, const QString&)>)>;

    static TaskManager& instance();

    uint64_t submitTask(const QString& name, const QString& description, TaskFunc work);
    bool cancelTask(uint64_t taskId);
    void cancelAll();

    bool isTaskRunning(uint64_t taskId) const;
    TaskInfo getTaskInfo(uint64_t taskId) const;
    QList<TaskInfo> allTasks() const;

signals:
    void taskStarted(uint64_t taskId, const QString& name);
    void taskProgress(uint64_t taskId, int percent, const QString& message);
    void taskFinished(uint64_t taskId, bool success, const QString& message);
    void taskCancelled(uint64_t taskId);

private:
    TaskManager();
    ~TaskManager() override;
    TaskManager(const TaskManager&) = delete;
    TaskManager& operator=(const TaskManager&) = delete;

    mutable QMutex m_mutex;
    uint64_t m_nextId = 1;
    std::map<uint64_t, TaskInfo> m_tasks;
};

} // namespace UdooKey
