/*
 * Copyright (C) 2022 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#pragma once
// There are quite good state machine implementations out there, but since we are avoiding dependencies I took the
// freedom to model one using std::variant. I plan to use this model in other pieces very soon so I made it
// somewhat generic an abstract enough to be composed in multiple situations.
// To make that happen, there is some template magic going on under the internal namespace.

namespace Oobe::internal
{
    /**
     * is_variant_v metafunction holds true if the type argument is a concrete instantiation of std::variant.
     */
    template <typename T> struct is_variant : std::false_type
    { };

    template <typename... Args> struct is_variant<std::variant<Args...>> : std::true_type
    { };

    template <typename T> inline constexpr bool is_variant_v = is_variant<T>::value;

    /**
     * is_variant_of_v holds true if the type argument T is one of the alternative types of the variant V.
     */
    template <typename T, typename U> struct is_variant_of : std::false_type
    { };

    template <typename T, typename... Ts>
    struct is_variant_of<T, std::variant<Ts...>> : public std::disjunction<std::is_same<T, Ts>...>
    { };

    template <typename T, typename V> inline constexpr bool is_variant_of_v = is_variant_of<T, V>::value;

    // The overloaded pattern is amazingly helpful when dealing with variant visitation. It allows grouping lambdas or
    // other callable objects that can receive one alternative type of the variant as an overload set that will be used
    // by std::visit to implement the variant visitation in an exhaustive way, i.e. it fails to compile if some
    // alternative type of the variant is left unhandled.
    // It's usage is like:
    // ```cpp
    // std::visit(
    // overloaded
    // {
    //  <list of callables, each one handling one possible alternative type of the referred variant>
    // }, variant_);
    //
    template <class... Ts> struct overloaded : Ts...
    {
        using Ts::operator()...;
    };
    template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

    /**
     * This state machine implementation builds on the value semantics of std::variant sum type instead of inheritance
     * hierarchies as commonly practiced in object oriented languages.
     * The template mechanisms above just make a bit easier implementing pattern matching visitation of a variant based
     * on the underlying types and compile-time checking of types in the context of std::variant instantiation.
     *
     * In this state machine the state transition logic is implemented in the state classes. Thus, this is a generic
     * component which just connects the pieces, the business logic must be implemented somewhere else, being a subclass
     * or a controller class which has one instance of this. They must provide a member function `ExpectedState
     * on_event(Events event)` which matches against the Events variant returning the appropriate State. Unhandled
     * events will default to `return nonstd::make_unexpected(InvalidTransition{state_, event});`
     *
     * To effectively build upon this state machine class, some conventions must be followed (they are reinforced by the
     * template checking spread around this class):
     *
     * 1. Define an outer context class, enclosing the definition of states, events and holding the state_machine
     *    instantiation.
     * 2. Define state classes grouped in a States class (or struct) for easy discoverability (making dev life a bit
     *    easier).
     * 3. Define State as a type alias for std::variant of the state classes. The first type declared in the variant
     *    argument will be the initial state when the machine is default constructed (if that type is default
     *   constructible).
     * 4. Apply the exact same steps 2 and 3 for the events, i.e. create the struct Events{}, define the possible events
     *    inside it and define the alias `using Event = std::variant<list of all the event classes>...`.
     * 5. Instantiate a state_machine or a subclass deriving from it passing the above mentioned outer scope as a
     *    template type argument.
     *
     * Following those principles turn possible building the API shown in StateMachineTests.cpp. Refer to that file for
     * examples of usage of this state_machine. It's easier than trying to understand everything from this header file
     * only.
     */

    template <
      class Context,
      class State_ = std::enable_if_t<is_variant<typename Context::State>::value, typename Context::State>,
      class Event_ = std::enable_if_t<is_variant<typename Context::Event>::value, typename Context::Event>,
      class States_ = std::enable_if_t<std::is_class<typename Context::States>::value, typename Context::States>,
      class Events_ = std::enable_if_t<std::is_trivial<typename Context::Events>::value, typename Context::Events>>
    class state_machine
    {
      public:
        /// The Event variant - i.e. an alias to std::variant<list of events...>
        using Event = Event_;
        /// The enclosing class of all the event classes - makes easier to discover than from auto-completion.
        using Events = Events_;
        /// The State variant - i.e. an alias to std::variant<list of states...>
        /// The current state of this state_machine is of type `State`.
        using State = State_;
        /// The enclosing class of all the state classes - makes easier to discover than from auto-completion.
        using States = States_;

        /// If an invalid state transition attempt occurs, this caches the current state and the surprising event
        /// received.
        struct InvalidTransition
        {
            State current;
            Event received;
        };

        /// Since not all state transitions will be possible, we model the return value of the functions as an expected.
        using ExpectedState = nonstd::expected<State, InvalidTransition>;

      protected:
        State state_;

        /// Attempts to set current state if the argument is a valid one.
        /// Returns the new state or the argument if it contains an error.
        /// Serves as a post processing call after adding an event.
        ExpectedState trySetState(ExpectedState maybe)
        {
            if (maybe.has_value()) {
                state_ = maybe.value();
                return state_;
            }

            return maybe;
        }

      public:
        /// Returns true if the current state matches the template type argument.
        /// use it like: `if(machine.isCurrentStateA<States::Connecting>()){`
        template <typename Alt> auto isCurrentStateA() -> std::enable_if_t<is_variant_of_v<Alt, State>, bool>
        {
            return std::holds_alternative<Alt>(state_);
        }
        /// This function accepts any argument whose type is an alternative of the Event variant.
        /// It effectively visits the state_ field (remember that it is a variant) and forwards the received event for
        /// processing. The state classes implement the logic to know whether a transition should occur (i.e. return a
        /// new state), or not (return unexpected(InvalidTransition)).
        ///
        /// This template is almost identical to the full specialization `ExpectedState addEvent(Event event)` with the
        /// benefit of not requiring building a Event variant object, which wold represent a minor overhead.
        /// Usage: `machine.addEvent(Events::EstablishConnection{"10.0.2.2", 8080})`.
        template <typename Ev> auto addEvent(Ev&& event) -> std::enable_if_t<is_variant_of_v<Ev, Event>, ExpectedState>
        {
            ExpectedState maybe = std::visit(
              overloaded{
                [&](auto& s) -> std::enable_if_t<is_variant_of_v<decltype(s.on_event(event)), State> ||
                                                    std::is_same_v<decltype(s.on_event(event)), State> ||
                                                    std::is_same_v<decltype(s.on_event(event)), ExpectedState>,
                                                  ExpectedState> { return s.on_event(event); },
                [&](auto&&... arg) -> ExpectedState {
                    return nonstd::make_unexpected(InvalidTransition{state_, event});
                },
              },
              state_);

            return trySetState(maybe);
        }

        ///  This overload of the addEvent() member function handles the case when the argument is not an alternative
        ///  type of the Event variant, but the variant itself. It's useful when the variant has to be created anyway,
        ///  such as when events are created programmatically in a way that is not possible to predict exactly which
        ///  alternative type is to be created at the call site. Although quite similar, the visitation pattern applied
        ///  here visits simultaneously both state and event variants.
        ExpectedState addEvent(Event event)
        {
            ExpectedState maybe = std::visit(
              overloaded{
                [](auto& s,
                   auto& event) -> std::enable_if_t<is_variant_of_v<decltype(s.on_event(event)), State> ||
                                                       std::is_same_v<decltype(s.on_event(event)), State> ||
                                                       std::is_same_v<decltype(s.on_event(event)), ExpectedState>,
                                                     ExpectedState> { return s.on_event(event); },
                [&](auto&&... arg) -> ExpectedState {
                    return nonstd::make_unexpected(InvalidTransition{state_, event});
                },
              },
              state_, event);

            return trySetState(maybe);
        }
    };
}
