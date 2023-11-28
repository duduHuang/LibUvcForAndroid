//
// Created by TedHuang on 2023/11/27.
//

#ifndef LVILIBUVCPROJECT_OBJECTARRAY_H
#define LVILIBUVCPROJECT_OBJECTARRAY_H

template<class T>
class ObjectArray {
private:
    T *mElements;
    const int minSize;
    int mMaxSize;
    int mSize;

public:
    ObjectArray(int initialCapacity = 2) : mElements(new T[initialCapacity]),
                                           mMaxSize(initialCapacity), mSize(0),
                                           minSize(initialCapacity) {}

    ~ObjectArray() {
        if (mElements)
            delete[] mElements;
        mElements = nullptr;
    }

    void size(int newSize) {
        if (newSize != capacity()) {
            T *new_elements = new T[newSize];
            const int n = (newSize < capacity()) ? newSize : capacity();
            for (int i = 0; i < n; i++) {
                new_elements[i] = mElements[i];
            }
            if (mElements)
                delete[] mElements;
            mElements = nullptr;
            mElements = new_elements;
            mMaxSize = newSize;
            mSize = (mSize < newSize) ? mSize : newSize;
        }
    }

    inline const int size() { return mSize; }

    inline bool isEmpty() const { return (mSize < 1); }

    inline int capacity() const { return mMaxSize; }

    inline T &operator[](int index) { return mElements[index]; }

    inline const T &operator[](int index) const { return mElements[index]; }

    int put(T object) {
        if (object) {
            if (size() >= capacity()) {
                size(capacity() ? capacity() * 2 : 2);
            }
            mElements[mSize++] = object;
        }
        return mSize;
    }

    T remove(int index) {
        T obj = mElements[index];
        for (int i = index; i < mSize - 1; i++) {
            mElements[i] = mElements[i + 1];
        }
        mSize--;
        return obj;
    }

    void removeObject(T object) {
        for (int i = 0; i < size(); i++) {
            if (mElements[i] == object) {
                remove(i);
                break;
            }
        }
    }

    inline T last() {
        if (mSize > 0)
            return mElements[--mSize];
        else
            return nullptr;
    }

    int getIndex(const T object) {
        int result = -1;
        for (int i = 0; i < size(); i++) {
            if (mElements[i] == object) {
                result = i;
                break;
            }
        }
        return result;
    }

    inline void clear() {
        size(minSize);
        mSize = 0;
    }
};

#endif //LVILIBUVCPROJECT_OBJECTARRAY_H
