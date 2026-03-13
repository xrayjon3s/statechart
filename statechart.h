#ifndef STATECHART_H_
#define STATECHART_H_

#include <variant>
#include <memory>
#include <type_traits>
#include <iostream>

namespace statechart {

struct None {};

template <class... Ts>
struct Overloaded : Ts... {
  using Ts::operator()...;
};

template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

}  // namespace statechart

#define STATECHART(Name, _EventType, _ContextType)                                \
class Name {                                                                    \
 public:                                                                        \
  using Base = Name;                                                           \
  using Parent = Name;                                                         \
  using ParentType = Name;                                                     \
  using _Event = _EventType;                                                  \
  using _Context = _ContextType;                                               \
  static Base* make() {                                                        \
    static Name singleton;                                                     \
    return &singleton;                                                        \
  }                                                                           \
  static Base* stay() { return make(); }                                      \
  static Base* none() { return nullptr; }                                     \
  static Base* defer(const _Event& event, _Context ctx) {                     \
    return make()->ParentState()->HandleEvent(event, ctx);                     \
  }                                                                           \
  virtual const char* name() const { return #Name; }                         \
  virtual int Depth() const { return 0; }                                    \
  virtual Base* ParentState() const { return nullptr; }                      \
  virtual void Enter(_Context ctx);                                           \
  virtual void Exit(_Context ctx);                                            \
  virtual Base* HandleEvent(const _Event& event, _Context ctx);              \
  static Base* Transition(Base* from, Base* to, _Context ctx);              \
  static Base* Start(_Context ctx);                                           \
};                                                                          \
static void Name##_EnterState(Name::Base* state, Name::Base* lca, Name::_Context ctx) { \
  if (!state) return;                                                       \
  if (state == lca) return;                                                 \
  Name##_EnterState(static_cast<Name::Base*>(state->ParentState()), lca, ctx); \
  state->Enter(ctx);                                                         \
}                                                                           \
static void Name##_ExitState(Name::Base* state, Name::Base* lca, Name::_Context ctx) { \
  if (!state) return;                                                       \
  if (state == lca) return;                                                 \
  state->Exit(ctx);                                                          \
  Name##_ExitState(static_cast<Name::Base*>(state->ParentState()), lca, ctx);  \
}                                                                           \
Name::Base* Name::Transition(Name::Base* from, Name::Base* to, Name::_Context ctx) { \
  if (!to) return from ? from : Name::make();                               \
  if (!from) { Name##_EnterState(to, nullptr, ctx); return to; }             \
  int fromDepth = from->Depth();                                           \
  int toDepth = to->Depth();                                               \
  Name::Base* fromCopy = from;                                            \
  Name::Base* toCopy = to;                                                \
  while (fromDepth > toDepth) {                                            \
    fromCopy = static_cast<Name::Base*>(fromCopy->ParentState());            \
    fromDepth--;                                                           \
  }                                                                         \
  while (toDepth > fromDepth) {                                            \
    toCopy = static_cast<Name::Base*>(toCopy->ParentState());                \
    toDepth--;                                                             \
  }                                                                         \
  while (fromCopy != toCopy) {                                              \
    fromCopy = static_cast<Name::Base*>(fromCopy->ParentState());            \
    toCopy = static_cast<Name::Base*>(toCopy->ParentState());               \
  }                                                                         \
  Name::Base* lca = fromCopy;                                             \
  Name##_ExitState(from, lca, ctx);                                        \
  Name##_EnterState(to, lca, ctx);                                          \
  return to; }                                                              \
Name::Base* Name::Start(Name::_Context ctx) {                               \
  Name::Base* initial = make()->HandleEvent(statechart::None{}, ctx);            \
  if (initial) { initial->Enter(ctx); }                                    \
  return initial ? initial : Name::make(); }

#define STATE(HSM, Name, Parent)                                               \
class Name : public Parent {                                                   \
 public:                                                                        \
  using Base = Name;                                                           \
  using Parent = Parent;                                                      \
  using ParentType = Parent;                                                  \
  static HSM::Base* make() {                                                 \
    static Name singleton;                                                     \
    return &singleton;                                                        \
  }                                                                           \
  static HSM::Base* stay() { return HSM::make(); }                           \
  static HSM::Base* none() { return nullptr; }                               \
  static HSM::Base* defer(const typename HSM::_Event& event,                 \
                          typename HSM::_Context ctx) {                       \
    return Parent::make()->HandleEvent(event, ctx);                           \
  }                                                                           \
  const char* name() const override { return #Name; }                        \
  int Depth() const override { return Parent::Depth() + 1; }                 \
  HSM::Base* ParentState() const override { return Parent::make(); }          \
  virtual void Enter(typename HSM::_Context ctx);                               \
  virtual void Exit(typename HSM::_Context ctx);                           \
  using Parent::HandleEvent;                                                  \
  virtual HSM::Base* HandleEvent(const typename HSM::_Event& event,         \
                            typename HSM::_Context ctx); }

#define HANDLE_EVENT(HSM, Name) \
HSM::Base* Name::HandleEvent(const typename Name::Parent::_Event& event,   \
                             typename Name::Parent::_Context ctx)

#define DISPATCH(state, event, ctx) \
  (state)->HandleEvent(event, ctx)

#endif  // STATECHART_H_
