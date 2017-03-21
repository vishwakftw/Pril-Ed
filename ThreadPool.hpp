#ifndef PARA_THREAD_POOL_HPP
#define PARA_THREAD_POOL_HPP
#include <functional>
namespace para {
    using task=std::function<void()>;
    void start_threads();
    void schedule(task&&);
    void stop_threads();
}
#endif
