#include <vector>
#include <string>
#include <atomic>
#include <random>
#include <functional>
#include <mutex>
#include <chrono>
#include <utility>
#include <thread>
#include <iterator>

namespace opt {

	using std::end;
	using std::begin;

	typedef std::vector<std::pair<std::reference_wrapper<transaction>,action> execution_plan_type;

	enum action_type {READ, WRITE};
	enum transaction_state {BEGIN, EVAL, STORE, COMMIT, CANCEL};
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
			const action_type type;
			const std::string object;
			const std::string transaction_name;
			action(action_type a, const std::string& name) : type(a), object(name) {}

	};

	class transaction {
		friend concurrency_controller;
		public:
			const std::string name;
			const std::vector<action> action_sequence;
			const std::vector<string> read_objects, write_objects;
			transaction(std::vector<action>& a, std::vector<string>& r, std::vector<string>& w, std::string n)
			: name(n), action_sequence(a), read_objects(r), write_objects(w) {}
			timestamp get_starting_time () const {return timestamp;}
			void set_starting_time (timestap t) {starting_time = t;}
			transaction_state get_state () {return state;}
			timestamp get_timestamp() {return priority;}
		private:
			timestamp priority;
			transaction_state state;
	};

	class concurrency_controller {
		public:
			~concurrency_controller();
			void request_execution (std::vector<transaction>&&);
			void request_execution (std::vector<transaction>&);
			template <typename Observer>
			void launch_execution (Observer);
		private:
			std::mutex t_list_mutex;
			system_timer timer;
			std::atomic<bool> stop = false;
			std::vector<transaction> pending_transactions;
			std::thread main_thread;
			void concurrency_controller::execute_transaction(transaction&, execution_plan_type&, std::mutex&);
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
		main_thread = thread(&concurrency_controller::serve_transactions, this, observer);
	}

	void concurrency_controller::execute_transaction(transaction& t, execution_plan_type& execution_plan, std::mutex& m) {
		random_blocker rb;
		for (action a : t.action_sequence) {
			rb.random_sleep();
			m.lock();
			execution_plan.push_back(std::make_pair(std::ref(t), a));
			m.unlock();
		}
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

			execution_plan_type execution_plan;
			std::mutex xp_mutex;
			std::thread worker_threads[4];

			for (int i=0; i<transaction_list.size(); ++i) {
				if (worker_threads[i%4].joinable())
					worker_threads[i%4].join();
					worker_threads[i%4] = std::thread(&concurrency_controller::execute_transaction,
						this, transaction_list[i], execution_plan, xp_mutex);
			}

			std::sort(begin(transaction_list), end(transaction_list), [](const transaction& a, const transaction& b) {
				return a.get_timestamp() < b.get_timestamp();
			});

			for (int i=0; i<transaction_list.size(); i++) {
				if (transaction_list[i].state == CANCEL) continue;
				for (int j=i+1; j<transaction_list.size(); j++) {
					if (transaction_list[j].state == CANCEL) continue;
					if (conflicts(transaction_list[i], transaction_list[j])) {
						//kill newer transaction and put it back on the pending transactions
						auto canceled_copy = transaction_list[j];
						transaction_list[j].state = CANCEL;
						t_list_mutex.lock();
						pending_transactions.push_back(canceled_copy);
						t_list_mutex.unlock();
					}
				}
			}
			for (auto pair : execution_plan) {
				if (pair.first.state == CANCEL) continue;
				observer.notify(pair);
			}
		}
	}
	concurrency_controller::~concurrency_controller() {
		stop = true;
		if (main_thread.joinable())
			main_thread.join();
	}
}
