# Statechart

Copyright (c) 2026 Chris Leger - Licensed under the MIT License

A header-only C++20 statechart framework using templates and macros.

## Overview

This is a modern implementation of statecharts based on David Harel's seminal 1987 paper:

> Harel, D. (1987). [Statecharts: A Visual Formalism for Complex Systems](https://doi.org/10.1016/0167-6423(87)90035-9). Science of Computer Programming, 8(3), 231-274.

For XML-based statechart references, see the [W3C SCXML Specification](https://www.w3.org/TR/scxml/).

## Features

- Header-only, single-file library
- Hierarchical state machines with deep inheritance
- Automatic entry/exit actions via LCA (Least Common Ancestor) algorithm
- Clean API: only 4 public static methods needed for most use cases

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

### Internal (Protected)

- `Transition()`, `EnterState()`, `ExitState()` - Called by Dispatch/Start

## Quick Example

```cpp
#include "statechart.h"
#include <variant>

// Define events
struct EvFoo {};
struct EvBar {};
using Event = std::variant<EvFoo, EvBar>;

// Define extended state (context)
struct Context {
  std::string log;
};

// Define state machine
STATECHART(Root, Event, Context*);
STATE(Root, A, Root);
STATE(Root, B, Root);

// Define entry/exit actions (protected - define in .cpp or friend class)
void Root::Enter(Context* ctx) { /* ... */ }
void Root::Exit(Context* ctx) { /* ... */ }
void A::Enter(Context* ctx) { /* ... */ }
void A::Exit(Context* ctx) { /* ... */ }

// Define event handlers
HANDLE_EVENT(Root, Root) {
  return Switch(event, [&](EvFoo) { return stay(); },
               [&](auto) { return stay(); });
}

// Use it
Context ctx;
Root* state = Root::Start(A::make(), &ctx);
state = state->Dispatch(EvFoo{}, &ctx);
```

For more examples, see `statechart_test.cc`.

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
- WALK button extends red light duration to 60 seconds
- HEARTBEAT event every second for timing

```bash
make traffic_light
./build/traffic_light
```

## License

MIT License - See LICENSE file.
