#include <experimental/coroutine>
#include "./../third_party/silk/silk_pool.h"

namespace slev {
    template<typename T> struct task_promise;
    template<typename T> struct task;
    struct independed_task;
    
    enum task_state { unspawned, awaitable, completed };
    
    struct task_promise_base {
    	std::atomic<task_state> state = task_state::unspawned;
    
    	std::experimental::coroutine_handle<> continuation;
    };
    
    struct frame : public silk::task {
    	std::experimental::coroutine_handle<> coro;
    
    	frame(std::experimental::coroutine_handle<> c) : coro(c) {}
    };
    
    void spawn(std::experimental::coroutine_handle<> coro) {
    	silk::spawn(silk::current_worker_id, (silk::task*) new frame(coro));
    }
    
    struct final_awaitable {
    	bool await_ready() const noexcept { return false; }
    
    	template<typename T> void await_suspend(std::experimental::coroutine_handle<T> coro) {
    		task_promise_base& p = coro.promise();
    
    		if (p.state.exchange(task_state::completed, std::memory_order_release) == task_state::awaitable) {
    			spawn(p.continuation);
    		}
    	}
    
    	void await_resume() noexcept {}
    };
    
    template<typename T = void> struct task_awaitable {
    	task<T>& awaitable;
    
    	bool await_ready() noexcept { return false; }
    
    	void await_suspend(std::experimental::coroutine_handle<> coro) noexcept {
    		task_promise_base& p = awaitable.coro.promise();
    
    		p.continuation = coro;
    
    		task_state s = task_state::unspawned;
    		if (!p.state.compare_exchange_strong(s, task_state::awaitable, std::memory_order_release)) {
    			spawn(coro);
    		}
    	}
    
    	auto await_resume() noexcept { return awaitable.result(); }
    };
    
    template<typename T> struct task_promise : public task_promise_base {
    	std::exception_ptr e_;
    	T v_;
    
    	task<T> get_return_object() noexcept;
    
    	auto initial_suspend() { return std::experimental::suspend_never{}; }
    
    	auto final_suspend() { return final_awaitable{}; }
    
    	void unhandled_exception() { e_ = std::current_exception(); }
    
    	void return_value(const T v) { v_ = v; }
    
    	T result() {
    		if (e_) {
    			std::rethrow_exception(e_);
    		}
    
    		return v_;
    	}
    };
    
    template<> struct task_promise<void> : public task_promise_base {
    	task_promise() noexcept = default;
    	task<void> get_return_object() noexcept;
    	auto initial_suspend() { return std::experimental::suspend_never{}; }
    
    	auto final_suspend() { return final_awaitable{}; }
    
    	void return_void() noexcept {}
    
    	std::exception_ptr e_;
    	void unhandled_exception() { e_ = std::current_exception(); }
    	void result() {
    		if (e_) {
    			std::rethrow_exception(e_);
    		}
    	}
    };
    
    template<typename T = void> struct task {
    	using promise_type = task_promise<T>;
    
    	std::experimental::coroutine_handle<task_promise<T>> coro;
    
    	task(std::experimental::coroutine_handle<task_promise<T>> c) : coro(c) { }
    
    	~task() {
    		if (coro && coro.done()) {
    			coro.destroy();
    		}
    	}
    
    	const T result() { return coro.promise().result(); }
    
    	task_awaitable<T> operator co_await() { return task_awaitable<T> {*this}; }
    };
    
    template<typename T> task<T> task_promise<T>::get_return_object() noexcept {
    	return task<T> { std::experimental::coroutine_handle<task_promise>::from_promise(*this) };
    }
    
    inline task<void> task_promise<void>::get_return_object() noexcept {
    	return task<void> { std::experimental::coroutine_handle<task_promise>::from_promise(*this) };
    }
    
    struct independed_task_promise {
    	independed_task get_return_object() noexcept;
    	auto initial_suspend() { return std::experimental::suspend_never{}; }
    
    	auto final_suspend() { return std::experimental::suspend_never{}; }
    
    	void return_void() noexcept {}
    
    	std::exception_ptr e_;
    	void unhandled_exception() { e_ = std::current_exception(); }
    	void result() {
    		if (e_) {
    			std::rethrow_exception(e_);
    		}
    	}
    };
    
    struct independed_task {
    	using promise_type = independed_task_promise;
    
    	std::experimental::coroutine_handle<> coro;
    
    	independed_task(std::experimental::coroutine_handle<> c) : coro(c) { }
    };
    
    inline independed_task independed_task_promise::get_return_object() noexcept {
    	return independed_task{ std::experimental::coroutine_handle<independed_task_promise>::from_promise(*this) };
    }
    
    void schedule(silk::task* t) {
    	frame* c = (frame*)t;
    	
    	c->coro.resume();
    
    	delete c;
    }
    
    struct yield_awaitable {
    	bool await_ready() const noexcept { return false; }
    
    	template<typename T> void await_suspend(std::experimental::coroutine_handle<T> c) {
    		spawn(c);
    	}
    
    	void await_resume() noexcept {}
    };
    
    auto yield() {
    	return yield_awaitable{};
    }
}