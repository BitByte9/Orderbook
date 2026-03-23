#include <bits/stdc++.h>
using namespace std;
enum class OrderType
{
    GoodTillCancel,
    FillandKill
};
enum class Side
{
    Buy,
    Sell
};

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

    const LevelInfos &GetBids()
    {
        return bids_;
    }
    const LevelInfos &GetAska()
    {
        return asks_;
    }
};

class Order
{
private:
    OrderType orderType_;
    int orderId_;
    int price_;
    int quantity_;
    int remQuantity_;
    Side side_;

public:
    Order(OrderType orderType, int orderId, Side side, int price, int quantity)
        : orderType_{orderType},
          orderId_{orderId},
          side_{side},
          price_{price},
          quantity_{quantity},
          remQuantity_{quantity}
    {
    }

    int GetOrderId() const
    {
        return orderId_;
    }

    Side GetSide() const
    {
        return side_;
    }
    int GetPrice() const
    {
        return price_;
    }
    int GetInitialQuantity() const
    {
        return quantity_;
    }
    OrderType GetOrderType() const
    {
        return orderType_;
    }
    int GetRemainQuantity() const
    {
        return remQuantity_;
    }
    int GetFilledQuantity() const
    {
        return GetInitialQuantity() - GetRemainQuantity();
    }
    bool isFilled()
    {
        return GetRemainQuantity() == 0;
    }
    void Fill(int quantity)
    {
        if (quantity > remQuantity_)
        {
            // throw logic_error();
        }
        remQuantity_ -= quantity;
    }
};

using OrderPointer = shared_ptr<Order>;
using OrderPointers = list<OrderPointer>;

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

    const TradeInfo &GetBidInfo()
    {
        return bidTrade_;
    }
    const TradeInfo &GetAskInfo()
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
    map< int, OrderPointers, greater< int>> bids_;
    map< int, OrderPointers, less< int>> asks_;
    unordered_map<int, OrderEntry> orders_;

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
        if (!bids_.empty())
        {
            auto &[_, bids] = *bids_.begin();
            auto &order = bids.front();
            if (order->GetOrderType() == OrderType::FillandKill)
                CancelOrder(order->GetOrderId());
        }
        if (!asks_.empty())
        {
            auto &[_, asks] = *asks_.begin();
            auto &order = asks.front();
            if (order->GetOrderType() == OrderType::FillandKill)
                CancelOrder(order->GetOrderId());
        }
        return trades;
    }

public:
    Trades AddOrder(OrderPointer order)
    {
        if (orders_.count(order->GetOrderId()))
        {
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
        return MatchOrders();
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

        auto CreateLevelInfos = []( int price, const OrderPointers &orders)
        {
            return LevelInfo{
                price,
                accumulate(
                    orders.begin(),
                    orders.end(),
                    0,
                    []( int runningSum, const OrderPointer &order)
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
    int Size(){
        return orders_.size();
    }

};


int main()
{
    Orderbook orderbook;
    const int orderId=1;
    orderbook.AddOrder(make_shared<Order>(OrderType::GoodTillCancel,orderId,Side::Buy,100,10));
    cout<<orderbook.Size()<<endl;
    orderbook.CancelOrder(orderId);
    cout<<orderbook.Size()<<endl;
    
    return 0;
}