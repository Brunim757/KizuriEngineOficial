#pragma once
#include <functional>
#include <string>
#include <sstream>

namespace kizuri {

enum class EventType {
    None = 0,
    WindowClose, WindowResize, WindowFocus, WindowLostFocus,
    KeyPressed, KeyReleased, KeyTyped,
    MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled
};

enum EventCategory {
    None = 0,
    EventCategoryApplication = 1 << 0,
    EventCategoryInput       = 1 << 1,
    EventCategoryKeyboard    = 1 << 2,
    EventCategoryMouse       = 1 << 3,
};

#define EVENT_CLASS_TYPE(type) \
    static EventType GetStaticType() { return EventType::type; } \
    EventType GetEventType() const override { return GetStaticType(); } \
    const char* GetName() const override { return #type; }

#define EVENT_CLASS_CATEGORY(category) \
    int GetCategoryFlags() const override { return category; }

class Event {
public:
    virtual ~Event() = default;
    bool Handled = false;

    virtual EventType GetEventType() const = 0;
    virtual const char* GetName() const = 0;
    virtual int GetCategoryFlags() const = 0;
    virtual std::string ToString() const { return GetName(); }

    bool IsInCategory(EventCategory category) const {
        return GetCategoryFlags() & category;
    }
};

class EventDispatcher {
public:
    explicit EventDispatcher(Event& event) : m_Event(event) {}

    template<typename T, typename F>
    bool Dispatch(const F& func) {
        if (m_Event.GetEventType() == T::GetStaticType()) {
            m_Event.Handled |= func(static_cast<T&>(m_Event));
            return true;
        }
        return false;
    }

private:
    Event& m_Event;
};

using EventCallbackFn = std::function<void(Event&)>;


class WindowResizeEvent : public Event {
public:
    WindowResizeEvent(unsigned w, unsigned h) : m_Width(w), m_Height(h) {}
    unsigned GetWidth() const { return m_Width; }
    unsigned GetHeight() const { return m_Height; }
    EVENT_CLASS_TYPE(WindowResize)
    EVENT_CLASS_CATEGORY(EventCategoryApplication)
private:
    unsigned m_Width, m_Height;
};

class WindowCloseEvent : public Event {
public:
    WindowCloseEvent() = default;
    EVENT_CLASS_TYPE(WindowClose)
    EVENT_CLASS_CATEGORY(EventCategoryApplication)
};


class KeyPressedEvent : public Event {
public:
    KeyPressedEvent(int keycode, bool repeat) : m_KeyCode(keycode), m_Repeat(repeat) {}
    int GetKeyCode() const { return m_KeyCode; }
    bool IsRepeat() const { return m_Repeat; }
    EVENT_CLASS_TYPE(KeyPressed)
    EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)
private:
    int m_KeyCode; bool m_Repeat;
};

class KeyReleasedEvent : public Event {
public:
    explicit KeyReleasedEvent(int keycode) : m_KeyCode(keycode) {}
    int GetKeyCode() const { return m_KeyCode; }
    EVENT_CLASS_TYPE(KeyReleased)
    EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)
private:
    int m_KeyCode;
};


class MouseMovedEvent : public Event {
public:
    MouseMovedEvent(float x, float y) : m_X(x), m_Y(y) {}
    float GetX() const { return m_X; }
    float GetY() const { return m_Y; }
    EVENT_CLASS_TYPE(MouseMoved)
    EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
private:
    float m_X, m_Y;
};

class MouseScrolledEvent : public Event {
public:
    MouseScrolledEvent(float xOff, float yOff) : m_XOffset(xOff), m_YOffset(yOff) {}
    float GetXOffset() const { return m_XOffset; }
    float GetYOffset() const { return m_YOffset; }
    EVENT_CLASS_TYPE(MouseScrolled)
    EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
private:
    float m_XOffset, m_YOffset;
};

class MouseButtonPressedEvent : public Event {
public:
    explicit MouseButtonPressedEvent(int button) : m_Button(button) {}
    int GetMouseButton() const { return m_Button; }
    EVENT_CLASS_TYPE(MouseButtonPressed)
    EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
private:
    int m_Button;
};

class MouseButtonReleasedEvent : public Event {
public:
    explicit MouseButtonReleasedEvent(int button) : m_Button(button) {}
    int GetMouseButton() const { return m_Button; }
    EVENT_CLASS_TYPE(MouseButtonReleased)
    EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
private:
    int m_Button;
};

} 
