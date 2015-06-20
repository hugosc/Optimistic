#include <vector>
#include <string>
#include <iostream>
#include <atomic>
#include <random>
#include <functional>
#include <mutex>
#include <chrono>
#include <thread>
#include <iterator>

namespace opt {

	using std::end;
	using std::begin;

	enum action_type {READ, WRITE};
	enum transaction_state {BEGIN, EVAL, STORE, COMMIT};
	typedef unsigned int timestamp;
	class transaction;
	class concurrency_controller;
	
	struct system_timer {
		std::atomic<timestamp> curr_time = 0;
		timestamp now () { return curr_time.fetch_add(1); }
	};

	struct random_blocker {

		std::mt19937 gen;
		std::uniform_int_distribution<int> dis;
		random_blocker() {
			std::random_device rd;
			gen = std::mt19937(rd());
			dis = std::uniform_int_distribution<int>(0, 100);
		}
		void random_sleep() {
			std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));
		}
	};

	class action {
		public:
			const action_type t;
			const std::string object;
			bool canceled = false;

	};

	class transaction {
		friend concurrency_controller;
		public:
			const std::vector<action> action_sequence;
			const std::vector<string> read_objects, write_objects;
			timestamp get_starting_time () const {return timestamp;}
			void set_starting_time (timestap t) {starting_time = t;}
			transaction_state get_state () {return state;}
		private:
			std::vector<action>::iterator next_action;
			timestamp priority;
			transaction_state state;
	};

	class concurrency_controller {
		public:
			void request_execution (std::vector<transaction>&&);
			void request_execution (std::vector<transaction>&);
			template <typename Observer>
			void launch_execution (Observer);
		private:
			std::mutex t_list_mutex;
			system_timer timer;
			random_blocker blocker;
			std::atomic<bool> stop = false;
			std::vector<transaction> pending_transactions;
			std::thread main_thread;
			static bool conflicts (const transaction&, const transaction&);
			template <typename Observer>
			void serve_transactions(Observer);
	};
	void concurrency_controller::request_execution(std::vector<transaction>&& transactions) {
		t_list_mutex.lock();
		pending_transactions.insert(end(pending_transactions), begin(transactions), end(transactions));
		t_list_mutex.unlock();
	}

	void concurrency_controller::request_execution(std::vector<transaction>& transactions) {
		t_list_mutex.lock();
		pending_transactions.insert(end(pending_transactions), begin(transactions), end(transactions));
		t_list_mutex.unlock();
	}

	template <typename Observer>
	void concurrency_controller::launch_execution(Observer observer) {
		main_thread = thread(&concurrency_controller::serve_transactions(), this, observer);
	}

	template <typename Observer>
	void concurrency_controller::serve_transactions(Observer observer) {
		while (!stop) {
			if (pending_transactions.empty())
				continue;

			t_list_mutex.lock();
			std::vector<transaction> transaction_list;
			std::swap(pending_transactions, transaction_list);
			t_list_mutex.unlock();

			std::vector<std::reference_wrapper<action>> execution_plan;

			std::thread worker_threads[4];

			unsigned int ended_transactions = 0;
			while (ended_transactions<transaction_list.size()) {
				for (int i=0; i<transaction_list.size(); ++i) {
					if (worker_threads[i%4].joinable())
						worker_threads[i%4].join();
					worker_threads[i%4] = std::thread();
					if (transaction_list[i].state == EVAL) {
						transaction_list.priority = timer.now();
						ended_transactions++;
					}

				}
			}
		}
	}
}

static bool concurrency_controller::conflicts (const transaction& ti, const transaction& tj){
	for (const auto ti_object : ti.write_objects) {
        if (std::binary_search(tj.read_objects.begin(), tj.read_objects.end(), ti_object)) {
            return true;
    	}
    	if (std::binary_search(tj.write_objects.begin(), tj.write_objects.end(), ti_object)) {
            return true;
    	}
    }

    return false;
}
