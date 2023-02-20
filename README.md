# Execution Chain

A C++ header-only library providing an ExecutionChain class, that allows the chaining of different actions together,
and the ability to execute these actions in the order they were chained.

# Requirements
This library requires a C++17 compatible compiler.

# Usage

```cpp
#include "ExecutionChain.h"

ExecutionChain<int> chain = start_chain 
    | [](int& a) { ++a; }
    | [](int& a) { a*=4; };
int x = 0;
chain(x); // --> x = 4
```

The actions chained in the ExecutionChain must be callables (e.g. lambdas or functors).
The executors (functors) do not need to return booleans, unless they are used in a logic flow (if then else - see unit tests as examples).
The executors are called on the provided arguments of types Args from the `ExecutionChain<Args...>` template.
Each executor acts on the provided arguments (note that the arguments may have been modified by previous executors in the chain).

The `start_chain` keyword is used to start the creation of a BlockTuple which may further be assigned to an ExecutionChain.
The operator| and operator|= operators are used for chaining of the actions, and the Execute and the operator() methods are
used to execute all the actions in the order they were added.

Another example using functors and conditional flow :

```cpp
struct Pod { int step; double time; double velocity; vec3 direction; };
struct Race {
    std::vector<Pod> m_myPods;
    std::vector<Pod> m_otherPods;
    Actions m_actions;
};

struct SteerToReachTarget { void operator()(Race&); };
struct FindCollisions { bool operator()(Race&); };
struct ShieldToPreventCollisions { bool operator()(Race&); };
struct SteerToPreventCollisions { void operator()(Race&); };
struct Thrust { void operator()(Race&); };

ExecutionChain<Race&> raceGamePlay = start_chain
    | SteerToReachTarget()
    | If(FindCollisions())
        .Then(Try(ShieldToPreventCollisions())
              .Fallback(SteerToPreventCollisions()))
    | Thrust();

// game loop
while (!endOfGame())
{
    raceGamePlay(race);
}
```
Example inspired from the codingame [mad-pod-racing](https://www.codingame.com/multiplayer/bot-programming/mad-pod-racing).
See [example code](example/pod_race.cpp).


## BlockTuple

BlockTuple is used as a building block for the ExecutionChain, it's a structure that holds a sequence of actions to be executed.
It can be constructed using the `start_chain` keyword, followed by actions separated by the pipe `|` operator. The sequence of actions
can be further assigned to an ExecutionChain.

# Context

In many software systems, certain tasks are designed as a sequential set of actions that need to be executed in a particular order.
These are commonly referred to as workflows.
As software systems grow, these workflows can become increasingly complex and often have unclear responsibilities or duplicated functionality, making it challenging to extend the system with new tasks.

To simplify these systems, one solution is to implement the [Chain of Responsibility pattern](https://refactoring.guru/design-patterns/chain-of-responsibility).
This a design pattern that passes requests along a chain of handlers.
This pattern allows tasks to be isolated into dedicated execution blocks, making it easier to manage and extend the system with new tasks.

The Chain of Responsibility pattern provides a way to handle a request by letting multiple handlers handle the request one after another.
We will refer to these handlers as 'execution blocks', and we will call the request 'execution context'.

Each execution block processes a part of the task and decides whether the execution can continue to the next execution block.
By doing so, the Chain of Responsibility pattern avoids coupling the client code to specific execution blocks and enables the system to handle tasks more dynamically.

# License
This code is licensed under the Apache license. See LICENSE file for more details.
