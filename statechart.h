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

/* STATECHART - Defines the top-level state (root of the state hierarchy)
 *
 * Parameters:
 *   Name         - Name of the top-level state class
 *   _EventType   - std::variant type containing all event types
 *   _ContextType - Type of the context object passed to Enter/Exit
 *
 * Generates a class with:
 *   - Public: make(), defer(), Start(), Dispatch()
 *   - Protected: Enter(), Exit(), Switch(), Transition(), EnterState(), ExitState()
 *   - Virtual: stay(), name(), Depth(), ParentState(), HandleEvent()
 *   - Type aliases: Base, Parent, ParentType, _Event, _Context
 */
#define STATECHART(Name, _EventType, _ContextType)                           \
  class Name {                                                              \
   public:                                                                  \
    using Base = Name;                                                      \
    using Parent = Name;                                                    \
    using ParentType = Name;                                                \
    using _Event = _EventType;                                             \
    using _Context = _ContextType;                                         \
                                                                           \
    /* make() - Returns singleton instance of this state */                 \
    static Base* make() {                                                  \
      static Name singleton;                                                \
      return &singleton;                                                   \
    }                                                                      \
                                                                           \
    /* stay() - Stay in the current state (returns this state) */          \
    virtual Base* stay() { return this; }                                  \
                                                                           \
    /* defer() - Delegate event handling to parent state */                 \
    static Base* defer(const _Event& event, _Context ctx) {               \
      return make()->ParentState()->HandleEvent(event, ctx);               \
    }                                                                      \
                                                                           \
    /* Start() - Initialize state machine at given initial state */         \
    static Base* Start(Base* initial, _Context ctx) {                      \
      Base* top = make();                                                 \
      top->Enter(ctx);                                                    \
      if (initial && initial != top) {                                     \
        return Transition(top, initial, ctx);                              \
      }                                                                    \
      return top;                                                          \
    }                                                                      \
                                                                           \
    /* Dispatch() - Main entry point for event handling */                 \
    Base* Dispatch(const _Event& event, _Context ctx) {                    \
      Base* next = this->HandleEvent(event, ctx);                         \
      if (next && next != this) {                                         \
        return Transition(this, next, ctx);                                \
      }                                                                    \
      return next ? next : this;                                          \
    }                                                                      \
                                                                           \
    /* name() - Returns the state name as a C-string */                    \
    virtual const char* name() const { return #Name; }                    \
                                                                           \
    /* Depth() - Returns depth in hierarchy (root = 0) */                  \
    virtual int Depth() const { return 0; }                               \
                                                                           \
    /* ParentState() - Returns pointer to parent state (nullptr for root) */\
    virtual Base* ParentState() const { return nullptr; }                 \
                                                                           \
    /* HandleEvent() - Handle an event, returning next state */            \
    virtual Base* HandleEvent(const _Event& event, _Context ctx);          \
                                                                           \
   protected:                                                             \
    /* Enter() - Entry action (called when entering this state) */        \
    virtual void Enter(_Context ctx);                                     \
                                                                           \
    /* Exit() - Exit action (called when exiting this state) */           \
    virtual void Exit(_Context ctx);                                      \
                                                                           \
    /* Switch() - Helper for std::visit with Overloaded */                \
    template <typename... Fs>                                               \
    static Base* Switch(const _Event& event, Fs&&... fs) {                \
      return std::visit(                                                  \
          statechart::Overloaded<std::decay_t<Fs>...>{                   \
              std::forward<Fs>(fs)...},                                   \
          event);                                                         \
    }                                                                      \
                                                                           \
    /* EnterState() - Internal: enter states from lca up to target */     \
    static void EnterState(Base* state, Base* lca, _Context ctx) {        \
      if (!state) return;                                                \
      if (state == lca) return;                                          \
      EnterState(static_cast<Base*>(state->ParentState()), lca, ctx);     \
      state->Enter(ctx);                                                 \
    }                                                                     \
                                                                           \
    /* ExitState() - Internal: exit states from current down to lca */     \
    static void ExitState(Base* state, Base* lca, _Context ctx) {        \
      if (!state) return;                                                \
      if (state == lca) return;                                          \
      state->Exit(ctx);                                                  \
      ExitState(static_cast<Base*>(state->ParentState()), lca, ctx);      \
    }                                                                     \
                                                                           \
    /* Transition() - Internal: performs state transition with entry/exit */\
    static Base* Transition(Base* from, Base* to, _Context ctx) {        \
      if (!to) return from ? from : make();                              \
      if (!from) { EnterState(to, nullptr, ctx); return to; }            \
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

/* STATE - Defines a child state in the hierarchy
 *
 * Parameters:
 *   HSM   - The top-level state (HSM) type
 *   Name  - Name of this state class
 *   Parent - Name of the parent state class
 *
 * Generates a class that:
 *   - Inherits from Parent
 *   - Public static: make(), defer()
 *   - Virtual: stay(), name(), Depth(), ParentState(), Enter(), Exit(), HandleEvent()
 */
#define STATE(HSM, Name, Parent)                                           \
  class Name : public Parent {                                             \
   public:                                                                 \
    using Base = Name;                                                     \
    using Parent = Parent;                                                 \
    using ParentType = Parent;                                             \
                                                                           \
    /* make() - Returns singleton instance of this state */                 \
    static HSM::Base* make() {                                            \
      static Name singleton;                                               \
      return &singleton;                                                   \
    }                                                                      \
                                                                           \
    /* stay() - Stay in this state (returns this state) */                 \
    HSM::Base* stay() override { return this; }                           \
                                                                           \
    /* defer() - Delegate event handling to parent state */                 \
    static HSM::Base* defer(const typename HSM::_Event& event,            \
                            typename HSM::_Context ctx) {                  \
      return Parent::make()->HandleEvent(event, ctx);                      \
    }                                                                      \
                                                                           \
    /* name() - Returns the state name as a C-string */                   \
    const char* name() const override { return #Name; }                   \
                                                                           \
    /* Depth() - Returns depth in hierarchy (computed once) */                   \
    int Depth() const override {                                             \
      static int d = Parent::Depth() + 1;                                     \
      return d;                                                              \
    }            \
                                                                           \
    /* ParentState() - Returns pointer to parent state */                   \
    HSM::Base* ParentState() const override { return Parent::make(); }     \
                                                                           \
    /* Enter() - Entry action (called when entering this state) */          \
    virtual void Enter(typename HSM::_Context ctx);                         \
                                                                           \
    /* Exit() - Exit action (called when exiting this state) */             \
    virtual void Exit(typename HSM::_Context ctx);                         \
                                                                           \
    /* Bring parent's HandleEvent into this scope */                       \
    using Parent::HandleEvent;                                             \
                                                                           \
    /* HandleEvent() - Handle an event, returning next state */            \
    virtual HSM::Base* HandleEvent(const typename HSM::_Event& event,     \
                                   typename HSM::_Context ctx);             \
  }

/* HANDLE_EVENT - Defines the event handler implementation for a state
 *
 * Parameters:
 *   HSM  - The top-level state (HSM) type
 *   Name - Name of the state class to handle events for
 *
 * Usage:
 *   HANDLE_EVENT(Root, A) {
 *     return Switch(event,
 *       [&](EvFoo) { return stay(); },
 *       [&](EvBar) { return B::make(); });
 *   }
 */
#define HANDLE_EVENT(HSM, Name)                                            \
  HSM::Base* Name::HandleEvent(const typename Name::Parent::_Event& event, \
                               typename Name::Parent::_Context ctx)

#endif  // STATECHART_H_
