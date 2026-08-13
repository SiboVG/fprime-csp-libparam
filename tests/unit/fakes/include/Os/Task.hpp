#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <thread>

namespace Os {

class TaskString final {
  public:
    explicit TaskString(const char* value) : m_value(value == nullptr ? "" : value) {}
    const char* toChar() const { return this->m_value.c_str(); }

  private:
    std::string m_value;
};

namespace Test {
inline int taskStartStatus = 0;
inline int taskJoinStatus = 0;
inline std::size_t taskStartCalls = 0;
inline std::size_t taskJoinCalls = 0;
inline std::string lastTaskName;

inline void resetTaskFake() {
    taskStartStatus = 0;
    taskJoinStatus = 0;
    taskStartCalls = 0;
    taskJoinCalls = 0;
    lastTaskName.clear();
}
}  // namespace Test

class Task final {
  public:
    using taskRoutine = void (*)(void*);
    using ParamType = std::size_t;

    static constexpr ParamType TASK_DEFAULT = std::numeric_limits<ParamType>::max();
    static constexpr std::uint32_t TASK_PRIORITY_DEFAULT = std::numeric_limits<std::uint32_t>::max();

    enum Status {
        OP_OK,
        INVALID_HANDLE,
        INVALID_PARAMS,
        INVALID_PRIORITY,
        INVALID_STACK,
        UNKNOWN_ERROR,
        INVALID_AFFINITY,
        DELAY_ERROR,
        JOIN_ERROR,
        ERROR_RESOURCES,
        ERROR_PERMISSION,
        NOT_SUPPORTED,
        INVALID_STATE,
    };

    class Arguments final {
      public:
        Arguments(const TaskString& name,
                  taskRoutine routine,
                  void* routineArgument,
                  std::uint32_t priority,
                  ParamType stackSize,
                  ParamType cpuAffinity)
            : m_name(name),
              m_routine(routine),
              m_routineArgument(routineArgument),
              m_priority(priority),
              m_stackSize(stackSize),
              m_cpuAffinity(cpuAffinity) {}

        TaskString m_name;
        taskRoutine m_routine;
        void* m_routineArgument;
        std::uint32_t m_priority;
        ParamType m_stackSize;
        ParamType m_cpuAffinity;
    };

    Task() = default;
    ~Task() {
        if (this->m_thread.joinable()) {
            this->m_thread.join();
        }
    }

    Status start(const Arguments& arguments) {
        ++Test::taskStartCalls;
        Test::lastTaskName = arguments.m_name.toChar();
        const Status status = static_cast<Status>(Test::taskStartStatus);
        if (status != OP_OK) {
            return status;
        }
        this->m_thread = std::thread(arguments.m_routine, arguments.m_routineArgument);
        return OP_OK;
    }

    Status join() {
        ++Test::taskJoinCalls;
        if (this->m_thread.joinable()) {
            this->m_thread.join();
        }
        return static_cast<Status>(Test::taskJoinStatus);
    }

  private:
    std::thread m_thread;
};

}  // namespace Os
