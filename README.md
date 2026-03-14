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
- Support for defer(), stay(), and none() transitions
- Entry/Exit virtual methods for each state

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

// Define entry/exit actions
void Root::Enter(Context* ctx) { /* ... */ }
void Root::Exit(Context* ctx) { /* ... */ }
void A::Enter(Context* ctx) { /* ... */ }
void A::Exit(Context* ctx) { /* ... */ }

// Define event handlers
HANDLE_EVENT(Root, Root) {
  return Root::Switch(event, [&](EvFoo) { return stay(); },
                      [&](auto) { return stay(); });
}

// Use it
Context ctx;
Root* state = Root::make();
state = Root::Dispatch(state, EvFoo{}, &ctx);
```

For more examples, see `statechart_test.cc`.

## Building

```bash
mkdir build && cd build
cmake ..
make
./statechart_test
```

## License

MIT License - See LICENSE file.
