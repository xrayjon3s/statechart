// Copyright (c) 2026 Chris Leger
// Licensed under the MIT License

// traffic_light.cc - Tutorial example: Traffic light controller
//
// This demonstrates:
// - Hierarchical state machines
// - Time-based transitions using HEARTBEAT events
// - Context modification (WALK button extends red duration)
// - Event data (current time in HEARTBEAT)

#include "statechart.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <variant>

// ============================================================================
// Events
// ============================================================================

// HEARTBEAT - dispatched every second with current time
struct Heartbeat {
  int current_time_seconds;
};

// WALK - user pressed walk button (extends ALL_RED duration)
struct Walk {};

// ============================================================================
// Context - Extended state for the state machine
// ============================================================================

struct Context {
  // Display output
  std::string ns_light;   // "GREEN", "YELLOW", "RED"
  std::string ew_light;  // "GREEN", "YELLOW", "RED"

  // Timing
  int current_time = 0;        // Current time in seconds
  int state_end_time = 0;     // When current state should end
  int red_extension = 0;      // Extra seconds to add to ALL_RED (from WALK)

  // Light durations (in seconds)
  static constexpr int GREEN_DURATION = 5;
  static constexpr int YELLOW_DURATION = 1;
  static constexpr int ALL_RED_DURATION = 2;
  static constexpr int WALK_RED_EXTENSION = 10;
};

// ============================================================================
// State Machine Definition
// ============================================================================

// Event type
using Event = std::variant<Heartbeat, Walk>;

// State hierarchy:
// TrafficLight (root)
//   ├── NS_Green    (NS: GREEN, EW: RED)
//   ├── NS_Yellow   (NS: YELLOW, EW: RED)
//   ├── NS_AllRed   (NS: RED, EW: RED) - this gets extended by WALK
//   ├── EW_Green    (EW: GREEN, NS: RED)
//   └── EW_Yellow   (EW: YELLOW, NS: RED)
//   └── EW_AllRed   (NS: RED, EW: RED) - this gets extended by WALK

STATECHART(TrafficLight, Event, Context*);
STATE(TrafficLight, NS_Green, TrafficLight);
STATE(TrafficLight, NS_Yellow, TrafficLight);
STATE(TrafficLight, NS_AllRed, TrafficLight);
STATE(TrafficLight, EW_Green, TrafficLight);
STATE(TrafficLight, EW_Yellow, TrafficLight);
STATE(TrafficLight, EW_AllRed, TrafficLight);

// ============================================================================
// Entry/Exit Actions - Update the display
// ============================================================================

void TrafficLight::Enter(Context* ctx) {
  ctx->ns_light = "RED";
  ctx->ew_light = "RED";
}

void TrafficLight::Exit(Context*) {}

void NS_Green::Enter(Context* ctx) {
  ctx->ns_light = "GREEN";
  ctx->ew_light = "RED";
  ctx->state_end_time = ctx->current_time + Context::GREEN_DURATION;
  std::cout << "[" << ctx->current_time << "s] NS: GREEN, EW: RED" << std::endl;
}

void NS_Green::Exit(Context*) {}

void NS_Yellow::Enter(Context* ctx) {
  ctx->ns_light = "YELLOW";
  ctx->ew_light = "RED";
  ctx->state_end_time = ctx->current_time + Context::YELLOW_DURATION;
  std::cout << "[" << ctx->current_time << "s] NS: YELLOW, EW: RED" << std::endl;
}

void NS_Yellow::Exit(Context*) {}

// ALL_RED - this is where WALK extends the duration
void NS_AllRed::Enter(Context* ctx) {
  ctx->ns_light = "RED";
  ctx->ew_light = "RED";
  // Apply red extension if WALK was pressed
  int extension = ctx->red_extension;
  ctx->red_extension = 0;
  ctx->state_end_time = ctx->current_time + Context::ALL_RED_DURATION + extension;
  std::cout << "[" << ctx->current_time << "s] NS: RED, EW: RED";
  if (extension > 0) {
    std::cout << " (WALK: +" << extension << "s)";
  }
  std::cout << std::endl;
}

void NS_AllRed::Exit(Context*) {}

void EW_Green::Enter(Context* ctx) {
  ctx->ns_light = "RED";
  ctx->ew_light = "GREEN";
  ctx->state_end_time = ctx->current_time + Context::GREEN_DURATION;
  std::cout << "[" << ctx->current_time << "s] NS: RED, EW: GREEN" << std::endl;
}

void EW_Green::Exit(Context*) {}

void EW_Yellow::Enter(Context* ctx) {
  ctx->ns_light = "RED";
  ctx->ew_light = "YELLOW";
  ctx->state_end_time = ctx->current_time + Context::YELLOW_DURATION;
  std::cout << "[" << ctx->current_time << "s] NS: RED, EW: YELLOW" << std::endl;
}

void EW_Yellow::Exit(Context*) {}

// ALL_RED - this is where WALK extends the duration
void EW_AllRed::Enter(Context* ctx) {
  ctx->ns_light = "RED";
  ctx->ew_light = "RED";
  // Apply red extension if WALK was pressed
  int extension = ctx->red_extension;
  ctx->red_extension = 0;
  ctx->state_end_time = ctx->current_time + Context::ALL_RED_DURATION + extension;
  std::cout << "[" << ctx->current_time << "s] NS: RED, EW: RED";
  if (extension > 0) {
    std::cout << " (WALK: +" << extension << "s)";
  }
  std::cout << std::endl;
}

void EW_AllRed::Exit(Context*) {}

// ============================================================================
// Event Handlers
// ============================================================================

// NS_Green: timeout -> NS_Yellow
HANDLE_EVENT(TrafficLight, NS_Green) {
  return Switch(event,
    [&](const Heartbeat& hb) {
      if (hb.current_time_seconds >= ctx->state_end_time) {
        return NS_Yellow::make();
      }
      return stay();
    },
    [&](const Walk&) {
      // Defer until we're in ALL_RED state
      return defer(event, ctx);
    });
}

// NS_Yellow: timeout -> NS_AllRed
HANDLE_EVENT(TrafficLight, NS_Yellow) {
  return Switch(event,
    [&](const Heartbeat& hb) {
      if (hb.current_time_seconds >= ctx->state_end_time) {
        return NS_AllRed::make();
      }
      return stay();
    },
    [&](const Walk&) {
      return defer(event, ctx);
    });
}

// NS_AllRed: timeout -> EW_Green
HANDLE_EVENT(TrafficLight, NS_AllRed) {
  return Switch(event,
    [&](const Heartbeat& hb) {
      if (hb.current_time_seconds >= ctx->state_end_time) {
        return EW_Green::make();
      }
      return stay();
    },
    [&](const Walk&) {
      return defer(event, ctx);
    });
}

// EW_Green: timeout -> EW_Yellow
HANDLE_EVENT(TrafficLight, EW_Green) {
  return Switch(event,
    [&](const Heartbeat& hb) {
      if (hb.current_time_seconds >= ctx->state_end_time) {
        return EW_Yellow::make();
      }
      return stay();
    },
    [&](const Walk&) {
      return defer(event, ctx);
    });
}

// EW_Yellow: timeout -> EW_AllRed
HANDLE_EVENT(TrafficLight, EW_Yellow) {
  return Switch(event,
    [&](const Heartbeat& hb) {
      if (hb.current_time_seconds >= ctx->state_end_time) {
        return EW_AllRed::make();
      }
      return stay();
    },
    [&](const Walk&) {
      return defer(event, ctx);
    });
}

// EW_AllRed: timeout -> NS_Green
HANDLE_EVENT(TrafficLight, EW_AllRed) {
  return Switch(event,
    [&](const Heartbeat& hb) {
      if (hb.current_time_seconds >= ctx->state_end_time) {
        return NS_Green::make();
      }
      return stay();
    },
    [&](const Walk&) {
      return defer(event, ctx);
    });
}

// Root handler: handle WALK here when it bubbles up
HANDLE_EVENT(TrafficLight, TrafficLight) {
  return Switch(event,
    [&](const Walk&) {
      // Set extension to be applied in next ALL_RED state
      ctx->red_extension = Context::WALK_RED_EXTENSION;
      std::cout << "[" << ctx->current_time << "s] WALK pressed! Next ALL_RED extended to "
                << Context::WALK_RED_EXTENSION << "s" << std::endl;
      return stay();
    },
    [&](const Heartbeat&) {
      return defer(event, ctx);
    });
}

// ============================================================================
// Main - Run the simulation
// ============================================================================

int main() {
  Context ctx;

  // Start in NS_Green state
  TrafficLight* state = TrafficLight::Start(NS_Green::make(), &ctx);

  std::cout << "\n=== Traffic Light Simulation ===\n";
  std::cout << "Press Ctrl+C to stop, or press WALK to extend red light\n\n";

  // Simulate heartbeat every second
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ctx.current_time++;

    // Dispatch heartbeat
    state = state->Dispatch(Heartbeat{ctx.current_time}, &ctx);
  }

  return 0;
}
