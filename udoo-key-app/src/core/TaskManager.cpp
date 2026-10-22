#include "TaskManager.h"
#include "Logger.h"

namespace UdooKey {

TaskManager::TaskManager() {
}

TaskManager::~TaskManager() {
    cancelAll();
}

TaskManager& TaskManager::instance() {
    static TaskManager s_instance;
    return s_instance;
}

uint64_t TaskManager::submitTask(const QString& name, const QString& description, TaskFunc work) {
    uint64_t taskId = 0;
    std::shared_ptr<CancellationToken> token = std::make_shared<CancellationToken>();

    {
        QMutexLocker locker(&m_mutex);
        taskId = m_nextId++;
        TaskInfo info;
        info.id = taskId;
        info.name = name;
        info.description = description;
        info.state = TaskState::Pending;
        info.startTime = QDateTime::currentDateTime();
        info.token = token;
        m_tasks[taskId] = info;
    }

    LOG_INFO(QString("Task [%1] '%2' submitted").arg(taskId).arg(name));

    // Run asynchronously via QtConcurrent
    (void)QtConcurrent::run([this, taskId, name, token, work]() {
        {
            QMutexLocker locker(&m_mutex);
            auto it = m_tasks.find(taskId);
            if (it != m_tasks.end()) {
                it->second.state = TaskState::Running;
            }
        }

        emit taskStarted(taskId, name);

        auto progressCallback = [this, taskId](int percent, const QString& message) {
            {
                QMutexLocker locker(&m_mutex);
                auto it = m_tasks.find(taskId);
                if (it != m_tasks.end()) {
                    it->second.progressPercent = percent;
                    it->second.statusMessage = message;
                }
            }
            emit taskProgress(taskId, percent, message);
        };

        bool success = false;
        QString finishMsg;

        try {
            success = work(token, progressCallback);
            finishMsg = success ? "Completed successfully" : "Execution failed";
        } catch (const std::exception& ex) {
            success = false;
            finishMsg = QString("Exception: %1").arg(ex.what());
        } catch (...) {
            success = false;
            finishMsg = "Unknown exception occurred";
        }

        TaskState finalState = TaskState::Finished;
        if (token->isCancelled()) {
            finalState = TaskState::Cancelled;
            finishMsg = "Task cancelled by user";
        } else if (!success) {
            finalState = TaskState::Failed;
        }

        {
            QMutexLocker locker(&m_mutex);
            auto it = m_tasks.find(taskId);
            if (it != m_tasks.end()) {
                it->second.state = finalState;
                it->second.finishTime = QDateTime::currentDateTime();
                it->second.statusMessage = finishMsg;
            }
        }

        if (finalState == TaskState::Cancelled) {
            LOG_INFO(QString("Task [%1] '%2' cancelled").arg(taskId).arg(name));
            emit taskCancelled(taskId);
        } else {
            LOG_INFO(QString("Task [%1] '%2' finished: %3 (success=%4)")
                     .arg(taskId).arg(name).arg(finishMsg).arg(success));
            emit taskFinished(taskId, success, finishMsg);
        }
    });

    return taskId;
}

bool TaskManager::cancelTask(uint64_t taskId) {
    QMutexLocker locker(&m_mutex);
    auto it = m_tasks.find(taskId);
    if (it != m_tasks.end() && it->second.token) {
        it->second.token->cancel();
        return true;
    }
    return false;
}

void TaskManager::cancelAll() {
    QMutexLocker locker(&m_mutex);
    for (auto& pair : m_tasks) {
        if (pair.second.token) {
            pair.second.token->cancel();
        }
    }
}

bool TaskManager::isTaskRunning(uint64_t taskId) const {
    QMutexLocker locker(&m_mutex);
    auto it = m_tasks.find(taskId);
    if (it != m_tasks.end()) {
        return it->second.state == TaskState::Running || it->second.state == TaskState::Pending;
    }
    return false;
}

TaskInfo TaskManager::getTaskInfo(uint64_t taskId) const {
    QMutexLocker locker(&m_mutex);
    auto it = m_tasks.find(taskId);
    if (it != m_tasks.end()) {
        return it->second;
    }
    return TaskInfo{};
}

QList<TaskInfo> TaskManager::allTasks() const {
    QMutexLocker locker(&m_mutex);
    QList<TaskInfo> list;
    for (const auto& pair : m_tasks) {
        list.append(pair.second);
    }
    return list;
}

} // namespace UdooKey
