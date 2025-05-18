#include <vector>
#include <algorithm>
#include <concepts>

// 1) "The definition of a concept"
template<typename T>
concept IResettable = requires(T t) {
    { t.reset() } -> std::same_as<void>;
};

// 2) The factory itself
template<IResettable T>
class ObjectFactory {
public:
    ObjectFactory(size_t maxItems, size_t batchSize)
        : _max(maxItems), _batch(batchSize) {
    }

    T* create() {
        if (_free.empty()) {
            allocateBatch();
        }
        T* obj = _free.back();
        _free.pop_back();
        obj->reset();
        return obj;
    }

    void destroy(T* obj) {
        obj->reset();
        _free.push_back(obj);
    }

    ~ObjectFactory() {
        for (auto* obj : _all) delete obj;
    }

private:
    void allocateBatch() {
        size_t toAlloc = (std::min)(_batch, _max - _all.size());
        for (size_t i = 0; i < toAlloc; ++i) {
            T* obj = new T();
            _free.push_back(obj);
            _all.push_back(obj);
        }
    }

    size_t _max, _batch;
    std::vector<T*> _free;
    std::vector<T*> _all;
};
