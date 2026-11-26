#pragma once
// A callable held in place (v455): std::function's use where the plugin's audio thread runs it, without its allocation.
// The VC-2's part passes lambdas down its note path, its settings' readers and its OS model as std::function, which takes
// the heap for one whose captures pass three pointers; on the audio thread that's an allocation a note (VSynthEmuVc2Play-
// Test counted 22 a chord).  This holds the lambda itself in a buffer of `Room` bytes, refused at compile time when it
// doesn't fit or isn't plain data, so a call never allocates.  A lambda that captures by reference refers to what it
// captured, as std::function's does.
#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

template <class Sig, size_t Room = 6 * sizeof(void*)>
class XtFn;

template <class R, class... A, size_t Room>
class XtFn<R(A...), Room> {
public:
    XtFn() = default;
    template <class F, class = std::enable_if_t<!std::is_same_v<std::decay_t<F>, XtFn>>>
    XtFn(F f) {   // NOLINT: as std::function, made from any callable
        static_assert(sizeof(F) <= Room, "the callable's captures don't fit: make Room larger");
        static_assert(alignof(F) <= alignof(std::max_align_t), "the callable is aligned past the buffer");
        static_assert(std::is_trivially_copyable_v<F> && std::is_trivially_destructible_v<F>,
                      "the callable must be plain data: capture pointers and references, not objects");
        new (buf_) F(f);
        call_ = [](void* at, A... a) -> R { return (*static_cast<F*>(at))(std::forward<A>(a)...); };
    }
    R operator()(A... a) const { return call_(buf_, std::forward<A>(a)...); }
    explicit operator bool() const { return call_ != nullptr; }

private:
    alignas(std::max_align_t) mutable unsigned char buf_[Room] {};
    R (*call_)(void*, A...) = nullptr;
};
