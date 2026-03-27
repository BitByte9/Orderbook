#include <bits/stdc++.h>
using namespace std;
#include <numeric>
#include <chrono>
#include <ctime>

#include "Order.h"

struct LevelInfo
{
    int price_;
    int quantity_;
};

using LevelInfos = vector<LevelInfo>;

class OrderbookLevelInfos
{
private:
    LevelInfos bids_;
    LevelInfos asks_;

public:
    OrderbookLevelInfos(const LevelInfos &bids, const LevelInfos &asks)
        : bids_{bids},
          asks_(asks) {}

    const LevelInfos &GetBids() const
    {
        return bids_;
    }
    const LevelInfos &GetAska() const
    {
        return asks_;
    }
};

class OrderModify
{
private:
    int orderId_;
    int price_;
    Side side_;
    int quantity_;

public:
    OrderModify(int orderId, int price, Side side, int quantity)
        : orderId_{orderId},
          price_{price},
          side_{side},
          quantity_{quantity}
    {
    }

    int GetOrderId() const
    {
        return orderId_;
    }
    int GetPrice() const
    {
        return price_;
    }
    int GetQuantity() const
    {
        return quantity_;
    }
    Side GetSide() const
    {
        return side_;
    }
    OrderPointer ToOrderPointer(OrderType type) const
    {
        return make_shared<Order>(type, GetOrderId(), GetSide(), GetPrice(), GetQuantity());
    }
};
struct TradeInfo
{
    int orderId_;
    int price_;
    int quantity_;
};

class Trade
{
private:
    TradeInfo bidTrade_;
    TradeInfo askTrade_;

public:
    Trade(const TradeInfo &bidTrade, const TradeInfo &askTrade)
        : bidTrade_{bidTrade},
          askTrade_{askTrade}
    {
    }

    const TradeInfo &GetBidInfo() const
    {
        return bidTrade_;
    }
    const TradeInfo &GetAskInfo() const
    {
        return askTrade_;
    }
};
using Trades = vector<Trade>;

class Orderbook
{
public:
    struct OrderEntry
    {
        OrderPointer order_{nullptr};
        OrderPointers::iterator location_;
    };
    map<int, OrderPointers, greater<int>> bids_;
    map<int, OrderPointers, less<int>> asks_;
    unordered_map<int, OrderEntry> orders_;

private:
    mutable std::mutex ordersMutex_;
    std::atomic<bool> shutdown_{false};
    std::condition_variable shutdownConditionVariable_;
    std::thread goodForDayThread_;

    void PruneGoodForDayOrders()
    {
        using namespace std::chrono;
        const auto end = hours(16);

        while (true)
        {
            const auto now = system_clock::now();
            const auto now_c = system_clock::to_time_t(now);
            tm now_parts = *localtime(&now_c);

            if (now_parts.tm_hour >= end.count())
                now_parts.tm_mday += 1;

            now_parts.tm_hour = end.count();
            now_parts.tm_min = 0;
            now_parts.tm_sec = 0;

            auto next = system_clock::from_time_t(mktime(&now_parts));
            auto till = next - now + milliseconds(100);

            {
                unique_lock ordersLock{ordersMutex_};

                if (shutdown_.load(std::memory_order_acquire) ||
                    shutdownConditionVariable_.wait_for(ordersLock, till) == std::cv_status::no_timeout)
                    return;
            }

            vector<int> orderIds;

            {
                std::scoped_lock ordersLock{ordersMutex_};

                for (const auto &[_, entry] : orders_)
                {
                    const auto &[order, __] = entry;

                    if (order->GetOrderType() != OrderType::GoodForDay)
                        continue;

                    orderIds.push_back(order->GetOrderId());
                }
            }

            CancelOrders(orderIds);
        }
    }

    bool CanMatch(Side side, int price)
    {
        if (side == Side::Buy)
        {
            if (asks_.empty())
            {
                return false;
            }

            const auto &[bestAsk, _] = *asks_.begin();
            return price >= bestAsk;
        }
        else
        {
            if (bids_.empty())
                return false;

            const auto &[bestBid, _] = *bids_.begin();
            return price <= bestBid;
        }
    }
    Trades MatchOrders()
    {
        Trades trades;
        trades.reserve(orders_.size());
        while (true)
        {
            if (bids_.empty() || asks_.empty())
            {
                break;
            }
            auto &[bidPrice, bids] = *bids_.begin();
            auto &[askPrice, asks] = *asks_.begin();

            if (bidPrice < askPrice)
                break;

            while (bids.size() && asks.size())
            {
                auto &bid = bids.front();
                auto &ask = asks.front();

                int quantity = min(bid->GetRemainQuantity(), ask->GetRemainQuantity());
                bid->Fill(quantity);
                ask->Fill(quantity);

                if (bid->isFilled())
                {
                    bids.pop_front();
                    orders_.erase(bid->GetOrderId());
                }
                if (ask->isFilled())
                {
                    asks.pop_front();
                    orders_.erase(ask->GetOrderId());
                }

                if (bids.empty())
                    bids_.erase(bidPrice);

                if (asks.empty())
                    asks_.erase(askPrice);
                trades.push_back(Trade{TradeInfo{bid->GetOrderId(), bid->GetPrice(), quantity},
                                       TradeInfo{ask->GetOrderId(), ask->GetPrice(), quantity}});
            }
        }
        return trades;
    }

public:
    Orderbook()
    {
        goodForDayThread_ = std::thread(&Orderbook::PruneGoodForDayOrders, this);
    }

    ~Orderbook()
    {
        shutdown_.store(true, std::memory_order_release);
        shutdownConditionVariable_.notify_all();
        if (goodForDayThread_.joinable())
            goodForDayThread_.join();
    }

    Trades AddOrder(OrderPointer order)
    {
        bool wasMarket = (order->GetOrderType() == OrderType::Market);
        bool wasFillAndKill = (order->GetOrderType() == OrderType::FillandKill);
        if (orders_.count(order->GetOrderId()))
        {
            return {};
        }

        if (order->GetOrderType() == OrderType::Market)
        {
            if (order->GetSide() == Side::Buy && !asks_.empty())
            {
                const auto &[worstAsk, _] = *asks_.rbegin();
                order->ToGoodTillCancel(worstAsk);
            }
            else if (order->GetSide() == Side::Sell && !bids_.empty())
            {
                const auto &[worstBid, _] = *bids_.rbegin();
                order->ToGoodTillCancel(worstBid);
            }
            else
                return {};
        }
        if (order->GetOrderType() == OrderType::FillandKill && !CanMatch(order->GetSide(), order->GetPrice()))
            return {};
       
        OrderPointers::iterator iterator;
        if (order->GetSide() == Side::Buy)
        {
            auto &orders = bids_[order->GetPrice()];
            orders.push_back(order);
            iterator = next(orders.begin(), orders.size() - 1);
        }
        else
        {
            auto &orders = asks_[order->GetPrice()];
            orders.push_back(order);
            iterator = next(orders.begin(), orders.size() - 1);
        }
        orders_.insert({order->GetOrderId(), OrderEntry{order, iterator}});
        auto trades = MatchOrders();
        if (wasMarket)
        {
            if (orders_.count(order->GetOrderId()))
            {
                CancelOrder(order->GetOrderId());
            }
        }
        if (wasFillAndKill && orders_.count(order->GetOrderId()))
        {
            CancelOrder(order->GetOrderId());
        }
        return trades;
    }
    void CancelOrders(vector<int> &OrderId)
    {
        for (auto it : OrderId)
        {
            if (!orders_.count(it))
                continue;

            const auto &[order, orderIterator] = orders_.at(it);
            orders_.erase(it);

            if (order->GetSide() == Side::Sell)
            {
                auto price = order->GetPrice();
                auto &orders = asks_.at(price);
                orders.erase(orderIterator);
                if (orders.empty())
                {
                    asks_.erase(price);
                }
            }
            else
            {
                auto price = order->GetPrice();
                auto &orders = bids_.at(price);
                orders.erase(orderIterator);
                if (orders.empty())
                    bids_.erase(price);
            }
        }
    }
    void CancelOrder(int OrderId)
    {
        if (!orders_.count(OrderId))
            return;
        const auto &[order, orderIterator] = orders_.at(OrderId);
        orders_.erase(OrderId);

        if (order->GetSide() == Side::Sell)
        {
            auto price = order->GetPrice();
            auto &orders = asks_.at(price);
            orders.erase(orderIterator);
            if (orders.empty())
            {
                asks_.erase(price);
            }
        }
        else
        {
            auto price = order->GetPrice();
            auto &orders = bids_.at(price);
            orders.erase(orderIterator);
            if (orders.empty())
                bids_.erase(price);
        }
    }
    Trades MatchOrder(OrderModify order)
    {
        if (!orders_.count(order.GetOrderId()))
        {
            return {};
        }

        const auto &[existingOrder, _] = orders_.at(order.GetOrderId());
        CancelOrder(order.GetOrderId());
        return AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));
    }
    OrderbookLevelInfos GetOrderInfo() const
    {
        LevelInfos bidInfos, askInfos;
        bidInfos.reserve(orders_.size());
        askInfos.reserve(orders_.size());

        auto CreateLevelInfos = [](int price, const OrderPointers &orders)
        {
            return LevelInfo{
                price,
                accumulate(
                    orders.begin(),
                    orders.end(),
                    0,
                    [](int runningSum, const OrderPointer &order)
                    {
                        return runningSum + order->GetRemainQuantity();
                    })};
        };

        for (const auto &[price, orders] : bids_)
            bidInfos.push_back(CreateLevelInfos(price, orders));
        for (const auto &[price, orders] : asks_)
            askInfos.push_back(CreateLevelInfos(price, orders));

        return OrderbookLevelInfos{bidInfos, askInfos};
    }
};

namespace
{
    optional<Side> ParseSide(const string &s)
    {
        if (s == "buy")
            return Side::Buy;
        if (s == "sell")
            return Side::Sell;
        return nullopt;
    }

    optional<OrderType> ParseOrderType(const string &s)
    {
        if (s == "gtc")
            return OrderType::GoodTillCancel;
        if (s == "fak")
            return OrderType::FillandKill;
        if (s == "gfd")
            return OrderType::GoodForDay;
        if (s == "mkt")
            return OrderType::Market;
        return nullopt;
    }

    void PrintTrades(const Trades &trades)
    {
        if (trades.empty())
        {
            cout << "No trades.\n";
            return;
        }
        for (const auto &trade : trades)
        {
            const auto &bid = trade.GetBidInfo();
            const auto &ask = trade.GetAskInfo();
            cout << "Trade | bidId=" << bid.orderId_
                 << " askId=" << ask.orderId_
                 << " qty=" << bid.quantity_
                 << " bidPx=" << bid.price_
                 << " askPx=" << ask.price_ << '\n';
        }
    }

    void PrintBook(Orderbook &book)
    {
        const auto infos = book.GetOrderInfo();
        cout << "----- ORDER BOOK -----\n";
        cout << "Bids:\n";
        for (const auto &lvl : infos.GetBids())
            cout << "  px=" << lvl.price_ << " qty=" << lvl.quantity_ << '\n';
        cout << "Asks:\n";
        for (const auto &lvl : infos.GetAska())
            cout << "  px=" << lvl.price_ << " qty=" << lvl.quantity_ << '\n';
        cout << "----------------------\n";
    }

    void PrintHelp()
    {
        cout << "Commands:\n";
        cout << "  add <id> <buy|sell> <qty> <gtc|fak|gfd|mkt> <price>\n";
        cout << "  cancel <id>\n";
        cout << "  modify <id> <buy|sell> <qty> <price>\n";
        cout << "  book\n";
        cout << "  help\n";
        cout << "  exit\n";
    }
}

int main(int argc, char **argv)
{
    const bool apiMode = (argc > 1 && string(argv[1]) == "--api");
    Orderbook book;
    if (!apiMode)
        PrintHelp();

    string line;
    while (true)
    {
        if (!apiMode)
            cout << "\n> ";

        if (!getline(cin, line))
            break;

        istringstream iss(line);
        string cmd;
        iss >> cmd;
        if (cmd.empty())
            continue;

        if (cmd == "exit")
            break;

        if (cmd == "help")
        {
            PrintHelp();
            if (apiMode)
                cout << "__END__\n";
            continue;
        }

        if (cmd == "book")
        {
            PrintBook(book);
            if (apiMode)
                cout << "__END__\n";
            continue;
        }

        if (cmd == "cancel")
        {
            int id;
            if (!(iss >> id))
            {
                cout << "Usage: cancel <id>\n";
            }
            else
            {
                book.CancelOrder(id);
                cout << "Cancel requested for id=" << id << '\n';
            }
            if (apiMode)
                cout << "__END__\n";
            continue;
        }

        if (cmd == "modify")
        {
            int id, qty, price;
            string sideText;
            if (!(iss >> id >> sideText >> qty >> price))
            {
                cout << "Usage: modify <id> <buy|sell> <qty> <price>\n";
                if (apiMode)
                    cout << "__END__\n";
                continue;
            }
            auto side = ParseSide(sideText);
            if (!side)
            {
                cout << "Invalid side.\n";
                if (apiMode)
                    cout << "__END__\n";
                continue;
            }

            auto trades = book.MatchOrder(OrderModify{id, price, *side, qty});
            PrintTrades(trades);
            if (apiMode)
                cout << "__END__\n";
            continue;
        }

        if (cmd == "add")
        {
            int id, qty, price;
            string sideText, typeText;
            if (!(iss >> id >> sideText >> qty >> typeText >> price))
            {
                cout << "Usage: add <id> <buy|sell> <qty> <gtc|fak|gfd|mkt> <price>\n";
                if (apiMode)
                    cout << "__END__\n";
                continue;
            }

            auto side = ParseSide(sideText);
            auto type = ParseOrderType(typeText);
            if (!side || !type)
            {
                cout << "Invalid side or order type.\n";
                if (apiMode)
                    cout << "__END__\n";
                continue;
            }

            auto order = make_shared<Order>(*type, id, *side, price, qty);
            auto trades = book.AddOrder(order);
            PrintTrades(trades);
            if (apiMode)
                cout << "__END__\n";
            continue;
        }

        cout << "Unknown command. Type 'help'.\n";
        if (apiMode)
            cout << "__END__\n";
    }

    return 0;
}