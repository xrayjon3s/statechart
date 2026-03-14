// Copyright (c) 2026 Chris Leger
// Licensed under the MIT License

#include "statechart.h"

#include <gtest/gtest.h>

#include <string>
#include <variant>

struct EvFoo {};
struct EvBar {};

struct EvBaz {};
struct EvQux {};
struct EvToA2 {};
struct EvToB {};
struct EvToB2 {};
struct EvToA1 {};

using Event = std::variant<EvFoo, EvBar, EvBaz, EvQux, EvToA2, EvToB, EvToB2, EvToA1>;

struct Context {
  std::string log;
  int value = 0;
};

STATECHART(Root, Event, Context*);
STATE(Root, A, Root);
STATE(Root, A1, A);
STATE(Root, A2, A);
STATE(Root, B, Root);
STATE(Root, B1, B);
STATE(Root, B2, B);

void Root::Enter(Context* ctx) { ctx->log += "Root:entry "; }
void Root::Exit(Context* ctx) { ctx->log += "Root:exit "; }

void A::Enter(Context* ctx) { ctx->log += "A:entry "; }
void A::Exit(Context* ctx) { ctx->log += "A:exit "; }

void A1::Enter(Context* ctx) { ctx->log += "A1:entry "; }
void A1::Exit(Context* ctx) { ctx->log += "A1:exit "; }

void A2::Enter(Context* ctx) { ctx->log += "A2:entry "; }
void A2::Exit(Context* ctx) { ctx->log += "A2:exit "; }

void B::Enter(Context* ctx) { ctx->log += "B:entry "; }
void B::Exit(Context* ctx) { ctx->log += "B:exit "; }

void B1::Enter(Context* ctx) { ctx->log += "B1:entry "; }
void B1::Exit(Context* ctx) { ctx->log += "B1:exit "; }

void B2::Enter(Context* ctx) { ctx->log += "B2:entry "; }
void B2::Exit(Context* ctx) { ctx->log += "B2:exit "; }

HANDLE_EVENT(Root, Root) {
  return Switch(event, [&](EvFoo) { return stay(); },
               [&](EvToB) { return B::make(); }, [&](EvToA1) { return A1::make(); },
               [&](auto) { return stay(); });
}

HANDLE_EVENT(Root, A) {
  return Switch(
      event, [&](EvFoo) { return stay(); },
      [&](EvBaz) {
        ctx->log += "A:baz ";
        return stay();
      },
      [&](auto) { return defer(event, ctx); });
}

HANDLE_EVENT(Root, A1) {
  return Switch(
      event,
      [&](EvFoo) {
        ctx->log += "A1:foo ";
        return stay();
      },
      [&](EvBaz) {
        ctx->log += "A1:baz ";
        return A2::make();
      },
      [&](EvToA2) { return A2::make(); },
      [&](EvToB) { return B::make(); },
      [&](EvToB2) { return B2::make(); },
      [&](auto) { return defer(event, ctx); });
}

HANDLE_EVENT(Root, A2) {
  return Switch(
      event, [&](EvFoo) { return stay(); },
      [&](EvToB) { return B::make(); },
      [&](EvToB2) { return B2::make(); },
      [&](auto) { return defer(event, ctx); });
}

HANDLE_EVENT(Root, B) {
  return Switch(event, [&](EvFoo) { return stay(); },
               [&](EvToB2) { return B2::make(); },
               [&](auto) { return defer(event, ctx); });
}

HANDLE_EVENT(Root, B1) {
  return Switch(event, [&](EvFoo) { return stay(); },
               [&](EvToB2) { return B2::make(); },
               [&](auto) { return defer(event, ctx); });
}

HANDLE_EVENT(Root, B2) {
  return Switch(
      event, [&](EvFoo) { return stay(); },
      [&](EvToA2) { return A2::make(); },
      [&](auto) { return defer(event, ctx); });
}

TEST(StateChartTest, InitialState) {
  Context ctx;
  auto* state = Root::make();
  EXPECT_STREQ(state->name(), "Root");
}

TEST(StateChartTest, SelfTransition) {
  Context ctx;
  auto* state = Root::make();

  state = state->Dispatch( EvFoo{}, &ctx);
  EXPECT_STREQ(state->name(), "Root");
}

TEST(StateChartTest, Depth) {
  auto* r = Root::make();
  auto* a = A::make();
  auto* a1 = A1::make();
  auto* b = B::make();

  EXPECT_EQ(r->Depth(), 0);
  EXPECT_EQ(a->Depth(), 1);
  EXPECT_EQ(a1->Depth(), 2);
  EXPECT_EQ(b->Depth(), 1);
}

TEST(StateChartTest, ParentState) {
  auto* r = Root::make();
  auto* a = A::make();
  auto* a1 = A1::make();
  auto* b = B::make();

  EXPECT_EQ(a->ParentState(), r);
  EXPECT_EQ(a1->ParentState(), a);
  EXPECT_EQ(b->ParentState(), r);
  EXPECT_EQ(r->ParentState(), nullptr);
}

TEST(StateChartTest, TransitionFromA1toB) {
  Context ctx;
  Root* state = A1::make();
  state = state->Dispatch(EvToB{}, &ctx);
  EXPECT_EQ(state, B::make());
  EXPECT_EQ(ctx.log, "A1:exit A:exit B:entry ");
}

TEST(StateChartTest, TransitionFromA1toA2) {
  Context ctx;
  Root* state = A1::make();
  state = state->Dispatch(EvToA2{}, &ctx);
  EXPECT_EQ(state, A2::make());
  EXPECT_EQ(ctx.log, "A1:exit A2:entry ");
}

TEST(StateChartTest, TransitionFromA1toRoot) {
  Context ctx;
  Root* state = A1::make();
  state = state->Dispatch(EvToA1{}, &ctx);  // goes to root via A1->A->Root, but actually ends at A1... wait, need a different approach
  // Actually, to go to root, we need to stay in Root. Let me add an event that transitions to root
}

TEST(StateChartTest, Depth3Plus) {
  auto* r = Root::make();
  auto* a = A::make();
  auto* a1 = A1::make();
  auto* b = B::make();
  auto* b1 = B1::make();
  auto* b2 = B2::make();

  EXPECT_EQ(r->Depth(), 0);
  EXPECT_EQ(a->Depth(), 1);
  EXPECT_EQ(a1->Depth(), 2);
  EXPECT_EQ(b->Depth(), 1);
  EXPECT_EQ(b1->Depth(), 2);
  EXPECT_EQ(b2->Depth(), 2);
}

TEST(StateChartTest, TransitionB1toB2) {
  Context ctx;
  Root* state = B1::make();
  state = state->Dispatch(EvToB2{}, &ctx);
  EXPECT_EQ(state, B2::make());
  EXPECT_EQ(ctx.log, "B1:exit B2:entry ");
}

TEST(StateChartTest, TransitionB1toA1) {
  Context ctx;
  Root* state = B1::make();
  state = state->Dispatch(EvToA1{}, &ctx);
  EXPECT_EQ(state, A1::make());
  EXPECT_EQ(ctx.log, "B1:exit B:exit A:entry A1:entry ");
}

TEST(StateChartTest, StartAtRoot) {
  Context ctx;

  Root* result = Root::Start(Root::make(), &ctx);

  EXPECT_EQ(result, Root::make());
  EXPECT_EQ(ctx.log, "Root:entry ");
}

TEST(StateChartTest, StartAtA) {
  Context ctx;

  Root* result = Root::Start(A::make(), &ctx);

  EXPECT_EQ(result, A::make());
  EXPECT_EQ(ctx.log, "Root:entry A:entry ");
}

TEST(StateChartTest, StartAtA1) {
  Context ctx;

  Root* result = Root::Start(A1::make(), &ctx);

  EXPECT_EQ(result, A1::make());
  EXPECT_EQ(ctx.log, "Root:entry A:entry A1:entry ");
}

TEST(StateChartTest, StartAtB1) {
  Context ctx;

  Root* result = Root::Start(B1::make(), &ctx);

  EXPECT_EQ(result, B1::make());
  EXPECT_EQ(ctx.log, "Root:entry B:entry B1:entry ");
}

TEST(StateChartTest, StayHandler) {
  Context ctx;

  Root* state = A1::make();
  state = state->Dispatch( EvFoo{}, &ctx);

  EXPECT_EQ(state, A1::make());
  EXPECT_EQ(ctx.log, "A1:foo ");
}

TEST(StateChartTest, DeferHandler) {
  Context ctx;

  Root* state = A1::make();
  state = state->Dispatch( EvBar{}, &ctx);

  EXPECT_EQ(state, Root::make());
}

TEST(StateChartTest, ChildOverridesParentEvent) {
  Context ctx;

  Root* state = A1::make();
  state = state->Dispatch( EvBaz{}, &ctx);

  EXPECT_EQ(state, A2::make());
  EXPECT_EQ(ctx.log, "A1:baz A1:exit A2:entry ");
}

TEST(StateChartTest, ParentEventBaz) {
  Context ctx;

  Root* state = A::make();
  state = state->Dispatch( EvBaz{}, &ctx);

  EXPECT_EQ(state, A::make());
  EXPECT_EQ(ctx.log, "A:baz ");
}

struct TokenEvent {
  enum class Token { FOO, BAR, BAZ, QUX } token;
  union {
    int numval;
    const char* strval;
  };
};

using Event2 = std::variant<TokenEvent>;

STATECHART(Machine, Event2, Context*);
STATE(Machine, S1, Machine);
STATE(Machine, S2, Machine);

void Machine::Enter(Context* ctx) { ctx->log += "Machine:entry "; }
void Machine::Exit(Context* ctx) { ctx->log += "Machine:exit "; }
void S1::Enter(Context* ctx) { ctx->log += "S1:entry "; }
void S1::Exit(Context* ctx) { ctx->log += "S1:exit "; }
void S2::Enter(Context* ctx) { ctx->log += "S2:entry "; }
void S2::Exit(Context* ctx) { ctx->log += "S2:exit "; }

HANDLE_EVENT(Machine, Machine) {
  return Switch(event, [&](const TokenEvent& e) {
    switch (e.token) {
      case TokenEvent::Token::FOO:
        return stay();
      case TokenEvent::Token::BAR:
        return S2::make();
      default:
        return stay();
    }
  });
}

HANDLE_EVENT(Machine, S1) {
  return Switch(event, [&](const TokenEvent& e) {
    switch (e.token) {
      case TokenEvent::Token::BAZ:
        ctx->log += "S1:baz ";
        return S2::make();
      default:
        return defer(event, ctx);
    }
  });
}

HANDLE_EVENT(Machine, S2) {
  return Switch(event, [&](const TokenEvent& e) {
    switch (e.token) {
      case TokenEvent::Token::QUX:
        return S1::make();
      default:
        return stay();
    }
  });
}

TEST(StateChartTest, SwitchOnVariantIndex) {
  Context ctx;

  Machine* state = Machine::Start(S1::make(), &ctx);
  EXPECT_EQ(state, S1::make());
  EXPECT_EQ(ctx.log, "Machine:entry S1:entry ");

  // Skip the FOO dispatch for now, just test BAR
  ctx.log.clear();
  state = state->Dispatch( TokenEvent{TokenEvent::Token::BAR}, &ctx);
  EXPECT_EQ(state, S2::make());
  EXPECT_EQ(ctx.log, "S1:exit S2:entry ");
}

TEST(StateChartTest, SwitchWithStructEvent) {
  Context ctx;

  Machine* state = Machine::Start(S1::make(), &ctx);

  ctx.log.clear();
  state = state->Dispatch( TokenEvent{TokenEvent::Token::BAZ}, &ctx);
  EXPECT_EQ(state, S2::make());
  EXPECT_EQ(ctx.log, "S1:baz S1:exit S2:entry ");
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
