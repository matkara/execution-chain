#include "../execution_chain/ExecutionChain.h"
#include "../execution_chain/ExecutionFlow.h"
#include <gtest/gtest.h>
#include <string>

namespace chain {

TEST(ExecutionChainTest, TestExecute) {
    // GIVEN an `ExecutionChain<int&, int&, std::string&>`
    // WHEN we use the `|=` operator to append an action that increments the values of `a` and `b` and sets the value of `c` to "hello"
    // AND we use the `|=` operator to append an action that doubles the values of `a` and `b` and appends " world" to the value of `c`
    
    ExecutionChain<int&, int&, std::string&> chain;
    chain |= [](int& a, int& b, std::string& c) { a = 1; b = 2; c = "hello"; };
    chain |= [](int& a, int& b, std::string& c) { a *= 2; b *= 2; c += " world"; };

    //  WHEN we call `Execute(x, y, z)` with `x = 0`, `y = 0`, and `z = ""`
    int x = 0;
    int y = 0;
    std::string z;
    chain.Execute(x, y, z);

    // THEN the values of `x` and `y` should be 2 and 4 respectively
    EXPECT_EQ(x, 2);
    EXPECT_EQ(y, 4);
    // AND the value of `z` should be "hello world"
    EXPECT_EQ(z, "hello world");
}

TEST(ExecutionChainTest, AppendAction)
{
    // GIVEN an ExecutionChain
    // WHEN we use the `|=` operator to append an action that increments the values of `a` and `b`
    ExecutionChain<int, int> executionChain;
    executionChain |= [&](int& a, int& b) { a++; b++; };

    // WHEN we call `Execute(x, y)` with `x = 0` and `y = 0`
    int x = 0;
    int y = 0;
    executionChain.Execute(x, y);

    // THEN the values of `x` and `y` should be 1 and 1 respectively
    EXPECT_EQ(x, 1);
    EXPECT_EQ(y, 1);
}

TEST(ExecutionChainTest, ChainingActions)
{
    // GIVEN an ExecutionChain
    // WHEN we use the `|=` operator to append actions that increments the values of `a` and `b`
    ExecutionChain<int, int> executionChain;
    executionChain |= start_chain | [&](int& a, int&) { a++; } | [&](int&, int& b) { b++; };

    // WHEN we call `Execute(x, y)` with `x = 0` and `y = 0`
    int x = 0;
    int y = 0;
    executionChain.Execute(x, y);

    // THEN the values of `x` and `y` should be 1 and 1 respectively
    EXPECT_EQ(x, 1);
    EXPECT_EQ(y, 1);
}

TEST(ExecutionChainTest, ChainingActionsWithParametersCopy)
{
    // GIVEN an ExecutionChain of type `ExecutionChain<int, int>`
    // WHEN we use the `|=` operator to append actions that **copy** of the values of `a` and `b`
    ExecutionChain<int&, int&> executionChain;
    // cppcheck-suppress uselessAssignmentArg
    executionChain |= start_chain | [](int a, int) { ++a; } | [](int, int b) { ++b; };

    // WHEN we call `Execute(x, y)` with `x = 0` and `y = 0`
    int x = 0;
    int y = 0;
    executionChain.Execute(x, y);

    // THEN the values of `x` and `y` have not changed
    EXPECT_EQ(x, 0);
    EXPECT_EQ(y, 0);
}

TEST(ExecutionChainTest, ClearActions)
{
    //  GIVEN an ExecutionChain object `executionChain` of type `ExecutionChain<int, int>`
    //  WHEN we use the `| = ` operator to append actions that increments the values of `a` and `b`
    //  WHEN we use the `=` operator to clear the actions

    ExecutionChain<int, int> executionChain;
    executionChain |= start_chain | [&](int& a, int&) { a++; } | [&](int&, int& b) { b++; };
    executionChain(3, 5);
    executionChain = {};

    //  WHEN we call `Execute(x, y)` with `x = 0` and `y = 0`

    int x = 0;
    int y = 0;

    executionChain.Execute(x, y);

    //  THEN the values of `x` and `y` should be 0 and 0 respectively
    EXPECT_EQ(x, 0);
    EXPECT_EQ(y, 0);
}

TEST(ExecutionChainTest, EmptyExecutionChain)
{
    // GIVEN an empty ExecutionChain
    ExecutionChain<int, int> executionChain;

    // WHEN calling Execute
    int x = 0;
    int y = 0;
    executionChain.Execute(x, y);

    // THEN nothing happens
    EXPECT_EQ(x, 0);
    EXPECT_EQ(y, 0);
}

template<class T>
struct expects;

template<class T>
struct gets;

template<class T, class U, class V = void>
struct execute_compiles : std::false_type {};

template<class T, class U>
struct execute_compiles<expects<T>, gets<U>,
                        std::void_t<decltype(std::declval<ExecutionChain<T>>().Execute(std::declval<U>()))>>
    : std::true_type {};

TEST(ExecutionChainTest, IncompatibleActions) {
    static_assert(execute_compiles<expects<int>, gets<int>>::value);
    static_assert(execute_compiles<expects<double>, gets<int>>::value);

    // GIVEN an ExecutionChain of type double
    // WHEN attempting to execute a std::string
    // THEN a compilation error should occur
    static_assert(!execute_compiles<expects<double>, gets<std::string>>::value);
}

TEST(ExecutionChainTest, CompatibleActionsWithClasses) {
    struct Action1 {
        void operator()(int& i) const { i += 1; }
    };

    struct Action2 {
        void operator()(int& i) { i *= 2; x = i; }
        int x = 0;
    };

    struct Action3 {
        void operator()(int& i) const { i -= 3; }
    };

    int value = 0;
    ExecutionChain<int&> chain;
    chain |= start_chain | Action1{} | Action2{};
    chain |= Action3{};
    chain.Execute(value);
    EXPECT_EQ(value, -1);
}

TEST(ExecutionChainTest, LambdaAndStructCanBePiped) {
    struct MyAction {
        void operator()(int& i) const { i *= 2; }
    };

    ExecutionChain<int> chain;
    int i = 2;
    chain = start_chain | [](int& i) { i += 2; } | MyAction{};
    chain.Execute(i);
    EXPECT_EQ(8, i);
}

TEST(ExecutionChainTest, IncompatibleBlockTupleAssignment) {
    // GIVEN a blocktuple that takes two int arguments
    BlockTuple<std::function<void(int, int)>> blockTuple{ [](int, int) {} };

    // WHEN we try to assign this blocktuple to an execution chain that takes one int and one double argument
    ExecutionChain<int, std::string> executionChain = {};
    //executionChain = blockTuple; // <-- this assignment must not compile
    
    // THEN a compile error is generated
    static_assert(!std::is_assignable_v<decltype(executionChain), decltype(blockTuple)>,
                   "BlockTuple should not be assignable to an execution chain with incompatible arguments");
}

TEST(ExecutionChainTest, IfThenElseFlow) {
    // GIVEN a blockTuple with an "if-then-else" flow
    BlockTuple blockTuple = start_chain | If([](const int a) { return a > 5; })
        .Then([](int& a) { a *= 2; })
        .Else([](int& a) { a /= 2; });

    auto pBlockTuple = std::make_unique<decltype(blockTuple)>(blockTuple);

    {
        // WHEN we call the blockTuple with an input of 10
        int x = 10;
        blockTuple(x);
        // THEN the input should be modified to 20
        EXPECT_EQ(20, x);

        // WHEN we call the blockTuple with an input of 4
        x = 4;
        blockTuple(x);
        // THEN the input should be modified to 2
        EXPECT_EQ(2, x);
    }
    {
        // WHEN we create an ExecutionChain from the blockTuple
        ExecutionChain<int> chain = *pBlockTuple;
        // AND we delete the blockTuple
        // (to ensure that the chain does not keep a reference to the blockTuple)
        pBlockTuple.reset();
        // AND we call the ExecutionChain with an input of 10
        int x = 10;
        chain(x);
        // THEN the input should be modified to 20 (just like before)
        EXPECT_EQ(20, x);

        // WHEN we call the ExecutionChain with an input of 4
        x = 4;
        chain(x);
        // THEN the input should be modified to 2 (just like before)
        EXPECT_EQ(2, x);
    }
}

TEST(ExecutionChainTest, ShouldBeCopiable) {
    ExecutionChain<int> chain = start_chain | [](int& a) { a += 5; };
    ExecutionChain<int> chain2 = chain;
    int x = 0;
    chain2(x);
    EXPECT_EQ(5, x);

    x = 0;
    chain(x);
    EXPECT_EQ(5, x);

    chain2 |= [](int& a) { a -= 10; };
    chain = chain2;

    x = 0;
    chain2(x);
    EXPECT_EQ(-5, x);

    x = 0;
    chain(x);
    EXPECT_EQ(-5, x);

    struct Action
    {
        Action()
            : fct{ [](int& a) { a += 47; } }
        {
        }

        Action(Action&&) = default;
        Action(const Action&) = default;

        bool operator()(int& a) const
        {
            fct(a);
            return true;
        }
        std::function<void(int&)> fct;
    };

    chain |= Action{};
    chain2 = chain;

    x = 0;
    chain(x);
    EXPECT_EQ(42, x);

    x = 0;
    chain2(x);
    EXPECT_EQ(42, x);
}

TEST(ExecutionChainTest, ShouldBeMovable) {
    ExecutionChain<int> chain = start_chain | [](int& a) { a += 5; };
    ExecutionChain<int> chain2 = std::move(chain);

    int x = 0;
    chain2(x);
    EXPECT_EQ(5, x);

    x = 0;
    chain(x);
    EXPECT_EQ(0, x);

    struct Action
    {
        Action()
            : fct{ [](int& a) { a += 37; } }
        {
        }

        Action(Action&&) = default;
        Action(const Action&) = default;

        bool operator()(int& a) const
        {
            fct(a);
            return true;
        }
        std::function<void(int&)> fct;
    };

    chain2 |= Action{};
    chain = std::move(chain2);

    x = 0;
    chain2(x);
    EXPECT_EQ(0, x);

    x = 0;
    chain(x);
    EXPECT_EQ(42, x);
}

TEST(ExecutionChainTest, ShouldBeChainable) {

    // GIVEN three ExecutionChain objects, "nothing", "chain", "chain2", and "chain3"
    // "nothing" is defined as an empty chain,
    // "chain" is defined as a chain with one action that adds 5 to its input
    // "chain2" is defined as a chain with one action that adds 5 to its input
    // "chain3" is defined as the combination of "chain", "chain2", and "nothing"

    const ExecutionChain<int> nothing = start_chain | [](int&) {};
    ExecutionChain<int> chain = start_chain | [](int& a) { a += 5; };
    ExecutionChain<int> chain2 = start_chain | [](int& a) { a += 5; };
    ExecutionChain<int> chain3 = chain | chain2 | nothing;

    // each chain is further modified by chaining additional actions that modify its input

    chain |= [](int& a) { a += 5; }; // x:0 => 5 + 5 = 10
    chain2 |= [](int& a) { a -= 10; }; // x:0 => 5 - 10 = -5
    chain3 |= [](int& a) { a += 32; }; // x:0 => 5 + 5 + 37 = 47

    // AND we execute each chain with an input of 0
    // THEN the output of "chain" should be 10, "chain2" should be -5 and "chain3" should be 42
    int x = 0;
    chain(x);
    EXPECT_EQ(10, x);

    x = 0;
    chain2(x);
    EXPECT_EQ(-5, x);

    x = 0;
    chain3(x);
    EXPECT_EQ(42, x);

    // AND when we reset "chain" and "chain2"
    chain = {};
    chain2 = {};

    // re-execute "chain3"
    x = 0;
    chain3(x);
    // the output should still be 42
    // showing that chain3 was not impacted by the modification of chain and chain2
    EXPECT_EQ(42, x);

    // WHEN modifying the action "nothing" provided with a const qualifier to the chain
    // to set its input to 0
    const_cast<ExecutionChain<int>&>(nothing) = [](int& a) { a = 0; };

    // AND re-execute "chain3"
    x = 0;
    chain3(x);
    // THEN the output should still be 42
    // showing that the chain does also not keep a reference to the const action.
    EXPECT_EQ(42, x);
}

} // namespace chain