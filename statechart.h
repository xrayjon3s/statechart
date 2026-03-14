// Copyright (c) 2026 Chris Leger
// Licensed under the MIT License

// statechart.h - Header-only C++20 Hierarchical State Machine (HSM) framework
//
// Based on David Harel's original statechart paper.
// Provides macros for defining states, events, and transitions.

#ifndef STATECHART_H_
#define STATECHART_H_

#include <iostream>
#include <memory>
#include <type_traits>
#include <variant>

// statechart namespace - contains utility types for the framework
namespace statechart {

// Overloaded - Visitor pattern helper for std::variant
// Inherits from all provided lambda types to enable operator() overloads.
// Required for std::visit to work with multiple lambda types.
template <class... Ts>
struct Overloaded : Ts... {
  using Ts::operator()...;
};

template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

}  // namespace statechart

// STATECHART - Defines the top-level state (root of the state hierarchy)
//
// Parameters:
//   Name         - Name of the top-level state class
//   _EventType   - std::variant type containing all event types
//   _ContextType - Type of the context object passed to Enter/Exit
//
// Generates a class with:
//   - Public: make(), defer(), Start(), Dispatch(), Switch()
//   - Protected: Transition(), EnterState(), ExitState() (internal use)
//   - Virtual: stay(), name(), Depth(), ParentState(), Enter(), Exit(), HandleEvent()
//   - Type aliases: Base, Parent, ParentType, _Event, _Context
#define STATECHART(Name, _EventType, _ContextType)                           \
  class Name {                                                              \
   public:                                                                  \
    using Base = Name;                                                      \
    using Parent = Name;                                                    \
    using ParentType = Name;                                                \
    using _Event = _EventType;                                             \
    using _Context = _ContextType;                                         \
                                                                           \
    static Base* make() {                                                  \
      static Name singleton;                                                \
      return &singleton;                                                   \
    }                                                                      \
                                                                           \
    virtual Base* stay() { return this; }                                  \
                                                                           \
    static Base* defer(const _Event& event, _Context ctx) {               \
      return make()->ParentState()->HandleEvent(event, ctx);               \
    }                                                                      \
                                                                           \
    static Base* Start(Base* initial, _Context ctx) {                      \
      Base* top = make();                                                 \
      top->Enter(ctx);                                                    \
      if (initial && initial != top) {                                     \
        return Transition(top, initial, ctx);                              \
      }                                                                    \
      return top;                                                          \
    }                                                                      \
                                                                           \
    Base* Dispatch(const _Event& event, _Context ctx) {                    \
      Base* next = this->HandleEvent(event, ctx);                         \
      if (next && next != this) {                                         \
        return Transition(this, next, ctx);                                 \
      }                                                                    \
      return next ? next : this;                                          \
    }                                                                      \
                                                                           \
    virtual const char* name() const { return #Name; }                    \
    virtual int Depth() const { return 0; }                               \
    virtual Base* ParentState() const { return nullptr; }                 \
    virtual void Enter(_Context ctx);                                     \
    virtual void Exit(_Context ctx);                                      \
    virtual Base* HandleEvent(const _Event& event, _Context ctx);        \
                                                                           \
   protected:                                                             \
    template <typename... Fs>                                               \
    static Base* Switch(const _Event& event, Fs&&... fs) {                \
      return std::visit(                                                  \
          statechart::Overloaded<std::decay_t<Fs>...>{                   \
              std::forward<Fs>(fs)...},                                   \
          event);                                                         \
    }                                                                      \
                                                                           \
    static void EnterState(Base* state, Base* lca, _Context ctx) {        \
      if (!state) return;                                                \
      if (state == lca) return;                                          \
      EnterState(static_cast<Base*>(state->ParentState()), lca, ctx);     \
      state->Enter(ctx);                                                 \
    }                                                                     \
                                                                           \
    static void ExitState(Base* state, Base* lca, _Context ctx) {        \
      if (!state) return;                                                \
      if (state == lca) return;                                          \
      state->Exit(ctx);                                                  \
      ExitState(static_cast<Base*>(state->ParentState()), lca, ctx);      \
    }                                                                     \
                                                                           \
    static Base* Transition(Base* from, Base* to, _Context ctx) {        \
      if (!to) return from ? from : make();                              \
      if (!from) { EnterState(to, nullptr, ctx); return to; }          \
      int fromDepth = from->Depth();                                     \
      int toDepth = to->Depth();                                        \
      Base* fromCopy = from;                                            \
      Base* toCopy = to;                                                \
      while (fromDepth > toDepth) {                                     \
        fromCopy = static_cast<Base*>(fromCopy->ParentState());          \
        fromDepth--;                                                    \
      }                                                                  \
      while (toDepth > fromDepth) {                                     \
        toCopy = static_cast<Base*>(toCopy->ParentState());              \
        toDepth--;                                                      \
      }                                                                  \
      while (fromCopy != toCopy) {                                      \
        fromCopy = static_cast<Base*>(fromCopy->ParentState());          \
        toCopy = static_cast<Base*>(toCopy->ParentState());             \
      }                                                                  \
      Base* lca = fromCopy;                                             \
      ExitState(from, lca, ctx);                                        \
      EnterState(to, lca, ctx);                                         \
      return to;                                                        \
    }                                                                     \
  };

// STATE - Defines a child state in the hierarchy
//
// Parameters:
//   HSM   - The top-level state (HSM) type
//   Name  - Name of this state class
//   Parent - Name of the parent state class
//
// Generates a class that:
//   - Inherits from Parent
//   - Public static: make(), defer()
//   - Virtual: stay(), name(), Depth(), ParentState(), Enter(), Exit(), HandleEvent()
#define STATE(HSM, Name, Parent)                                           \
  class Name : public Parent {                                             \
   public:                                                                 \
    using Base = Name;                                                     \
    using Parent = Parent;                                                 \
    using ParentType = Parent;                                             \
                                                                           \
    static HSM::Base* make() {                                            \
      static Name singleton;                                               \
      return &singleton;                                                   \
    }                                                                      \
                                                                           \
    HSM::Base* stay() override { return this; }                           \
                                                                           \
    static HSM::Base* defer(const typename HSM::_Event& event,            \
                            typename HSM::_Context ctx) {                  \
      return Parent::make()->HandleEvent(event, ctx);                      \
    }                                                                      \
                                                                           \
    const char* name() const override { return #Name; }                   \
    int Depth() const override { return Parent::Depth() + 1; }            \
    HSM::Base* ParentState() const override { return Parent::make(); }    \
    virtual void Enter(typename HSM::_Context ctx);                         \
    virtual void Exit(typename HSM::_Context ctx);                         \
    using Parent::HandleEvent;                                             \
    virtual HSM::Base* HandleEvent(const typename HSM::_Event& event,     \
                                   typename HSM::_Context ctx);             \
  }

// HANDLE_EVENT - Defines the event handler implementation for a state
//
// Parameters:
//   HSM  - The top-level state (HSM) type
//   Name - Name of the state class to handle events for
//
// Usage:
//   HANDLE_EVENT(Root, A) {
//     return Switch(event,
//       [&](EvFoo) { return stay(); },
//       [&](EvBar) { return B::make(); });
//   }
#define HANDLE_EVENT(HSM, Name)                                            \
  HSM::Base* Name::HandleEvent(const typename Name::Parent::_Event& event, \
                               typename Name::Parent::_Context ctx)

#endif  // STATECHART_H_
