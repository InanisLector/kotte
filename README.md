# Bomberman World - Daisy chain

**Student:** Kuznietsov Illia

# Compilation
Since I have replaced raylib with submodules for it to be compilable on linux, you have to clone submodules first
#### If you haven't cloned yet:
- git clone --recurse-submodules https://github.com/InanisLector/kotte.git
#### If you have:
- git submodule update --init --recursive

##### Then just use cmake to compile it
Linux -> Linux
``````
cmake --preset linux-release && cmake --build --preset linux-release
``````

Windows -> Windows (VS generator is multi-config: configure once, pick config at build time)
``````
cmake --preset windows-msvc
cmake --build --preset windows-release
``````

Linux -> Windows (sudo apt install mingw-w64)
``````
cmake --preset linux-to-windows-release && cmake --build --preset linux-to-windows-release
``````

## How to play

- **Controls:** movement WASD, lit bombs Space, unlit bombs R, restart Space in menu, AD to navigate menu
- **Objective and rules:** To win you need to clear all the enemies and then go into the Gate (right-bottom corner). Winning takes priority. If you kill multiple enemies with a chain - you get more unlit bombs to use.
- **Configurations / demo setup:** nothing like that

## Features and validation

| Feature | Check, observed result, and expected behaviour |
| --- | --- |
| 1. Player damage and death | I checked both possible damage sources and the delay between damage ticks. All works as expected. |
| 2. Round states and objective | I if you both lose and win on the same frame - losing is prioritised, as expected. |
| 3. Safe restart | After restart, all used lists get cleared in an appropriate phase of the run cycle, without refering to something that doesn't exist anymore. |
| 4. Solid bombs with owner escape | Bombs let the owner escape, before preventing morement. If placement would result in bomb sharing a space with another entity - it's aborted. |
| 5. Blast-range pickup | Default blast range is three, but it increases with 1 by every pick up. Though pick ups are limited by how many spawn, so it's fine. |
| 6. Multiple bombs per owner | Because unlit bombs, that have been lit in chain, are no longer "unlit" - they do not count towards capacity. It's possible to fix that with an additional bool, but I do like this variation more. |
| 7. Chain reactions | Player can spawn as many (at the moment of this note) unlimited unfused bombs, but only one lit, to create chain reactions. Bomb's range is still defined by the range at the moment it was placed |
| 10. Exit or progression gate | Player does not interact with the gate, before all the enemies are cleared and does after they are, regardless of if the player moved or not at the moment of their opening. Win/loss priority remains correct. |
| 11. Scoring and combos | I integrated combo system. You max number of unlit bombs you can place is counted by 3 + [max number enemies killed with a single lit bomb (a.k.a includes chained killes)], which encourages a strategic gameplay |

## Sources and assistance

This project builds on the shared Kotte foundations.

All has been done by myself.

## Architectural investigation - VG only

**Question:** Does routing player death through an event in `[Application]` provide any decoupling compared to a direct method call?

### Decision

Currently, player death is handled with an event: `[OnDeath]` is declared in `[Entity.h:44]`, invoked in `[Entity.cpp:24]`, subscribed to in `[Application.cpp:737]`, and handled by a lambda defined `[Application.cpp:737-740]`, which executes all the actions associated with player's death.

The plausible alternative would be direct management from `[Application]`. The only spot the code would have to check would be `[Application.cpp:192]`, where I could check if health is at zero and do the corresponding actions from lambda function on the spot.

I chose the event because it lets the object that owns the state announce changes without outsiders checking it, which keeps state checks encapsulated and reduces context size required to read the code.

### Method

1. Located every part of the death flow: condition, invoke, subscription, handler, and effect. (listed in Decision)
2. Counted the classes involved in the flow.
3. Estimated the cost of one change request, (e.g. play a sound), under both designs.

### Evidence

| Measure | Event callback | Direct check | Observation |
| --- | --- | --- | --- |
| Stack traces | `[Application::run]` -> `[Application::run_frame]` -> `[Application::resolve_gameplay_facts]` -> `[PlayerData::apply_update]` -> `[Event::invoke]` -> `[Application::[]]` | `[Application::run]` -> `[Application::run_frame]` -> `[Application::resolve_gameplay_facts]` -> `[Application::[]]` | When excluding the shared stack paths, stack trace is much shorter with direct check (2 added vs 0 added) |
| Classes executing logic | `[Application]`, `[PlayerData]`, `[Event]` | `[Application]` | Event path abstracts part of the logic out into `[PlayerData]` with `[Event]` class acting as glue |
| Estimation to add a playing sound on death | one line in the lambda on `[Application.cpp:737]` | one line in the code block on `[Application.cpp:192]` | the logic to be executed by the `[Application]` remains inside that class, so cost of adding new logic is the same, as long as `[Application]` is the only consumer of the event |

### Interpretation

The event has one raise site `[Entity.cpp:24]` (only ever called from `[Application::192]`) and one handler `[Application.cpp:737]`, registered once at startup. The stack trace is therefore linear and fully traceable: there is no fan-out, no fan-in, and no runtime change in subscribers. With this one-to-one wiring, the event behaves like an indirect direct call. It does remove the entity's knowledge of who reacts, but since the only subscriber is `[Application]`, which already owns every effect, no reaction logic can move out of it.
The pattern's purpose is to decouple a publisher from the independent subscribers, and no such separation exists here.
Higher degree of responsibility splitting (e.g. more peer-level managers interacting directly with one another instead of through one higher-level manager) would create independent objects to be decoupled.

Because there is one raise site and one handler, ordering between handlers and re-entrancy cannot occur here, and Application is indifferent to call order anyway, so the usual costs of events are absent.
The trade-off is in extensibility: a direct check at `[Application.cpp:192]` is sufficient only while health changes in one place. A second caller or a second reaction would need its own check, whereas the event keeps the check inside the entity and lets reactions be added by subscribing.

### Conclusion

With god class architecture, a direct call is simpler to build something short term, since all the information is already available to the god class, but a lot less appropriate for expansion.
Events become worthwhile once separate classes are large and contained enough to hide information, and have at least 2 independent subscribers (e.g. score system, audio system, game-state system) react to them.
The trigger for refactoring would be splitting responsibilities out of `Application`.
Before that - using observer pattern only provides decoupling exclusively on the publisher's side.

I kept the event for death because it costs little and documents intent, but I did not make further conversion, since the evidence shows no benefit on subscriber side, until responsibilities are separated.
