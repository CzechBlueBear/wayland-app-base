#pragma once

template<class T>
class Box {
private:
    T* _M_ptr = nullptr;
    void (*deinitializer)(T*) = nullptr;

public:

    Box() {
        _M_ptr = nullptr;
    }

    Box(T* src) {
        _M_ptr = src;
    }

    operator T*() {
        return _M_ptr;
    }

    ~Box() {
        if (_M_ptr) {
            if (deinitializer) {
                deinitializer(_M_ptr);
                _M_ptr = nullptr;
            }
        }
    }

    T* release() {
        auto result = _M_ptr;
        _M_ptr = nullptr;
        return result;
    }

    void reset() {
        if (_M_ptr) {
            delete _M_ptr;
            _M_ptr = nullptr;
        }
    }
};
