
#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <array>
#include <list>
#include <map>
#include <cassert>

enum class Side {BUY, SELL};
enum class UpdateType {ADD, CANCEL};

struct MarketUpdate {

    using SymbId = uint32_t;
    using Price = uint64_t;
    using Qty = uint64_t;
    using SeqNum = uint64_t;
    using OrderId = uint64_t;

    SymbId symb_id;
    Price price;
    Qty qty;
    Side side;
    SeqNum seq_num;
    OrderId order_id;
    UpdateType update_type;
};
//XM197988707GB
class OrderBook {
    using Price = uint64_t;
    using Qty = uint64_t;
    using OrderId = uint64_t;

    struct Order {
        OrderId id;
        Price price;
        Qty qty;
    };

    using OrderList = std::list<Order>;

    struct OrderLocation {
        Side side;
        Price price;
        OrderList::iterator it;
    };

    using Bids = std::map<Price, OrderList>;
    using Asks = std::map<Price, OrderList>;
    //these are logn inserts i believe
    using SeqNum = uint64_t;

    Bids bids_;
    Asks asks_;
    SeqNum last_seq_{0};

    //this makes sure that we can delete the order in O1 time
    std::unordered_map<OrderId, OrderLocation> order_index_;

public:
    struct Snapshot {
        using BestBid = uint64_t;
        using BestAsk = uint64_t;
        using LastSeq = uint64_t;

        BestBid best_bid{0};
        BestAsk best_ask{0};
        LastSeq last_seq{0};
    };

    void add(Price price, Side side, OrderId id, Qty qty) {
        auto& book = (side == Side::BUY) ? bids_ : asks_;
        auto& orders = book[price];

        orders.push_back({id, price, qty});

        auto end = std::prev(orders.end());

        order_index_[id] = {side, price, end};

    }

    void cancel(OrderId order_id) {
        auto it = order_index_.find(order_id);
        if(it == order_index_.end()) return;


        auto& loc = it->second;
        auto& book = (loc.side == Side::BUY) ? bids_ : asks_;

        auto level = book.find(loc.price); //this doesnt auto crreat a price level
        level->second.erase(loc.it);

        if(level->second.empty()) book.erase(level);

        order_index_.erase(it);
    }

    void apply(MarketUpdate& u) {
        //cancel or add
        if(u.update_type == UpdateType::ADD) add(u.price, u.side, u.order_id, u.qty);
        else if(u.update_type == UpdateType::CANCEL) cancel(u.order_id);

        last_seq_ = u.seq_num;

    }

    const Snapshot read() const {
        Snapshot s{};

        if(!bids_.empty()) s.best_bid = bids_.rbegin()->first;
        if(!asks_.empty()) s.best_ask = asks_.begin()->first;
        s.last_seq = last_seq_;
        return s;
    }
};

template <typename T, std::size_t Capacity>
class ThreadSafeQueue {
    using Idx = std::size_t;
    using Size = std::size_t;
    using AtomicIdx = std::atomic<Idx>;
    using AtomicBool = std::atomic<bool>;
    using Diff = intptr_t;

    struct Slot {
        AtomicIdx sequence;
        T update_value;
    };

    std::array<Slot, Capacity> buffer;

    AtomicIdx enqueue_pos{0};
    AtomicIdx dequeue_pos{0};
    AtomicBool stopped_{0};

public:
    ThreadSafeQueue() {
        for(std::size_t i = 0; i<Capacity; ++i) {
            buffer[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    bool push(T item) {
        if(stopped_.load(std::memory_order_relaxed)) return false;

        Idx pos = enqueue_pos.load(std::memory_order_relaxed);

        for(;;) {
            Slot& slot = buffer[pos%Capacity];
            Idx seq = slot.sequence.load(std::memory_order_acquire);

            Diff diff = static_cast<Diff>(seq) - static_cast<Diff>(pos);

            if(diff == 0) {
                //ensurs that other threads haven't changed the position
                if(enqueue_pos.compare_exchange_weak(pos,
                    pos+1, std::memory_order_relaxed)){
                        break;
                }
            } else if(diff < 0){
                    //producer pushed item and another consumer has already taken it
                    // then the consumer woiuld update seq num
                    return false;
            } else {
                pos = enqueue_pos.load(std::memory_order_relaxed);
            }
        }

        buffer[pos%Capacity].update_value = std::move(item);
        buffer[pos%Capacity].sequence.store(pos+1, std::memory_order_release);

        return true;
    }

    bool pop(T& out) {
        Idx pos = dequeue_pos.load(std::memory_order_relaxed);

        for(;;){
            Slot& slot = buffer[pos%Capacity];
            Idx seq = slot.sequence.load(std::memory_order_acquire);

            Diff diff = static_cast<Diff>(seq) - static_cast<Diff>(pos+1);

            if(diff == 0) {
                //sets value of dequeue to pos+1 if dequeue hasnt changed between the time
                if(dequeue_pos.compare_exchange_weak(pos, pos+1,
                    std::memory_order_relaxed)) {
                        break;
                }
            }else if(diff < 0) {
                return false;
            } else {
                pos = dequeue_pos.load(std::memory_order_relaxed);
            }

        }
        Slot& slot_new = buffer[pos%Capacity];
        out = std::move(slot_new.update_value);

        slot_new.sequence.store(pos+Capacity, std::memory_order_release);

        return true;

    }

    Size size() const {
        const Idx head = enqueue_pos.load(std::memory_order_acquire);
        const Idx tail = dequeue_pos.load(std::memory_order_acquire);
        return head - tail;
    }

    void stop() {
        stopped_.store(1, std::memory_order_release);
    }
};

class MarketDataEngine {
    using SymbId = uint64_t;
    using Dropped = uint64_t;
    using AtomicDropped = std::atomic<Dropped>;
    using SeqNum = uint64_t;
    using BuffSize = std::size_t;

    std::unordered_map<SymbId, OrderBook> symb_to_book;
    static constexpr BuffSize buff_size = 1024;
    ThreadSafeQueue<MarketUpdate, buff_size> queue;
    AtomicDropped dropped_counter{0};
public:

    void publish(MarketUpdate u) {
        if(!queue.push(u)) {
            dropped_counter.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void dispatch_loop() {
        MarketUpdate u;
        while(queue.pop(u)) {
            auto& book = symb_to_book[u.symb_id];
            book.apply(u);
        }
    }

    void stop() {
        queue.stop();
    }

    OrderBook::Snapshot get_snapshot(SymbId sym_id) const {
        return symb_to_book.at(sym_id).read();
    }

    Dropped dropped() const {
        return dropped_counter;
    }


};

#include <iostream>
#include <thread>
#include <vector>
#include <unordered_set>

int main() {
    std::cout << "=== ORDER BOOK TESTS ===\n";

    {
        OrderBook book;

        std::cout << "[ADD] BUY  id=1 price=100 qty=10\n";
        book.add(100, Side::BUY, 1, 10);

        std::cout << "[ADD] BUY  id=2 price=105 qty=20\n";
        book.add(105, Side::BUY, 2, 20);

        std::cout << "[ADD] SELL id=3 price=110 qty=15\n";
        book.add(110, Side::SELL, 3, 15);

        std::cout << "[ADD] SELL id=4 price=108 qty=5\n";
        book.add(108, Side::SELL, 4, 5);

        auto s = book.read();

        std::cout << "[SNAPSHOT] best_bid=" << s.best_bid
                  << " best_ask=" << s.best_ask << "\n";

        assert(s.best_bid == 105);
        assert(s.best_ask == 108);

        std::cout << "[CANCEL] id=2\n";
        book.cancel(2);

        s = book.read();

        std::cout << "[SNAPSHOT] best_bid=" << s.best_bid
                  << " best_ask=" << s.best_ask << "\n";

        assert(s.best_bid == 100);

        std::cout << "[CANCEL] id=4\n";
        book.cancel(4);

        s = book.read();

        std::cout << "[SNAPSHOT] best_bid=" << s.best_bid
                  << " best_ask=" << s.best_ask << "\n";

        assert(s.best_ask == 110);
    }

    std::cout << "\n=== QUEUE TEST ===\n";

    {
        ThreadSafeQueue<int, 4> queue;

        std::cout << "[PUSH] 1\n";
        assert(queue.push(1));

        std::cout << "[PUSH] 2\n";
        assert(queue.push(2));

        std::cout << "[PUSH] 3\n";
        assert(queue.push(3));

        std::cout << "[PUSH] 4\n";
        assert(queue.push(4));

        std::cout << "[PUSH] 5 -> expected failure (full)\n";
        assert(!queue.push(5));

        int value;

        assert(queue.pop(value));
        std::cout << "[POP] " << value << "\n";
        assert(value == 1);

        assert(queue.pop(value));
        std::cout << "[POP] " << value << "\n";
        assert(value == 2);

        std::cout << "[PUSH] 5\n";
        assert(queue.push(5));

        std::cout << "[PUSH] 6\n";
        assert(queue.push(6));

        while (queue.pop(value)) {
            std::cout << "[POP] " << value << "\n";
        }

        assert(queue.size() == 0);
    }

    std::cout << "\n=== MPMC QUEUE TEST ===\n";

    {
        ThreadSafeQueue<int, 1024> queue;

        constexpr int N = 10000;
        constexpr int PRODUCERS = 2;
        constexpr int CONSUMERS = 2;
        constexpr int TOTAL = N * PRODUCERS;

        std::atomic<int> consumed{0};

        std::vector<std::thread> producers;

        for (int p = 0; p < PRODUCERS; ++p) {
            producers.emplace_back([&] {
                for (int i = 0; i < N; ++i) {
                    while (!queue.push(i))
                        std::this_thread::yield();
                }
            });
        }

        std::vector<std::thread> consumers;

        for (int c = 0; c < CONSUMERS; ++c) {
            consumers.emplace_back([&] {
                int value;

                while (consumed.load() < TOTAL) {
                    if (queue.pop(value))
                        consumed.fetch_add(1);
                    else
                        std::this_thread::yield();
                }
            });
        }

        for (auto& t : producers)
            t.join();

        for (auto& t : consumers)
            t.join();

        std::cout << "[MPMC] produced=" << TOTAL
                  << " consumed=" << consumed.load() << "\n";

        assert(consumed == TOTAL);
    }

    std::cout << "\n=== MARKET DATA ENGINE TEST ===\n";

    {
        MarketDataEngine engine;

        std::cout << "[PUBLISH] BUY  id=1001 price=100 qty=10\n";
        engine.publish({
            1, 100, 10, Side::BUY, 1, 1001, UpdateType::ADD
        });

        std::cout << "[PUBLISH] SELL id=1002 price=105 qty=20\n";
        engine.publish({
            1, 105, 20, Side::SELL, 2, 1002, UpdateType::ADD
        });

        std::cout << "[PUBLISH] CANCEL id=1001\n";
        engine.publish({
            1, 100, 0, Side::BUY, 3, 1001, UpdateType::CANCEL
        });

        engine.dispatch_loop();

        auto s = engine.get_snapshot(1);

        std::cout << "[SNAPSHOT] best_bid=" << s.best_bid
                  << " best_ask=" << s.best_ask
                  << " last_seq=" << s.last_seq << "\n";

        assert(s.best_bid == 0);
        assert(s.best_ask == 105);
        assert(s.last_seq == 3);
    }

    std::cout << "\nALL TESTS PASSED\n";
}
