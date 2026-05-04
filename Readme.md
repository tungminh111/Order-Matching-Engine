# Project Overall
An order matching engine built using C++, focus heavily on performance optimization.

# Requirement 
Input of the engine is a list of orders that are streamed via a TCP connection.
The engine need to output:
- L2 order book data change via UDP multicast (for order book construction)
- Matched order via another TCP connection (for post trade / notification systems)

Order schema:
- order_id
- timestamp
- instrument_id
- price
- quantity
- side (BUY/SELL)
- action (CREATE/MODIFY/CANCEL)
- type (LIMIT/MARKET)

Limit order needs to be matched ordered by price, and the order it arrives the server.

# Non-functional requirement
- Zero dynamic allocation on the hot path
- Lock free concurrency
- Cache line optimization
- Thread affinity

# Project Design & Implementation  
## High Level Design 
To avoid any steps to be bottlenecked by others, split the stream consumer, engine, and publishers to different.
```mermaid
flowchart TD
    Input -- tcp packet --> TCP stream consumer 
    TCP stream consumer -- Order SPSC --> Matcher engine
    Matcher engine -- L2 Data SPSC --> L2DataPublisher
    L2DataPublisher -- UDP multicast --> Output
    Matcher engine -- MatchedOrder SPSC --> MatchedOrderPublisher
    MatchedOrderPublisher -- UDP multicast --> Output
```

Each of threads needs to use a while (true) loop to check if there are available data to proceed, to ensure no thread sleep + context switch overhead happen

## Project setup
Using Cmake for project setup + C++26, to ultilize the latest features of C++.
```
root 
├──src
│  ├─OrderMatcher.cpp
│  └─CmakeLists.txt
├──include
│  └─OrderMatcher.hpp
├──tests
│  ├─OrderMatcherTest.cpp
│  └─CmakeLists.txt
├──Readme.MD
└──CmakeLists.txt
```

## TCP stream consumer implementation
### The TCP server
The goal of this layer is to create a TCP server that receives orders in bytes.

The first thing come to my mind is to create a TCP server using socket library, straighforward, and high performance!

The server reads the data using recv function, and buffer the data into a Ring Buffer. 

The server initially simply reads chunk of data, and decode to Order object with the assumption that I can guarantee the capacity can be devided by the size of Order object.
Therefore, no boundary handling is made (for example, list 8 bytes in the end of the buffer + first 8 bytes of the buffer are of the same object).

The capacity of the buffer is chosen as 2 ^ 15, to ensure the modulo operations of pointers can be optimized by the compiler.

### The Schema problem

## MatcherEngine implementation

## MatchedOrderPublisher implementation
