#pragma once

#include "polymorphic_value.h"
#include <memory>
#include <tuple>
#include <type_traits>
#include <vector>

namespace chain
{

template <class... Actions>
class BlockTuple;

template <class... Args>
class ExecutionChain;

// The structs below are used as predicate trait to detect if a certain
// structure is of a given type (ExecutionChain or BlockTuple).

template<class T>
struct is_block_tuple : std::false_type {};

template<class... T>
struct is_block_tuple<BlockTuple<T...>> : std::true_type {};

template<class T>
inline constexpr bool is_block_tuple_v = is_block_tuple<T>::value;

template<class... T>
struct is_execution_chain : std::false_type{};

template<class... T>
struct is_execution_chain<ExecutionChain<T...>> : std::true_type {};

// The aliases below are used as contrains to enable methods depending on the provided template argument type

template<class... NotBlockTupleT>
using enable_if_not_block_tuple = std::enable_if_t<!std::conjunction_v<is_block_tuple<std::decay_t<NotBlockTupleT>>...>, bool>;

template<class MaybeExecutionChain>
using enable_if_not_chain = std::enable_if_t<!is_execution_chain<std::decay_t<MaybeExecutionChain>>::value, bool>;

template<class MaybeExecutionChain>
using enable_if_chain = std::enable_if_t<is_execution_chain<std::decay_t<MaybeExecutionChain>>::value, bool>;

template<class... MaybeExecutionChain>
using enable_if_chains = std::enable_if_t<std::conjunction_v<is_execution_chain<std::decay_t<MaybeExecutionChain>>...>, bool>;

/**
 * \brief A ExecutionChain allows the chaining of different actions together, 
 * and the ability to execute these actions in the order they were chained.
 * \code{.cpp}
 *    ExecutionChain<int> chain = start_chain | [](int& a) { ++a; } | | [](int& a) { a*=4; };
 *    int x = 0;
 *    chain(x); // --> x = 4
 * \endcode
 *
 * \details The actions chained in the ExecutionChain must be callables (e.g. lambdas or functors).
 * The executors (functors) do not need to return booleans, unless they are
 * used in a logic flow (if then else - see unit tests as examples). <br>
 * The executors are called on the provided arguments of types Args. <br>
 * Each executor acts on the provided arguments (note that the arguments may have been modified by previous
 * executors in the chain). <br>
 * The `start_chain` keyword is used to start the creation of a BlockTuple which may further be assigned to an ExecutionChain.
 * The operator| and operator|= operators are used for chaining of the actions, and the Execute and the operator() methods are
 * used to execute all the actions in the order they were added.
*/ 
template <class... Args>
class ExecutionChain {
    // IExecutionBlock represents a single block in the execution chain
    class IExecutionBlock {
    public:
        virtual ~IExecutionBlock() = default;
        virtual void Execute(Args&...) = 0;
    };

    // Concrete implementation of IExecutionBlock, storing a specific action
    // An action could for instance : struct A { void operator()(int a, double b){} };
    // or a lambda
    template <class Action>
    class ExecutionBlock : public IExecutionBlock {
    public:
        struct is_action_invocable : std::is_invocable<Action, Args&...> {};
        static_assert(is_action_invocable::value, "The action must be invocable with the provided arguments.");

        explicit ExecutionBlock(Action&& action) : m_action(std::forward<Action>(action)) {}
        explicit ExecutionBlock(Action& action) : m_action(std::move(action)) {}
        explicit ExecutionBlock(const Action& action) : m_action(action) {}

        // Calls the stored action with the given arguments
        // If the stored action is a class, it must implement the operator()(Args... args)
        // For instance : struct A { void operator()(int a, double b){} };
        void Execute(Args&... args) override {
            assert((std::is_invocable_v<decltype(m_action), Args&...> &&
                   "Action::operator() is not callable as non const"));
            std::invoke(m_action, args...);
        }
    private:
        Action m_action;
    };

    using ExecutionBlock_t = IExecutionBlock;
    using ExecutionBlockList_t = std::vector<polymorphic_value<ExecutionBlock_t>>;

public:

    template<class... ActionsT>
    struct are_actions_invocable : std::conjunction<std::is_invocable<ActionsT, Args&...>...> {};

    template<class... ActionsT>
    struct are_actions_invocable<BlockTuple<ActionsT...>>
        : std::conjunction<std::is_invocable<ActionsT, Args&...>...> {};

    template<class... ActionsT>
    using enable_if_actions_are_invocable = std::enable_if_t<are_actions_invocable<ActionsT...>::value, bool>;

    template<class BlockTupleT>
    using enable_if_compatible_block_tuple = std::enable_if_t<
        is_block_tuple_v<std::decay_t<BlockTupleT>>
        && are_actions_invocable<std::decay_t<BlockTupleT>>::value, bool>;

    ExecutionChain() {}

    ExecutionChain(const ExecutionChain&) = default;
    ExecutionChain(ExecutionChain&&) = default;

    ExecutionChain& operator=(const ExecutionChain&) = default;
    ExecutionChain& operator=(ExecutionChain&&) = default;

    template <class ActionT,
              enable_if_not_block_tuple<ActionT> = true,
              enable_if_not_chain<ActionT> = true
    >
    ExecutionChain(ActionT&& action)
    {
        append(action);
    }

    template <class BlockTupleT, enable_if_compatible_block_tuple<BlockTupleT> = true>
    ExecutionChain(BlockTupleT&& handler)
    {
        CheckBlockTupleCompatibility(handler);
        append(handler);
    }

    // Clears the chain and append an action
    template <class ActionT,
              enable_if_not_block_tuple<ActionT> = true,
              enable_if_not_chain<ActionT> = true
    >
    ExecutionChain& operator=(ActionT&& action) {
        m_blocks.clear();
        return append(action);
    }

    // Append an action to the chain
    template <class ActionT, enable_if_not_block_tuple<ActionT> = true>
    ExecutionChain& operator|=(ActionT&& action) {
        return append(action);
    }

    template<class... ExecArgsT>
    struct acceptable_args : std::conjunction<std::is_convertible<ExecArgsT, Args>...> {};

    template<class... ExecArgsT>
    using enable_if_all_args_are_compatible = std::enable_if_t<acceptable_args<ExecArgsT...>::value, bool>;

    template<class... ExecArgsT, enable_if_all_args_are_compatible<ExecArgsT...> = true>
    void Execute(ExecArgsT&&... args) const {
        for (const auto& block : m_blocks) {
            block.get()->Execute(args...);
        }
    }

    template<class... ExecArgsT, enable_if_all_args_are_compatible<ExecArgsT...> = true>
    void operator()(ExecArgsT&&... args) const {
        Execute(args...);
    }

    template <class ActionT,
              enable_if_not_block_tuple<ActionT> = true,
              enable_if_not_chain<ActionT> = true>
    ExecutionChain& append(ActionT&& action) {
        m_blocks.emplace_back(create_block(action));
        return *this;
    }

    template <class BlockTupleT, enable_if_compatible_block_tuple<BlockTupleT> = true>
    ExecutionChain& append(BlockTupleT&& action) {
        m_blocks.emplace_back(create_block(action));
        return *this;
    }

    template<class... RhsT, enable_if_chains<RhsT...> = true>
    ExecutionChain& append(RhsT&&... rhs)
    {
        (m_blocks.insert(m_blocks.end(), rhs.m_blocks.begin(), rhs.m_blocks.end()), ...);
        return *this;
    }

    template<class BlockTupleT, enable_if_compatible_block_tuple<BlockTupleT> = true>
    ExecutionChain& operator|=(BlockTupleT&& handler)
    {
        CheckBlockTupleCompatibility(handler);
        append(handler);
        return *this;
    }

    template<class BlockTupleT, enable_if_compatible_block_tuple<BlockTupleT> = true>
    ExecutionChain& operator=(BlockTupleT&& handler)
    {
        CheckBlockTupleCompatibility(handler);
        m_blocks.clear();
        return this->operator|=(handler);
    }

private:
    template<class... ActionsT>
    constexpr static void CheckBlockTupleCompatibility(BlockTuple<ActionsT...>&)
    {
        // because of the enable_if, this assertion won't fail
        // keep it to prevent the removal of the enable_if
        static_assert(are_actions_invocable<BlockTuple<ActionsT...>>::value,
            "When passing a block tuple to an execution list, the arguments expected by the handlers (blocks)"
            " must be compatible with the ones expected to execute the ExecutionChain");
    }

    template <class ActionT>
    static auto create_block(ActionT&& action)
    {
        return std::make_unique<ExecutionBlock<std::decay_t<ActionT>>>(std::forward<ActionT>(action));
    }

    ExecutionBlockList_t m_blocks;
};

/**
 * \brief BlockTuple builds a compile-time list of actions. <br>
 * Therefore there is no indirection when passing from action to the next one. <br>
 * The BlockTuple can be executed as such or moved to an ExecutionChain
 * Compared to an ExecutionChain, the BlockTuple exposes the list of actions
 * instead of the list of types expected for execution. <br>
 * As a consequence, the BlockTuple can be executed with all the () overloads supported
 * by the ActionsT. <br>
 * For instance, the class below is invocable both with an int and a std::string :
 * \code{.cpp}
 * struct MyAction { void operator()(int); void operator()(std::string); };
 * auto handler = start_chain | MyAction{} | MyAction{}; // repeats MyAction
 * \endcode
 * Therefore, BlockTuple<MyAction> handler can be used as :
 * \code{.cpp}
 * handler(int{5}); // with an int
 * handler("Coucou"); // with a std::string
 * // Because of MyAction, both of the above calls are valid.
 * \endcode
*/
template <class... ActionsT>
class BlockTuple
{
    using ActionTuple_t = std::tuple<ActionsT...>;
public:
    template<class... OtherActionsT, enable_if_not_block_tuple<OtherActionsT...> = true>
    constexpr explicit BlockTuple(OtherActionsT&&... actions) : m_actions(actions...) {}

    BlockTuple(const BlockTuple&) = default;
    BlockTuple(BlockTuple&&) = default;

    template<class... ArgsT>
    struct are_actions_invocable;

    template<template<class...> class BlockTupleT, class... Actions, class... ArgsT>
    struct are_actions_invocable<BlockTupleT<Actions...>, ArgsT...>
        : std::conjunction<std::is_invocable<Actions, ArgsT&...>...> {};

    template <class... ArgsT,
              std::enable_if_t<are_actions_invocable<
                    const BlockTuple<ActionsT...>, ArgsT...>::value, bool> = true>
    constexpr void Execute(ArgsT&&... args) const
    {
        std::apply([&](auto&&... actions)
        {
            (std::invoke(std::forward<decltype(actions)>(actions), std::forward<ArgsT>(args)...), ...);
        }, m_actions);
    }

    template <class... ArgsT,
              std::enable_if_t<are_actions_invocable<
                    BlockTuple<ActionsT...>, ArgsT...>::value, bool> = true>
    constexpr void Execute(ArgsT&&... args)
    {
        std::apply([&](auto&&... actions)
        {
            (std::invoke(std::forward<decltype(actions)>(actions), std::forward<ArgsT>(args)...), ...);
        }, m_actions);
    }

    template <class... ArgsT,
              std::enable_if_t<are_actions_invocable<
                  const BlockTuple<ActionsT...>, ArgsT...>::value, bool> = true>
    constexpr void operator()(ArgsT&&... args) const
    {
        Execute(std::forward<ArgsT>(args)...);
    }

    template <class... ArgsT,
              std::enable_if_t<are_actions_invocable<
                  BlockTuple<ActionsT...>, ArgsT...>::value, bool> = true>
    constexpr void operator()(ArgsT&&... args)
    {
        Execute(std::forward<ArgsT>(args)...);
    }

    template <class... OtherActions>
    constexpr BlockTuple<ActionsT..., OtherActions...> operator|(BlockTuple<OtherActions...> other)
    {
        return BlockTuple<ActionsT..., OtherActions...>(std::tuple_cat(m_actions, other.m_actions));
    }

    template <class OtherActionT
             , std::enable_if_t<!is_block_tuple_v<OtherActionT>, bool> = true
    >
    constexpr BlockTuple<ActionsT..., OtherActionT> operator|(OtherActionT&& other)
    {
        return std::apply(
            [](ActionsT&&... actions, OtherActionT&& otherAction) -> BlockTuple<ActionsT..., OtherActionT>
            {
                return BlockTuple<ActionsT..., OtherActionT>{ actions..., otherAction };
            },
            std::tuple_cat(
                m_actions,
                std::forward_as_tuple<OtherActionT>(std::forward<OtherActionT>(other))
            )
        );
    }

    template <class OtherActionT
             , std::enable_if_t<!is_block_tuple_v<OtherActionT>, bool> = true
    >
    constexpr BlockTuple<ActionsT..., OtherActionT> operator|(OtherActionT& other)
    {
        return std::apply(
            [](ActionsT&&... actions, const OtherActionT& otherAction) -> BlockTuple<ActionsT..., OtherActionT>
            {
                return BlockTuple<ActionsT..., OtherActionT>{ actions..., otherAction };
            },
            std::tuple_cat(
                m_actions,
                std::forward_as_tuple<OtherActionT>(std::forward<OtherActionT>(other))
            )
        );
    }

private:
    ActionTuple_t m_actions;
};

template <>
class BlockTuple<void>
{
public:
    template <class... OtherActions>
    constexpr BlockTuple<OtherActions...> operator|(BlockTuple<OtherActions...> other) const
    {
        return BlockTuple<OtherActions...>(other.m_actions);
    }

    template <class OtherActionT
        , std::enable_if_t<!is_block_tuple_v<OtherActionT>, bool> = true
    >
    constexpr BlockTuple<OtherActionT> operator|(OtherActionT&& other) const
    {
        return BlockTuple<OtherActionT>{ other };
    }

    template <class OtherActionT
        , std::enable_if_t<!is_block_tuple_v<OtherActionT>, bool> = true
    >
    constexpr BlockTuple<std::decay_t<OtherActionT>> operator|(OtherActionT& other) const
    {
        return BlockTuple<std::decay_t<OtherActionT>>{ other };
    }
};

template<class... Actions>
BlockTuple(Actions...) -> BlockTuple<Actions...>;

// Call this function to kick off the definition of an execution chain
// Usage : auto blockTuple = start_chain | Action1{} | Action2{};
static inline constexpr auto start_chain =  BlockTuple<void>{};

template<class LhsT, class... RhsT,
         enable_if_chains<LhsT, RhsT...> = true
>
static auto operator|(LhsT&& lhs, RhsT&&... rhs)
{
    auto output = lhs;
    output.append(rhs...);
    return output;
}

namespace ChainStep
{
    struct LogicFlow {};
};

} // namespace chain

