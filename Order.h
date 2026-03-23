#pragma once
#include <bits/stdc++.h>
using namespace std;
#include "OrderType.h"
#include "Side.h"

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
    void ToGoodTillCancel(int price)
    {
        if(GetOrderType() != OrderType:: Market)
        {
           // throw::logic_error(format("Order ({}) cannot have its price adjusted, only market orders can.", GetOrderId()));
        }

        price_ = price;
        orderType_ = OrderType::GoodTillCancel;

    }
};

using OrderPointer = shared_ptr<Order>;
using OrderPointers = list<OrderPointer>;