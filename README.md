# Statechart

Copyright (c) 2026 Chris Leger - Licensed under the MIT License

A single-file, header-only C++20 statechart framework.

## Overview

This is a modern implementation of statecharts based on David Harel's seminal 1987 paper:

> Harel, D. (1987). [Statecharts: A Visual Formalism for Complex Systems](https://doi.org/10.1016/0167-6423(87)90035-9). Science of Computer Programming, 8(3), 231-274.

Statecharts, also known as Hierarchical State Machines, are a type of finite state machine that can have nested states.  These are very 
useful in event-driven system that need to respond differently to events based on internal state.  For example, a driver
may need to power on a remote device and walk through a configuration sequence when a request comes, but if the device
is already on and configured, the power on and configuration may be skipped.  The device may report an error at any point, 
indicating that an error response might be needed and requests would need to be dropped.  Thus different behavior to
events (request) is needed based on the state (off, on, error).  Within an operating state, there may be many substates 
depending on whether the device is busy or idle; these would be substates of 'on' and might need different response as well.

You can express the same behavior with a standard finite state machine, but it requires many more states, resulting in code 
that is far larger and harder to maintain and debug.  The power of a Statechart is that sub-states inherit all behavior
from their parent state except whatever difference in response is needed.  This is similar to a deep class hierarchy
where a derived class only needs to define what is different from its parent class (programming by difference).

While it is possible to fully write statecharts by hand, it is difficult to consistently enforce entry/exit actions and 
deferring of events states to parents.  This library makes statecharts more compact and correct through lots of syntactic sugar.

For XML-based statechart references, see the [W3C SCXML Specification](https://www.w3.org/TR/scxml/).

## Features

- Header-only, single-file library
- Hierarchical state machines with deep inheritance
- Clean API: only 4 public static methods needed for most use cases
- Allows extended state variables of any user-defined type to hold application state and 
  act on it.

The API is intended to be simple, gauruntee event dispatch and state transitions (including entry/action actions), 
and allow for compact code without lengthy C++ type definitions.  It does this through macros that define states and 
instantiate all helper functions.

To define a new state chart use the STATECHART macro.  Child states are defined using the STATE macro.  For each child state, 
you must implement three methods:

- `virtual void Enter(Context)` - state entry action.  Body may be empty if no action is needed.
- `virtual void Exit(Context)` - state exit action.  Body may be empty if no action is needed.
- `HANDLE_EVENT(StateChart, State) - handler for events.

The statechart states are pointers to static class objects, whose inheritance hierarchy matches the topology of the state machine.  
There is no data in the class objects, so they can always be const.

The events handled by the state machine can be any type, as long as they capture both the event type and any data associated with
the event (i.e. a message).  std::variant is recommended for safety, in which case the Switch() member can be used.  
But a C-style switch() statement can also be used with a struct or union type, and token identifying the state.  This, however, is
more error prone since the user must ensure that the token always correctly indicates the type of message.  Thus, std::variant
is recommended.  

## API

### Public Methods

- `static Base* make()` - Returns singleton instance of a state
- `static Base* Start(Base* initial, _Context ctx)` - Initialize state machine
- `Base* Dispatch(const _Event& event, _Context ctx)` - Handle event, returns new state
- `static Base* defer(const _Event& event, _Context ctx)` - Delegate to parent state

### Protected Methods (for use in event handlers)

- `virtual Base* stay()` - Stay in current state
- `virtual Base* HandleEvent(...)` - Override to handle events
- `virtual void Enter(_Context ctx)` - Override for entry action
- `virtual void Exit(_Context ctx)` - Override for exit action
- `template<typename... Fs> static Base* Switch(...)` - Helper for std::visit

## Quick Example

```cpp
#include "statechart.h"
#include <variant>

// Define events

// An event with a string message.  
struct EvFoo { std::string message; }; 

// An pure event (no data)
struct EvBar { };   // Go to B from anywhere

// The Event type encompassing all events.  
using Event = std::variant<EvFoo, EvBar>;

// Define extended state (context)
struct Context {
  std::string log;
};

// Define state machine
// Hierarchy:
//   Root
//   ├── A
//   │   └── A1
//   └── B
// We want to print 
STATECHART(Root, Event, Context*); // Define state chart with user event and context types
STATE(Root, A, Root);              // Define state A in state chart "Root" with parent "Root"
STATE(Root, A1, A);                // Define state A1 with parent A
STATE(Root, B, Root);              // Define state B with parent Root

// Define entry/exit actions (protected - define in .cpp or friend class)
void Root::Enter(Context* ctx) { ctx->log += "Root:entry "; }
void Root::Exit(Context* ctx) { ctx->log += "Root:exit "; }

// Define event handlers
// Root handles EvFoo (go to A), EvBar (go to B)
HANDLE_EVENT(Root, Root) {
  return Switch(event,
    // EvFoo goes to A
    [&](const EvFoo &a) { ctx->log += a.message; return A::make(); },
    // EvBar goes to B from all states
    [&](EvBar) { return B::make(); },
    [&](auto) { return stay(); });
}

void A::Enter(Context* ctx) { ctx->log += "A:entry "; }
void A::Exit(Context* ctx) { ctx->log += "A:exit "; }

// A overrides EvFoo (go to A1 if in A)
HANDLE_EVENT(Root, A) {
  return Switch(event,
    [&](EvFoo) { ctx->log += a.message; return A1::make(); },
    [&](auto) { return defer(event, ctx); });
}

void A1::Enter(Context* ctx) { ctx->log += "A1:entry "; }
void A1::Exit(Context* ctx) { ctx->log += "A1:exit "; }

// A1 handles EvFoo (stay in A1, don't append to message).
HANDLE_EVENT(Root, A1) {
  return Switch(event,
    [&](EvFoo) { return stay(); },
    [&](auto) { return defer(event, ctx); });
}

void B::Enter(Context* ctx) { ctx->log += "B:entry "; }
void B::Exit(Context* ctx) { ctx->log += "B:exit "; }

// B inerits responses
HANDLE_EVENT(Root, B) {
  return Switch(event,
    [&](auto) { return defer(event, ctx); });
}

// Use it
Context ctx;
Root* state = Root::Start(B::make(), &ctx);  // Start in B
state = state->Dispatch(EvFoo{"hello"}, &ctx);       // Transition to A, append "hello"
state = state->Dispatch(EvFoo{"world"}, &ctx);       // Transition to A1, append "world"
state = state->Dispatch(EvFoo{"silent"}, &ctx);      // Stay in A1, don't modify log
state = state->Dispatch(EvBar{}, &ctx);              // Transition to B
```

For more examples, see `statechart_test.cpp` and `traffic_light.cpp`

## Building

```bash
# Using the Makefile (recommended)
make              # Build
make test         # Build and run tests
make clean        # Clean build artifacts

# Or using CMake directly
mkdir build && cd build
cmake ..
make
./statechart_test
```

## Tutorial Example

See `traffic_light.cc` for a complete example of a traffic light controller:

- Two directions (North-South, East-West)
- Green → Yellow → Red cycle
- WALK button extends red light duration to 10 seconds
- HEARTBEAT event every second for timing

```bash
make traffic_light
./build/traffic_light
```

## License

MIT License - See LICENSE file.
