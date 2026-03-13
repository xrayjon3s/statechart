#include <gtest/gtest.h>
#include "statechart.h"
#include <string>
#include <variant>

struct EvFoo {};
struct EvBar {};

using Event = std::variant<statechart::None, EvFoo, EvBar>;

struct Context {
  std::string log;
  int value = 0;
};

STATECHART(Root, Event, Context*);
STATE(Root, A, Root);
STATE(Root, A1, A);
STATE(Root, A2, A);
STATE(Root, B, Root);

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

HANDLE_EVENT(Root, Root) {
  return std::visit(statechart::Overloaded{
    [&](EvFoo) { return stay(); },
    [&](auto) { return stay(); }
  }, event);
}

HANDLE_EVENT(Root, A) {
  return std::visit(statechart::Overloaded{
    [&](EvFoo) { return stay(); },
    [&](auto) { return defer(event, ctx); }
  }, event);
}

HANDLE_EVENT(Root, A1) {
  return std::visit(statechart::Overloaded{
    [&](EvFoo) { 
      ctx->log += "A1:foo "; 
      return stay(); 
    },
    [&](auto) { return defer(event, ctx); }
  }, event);
}

HANDLE_EVENT(Root, A2) {
  return std::visit(statechart::Overloaded{
    [&](auto) { return defer(event, ctx); }
  }, event);
}

HANDLE_EVENT(Root, B) {
  return std::visit(statechart::Overloaded{
    [&](auto) { return defer(event, ctx); }
  }, event);
}

TEST(StateChartTest, InitialState) {
  Context ctx;
  auto* state = Root::make();
  EXPECT_STREQ(state->name(), "Root");
}

TEST(StateChartTest, SelfTransition) {
  Context ctx;
  auto* state = Root::make();
  
  state = DISPATCH(state, EvFoo{}, &ctx);
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
  
  Root* from = A1::make();
  Root* to = B::make();
  
  Root* result = Root::Transition(from, to, &ctx);
  
  EXPECT_EQ(result, to);
  EXPECT_EQ(ctx.log, "A1:exit A:exit B:entry ");
}

TEST(StateChartTest, TransitionFromA1toA2) {
  Context ctx;
  
  Root* from = A1::make();
  Root* to = A2::make();
  
  Root* result = Root::Transition(from, to, &ctx);
  
  EXPECT_EQ(result, to);
  EXPECT_EQ(ctx.log, "A1:exit A2:entry ");
}

TEST(StateChartTest, TransitionFromA1toRoot) {
  Context ctx;
  
  Root* from = A1::make();
  Root* to = Root::make();
  
  Root* result = Root::Transition(from, to, &ctx);
  
  EXPECT_EQ(result, to);
  EXPECT_EQ(ctx.log, "A1:exit A:exit ");
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
