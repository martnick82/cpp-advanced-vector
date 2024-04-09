#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;
    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept 
    {
        if(buffer_)
        {
            Deallocate(buffer_);
        }
        Swap(other);
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept 
    { 
        Swap(rhs);
        return *this;
    }
    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {}

    ~RawMemory() 
    {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept 
    {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept 
    {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept 
    {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept 
    {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept 
    {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept 
    {
        return buffer_;
    }

    T* GetAddress() noexcept 
    {
        return buffer_;
    }

    size_t Capacity() const 
    {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) 
    {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept 
    {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};


template <typename T>
class Vector
{
public:
    Vector() = default;

    explicit Vector(size_t size);

    Vector(const Vector& other);

    Vector(Vector&& other) noexcept;

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept
    {
        return data_ + 0;
    }
    iterator end() noexcept
    {
        return data_ + size_;
    }
    const_iterator begin() const noexcept
    {
        return cbegin();
    }
    const_iterator end() const noexcept
    {
        return cend();
    }
    const_iterator cbegin() const noexcept
    {
        return data_ + 0;
    }
    const_iterator cend() const noexcept
    {
        return data_ + size_;
    }

    size_t Size() const noexcept;
    size_t Capacity() const noexcept;
    const T& operator[](size_t index) const noexcept;
    T& operator[](size_t index) noexcept;
    void Reserve(size_t new_capacity);
    Vector& operator=(const Vector& rhs);
    Vector& operator=(Vector&& rhs) noexcept;
    void Swap(Vector& other) noexcept;
    void Resize(size_t new_size);

    template <typename V>
    void PushBack(V&& value);

    template <typename... Args>
    T& EmplaceBack(Args&&... args);

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args)
    {
        size_t pos_t = pos - begin();
        if (size_ == Capacity())
        {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            try
            {
                ForwardConstruct(new_data + pos_t, (std::forward<Args>(args))...);
            }
            catch (...)
            {
                Destroy(new_data + pos_t);
                throw;
            }
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
            {
                std::uninitialized_move_n(data_.GetAddress(), pos_t, new_data.GetAddress());
                std::uninitialized_move_n(data_ + pos_t, size_ - pos_t, new_data + pos_t + 1);
            }
            else
            {
                std::uninitialized_copy_n(data_.GetAddress(), pos_t, new_data.GetAddress());
                std::uninitialized_copy_n(data_ + pos_t, size_ - pos_t, new_data + pos_t + 1);
            }
            data_.Swap(new_data);
            std::destroy_n(new_data.GetAddress(), size_);
        }
        else
        {            
            try
            {
                if (size_ != 0)
                {
                    T temp((std::forward<Args>(args))...);
                    ForwardConstruct(data_ + size_, std::forward<T>(data_[size_ - 1]));
                    std::move_backward(data_ + pos_t, data_ + (size_ - 1), data_ + size_);  
                    data_[pos_t] = std::forward<T>(temp);
                }
                else
                {
                    if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
                    {
                        ForwardConstruct(data_ + pos_t, (std::forward<Args>(args))...);
                    }
                    else
                    {
                        CopyConstruct(data_ + pos_t, (std::forward<Args>(args))...);
                    }
                }
            }
            catch (...)
            {
                Destroy(data_ + pos_t);
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
                {
                    ForwardConstruct(data_ + pos_t, std::forward<T>(data_[size_ + pos_t + 1]));
                    std::move(data_ + pos_t + 2, data_ + size_, data_ + pos_t + 1);
                }
                else
                {
                    CopyConstruct(data_ + pos_t, std::forward<T>(data_[size_ + pos_t + 1]));
                    std::copy(data_ + pos_t + 2, data_ + size_, data_ + pos_t + 1);
                }
                throw;
            }
        }
        ++size_;
        return data_ + pos_t;
    }

    template <typename V>
    iterator Insert(const_iterator pos, V&& value)
    {
        return Emplace(pos, std::forward<V>(value));
    }

    void PopBack() noexcept;
    T& Back() noexcept;
    iterator Erase(const_iterator pos)
    {
        assert((pos >= data_ + 0) && (pos < end()));
        const size_t pos_t = pos - data_.GetAddress();
        std::move(data_ + pos_t + 1, data_ + size_, data_ + pos_t);
        Destroy(data_ + size_ - 1);
        --size_;
        return data_ + pos_t;
    }

    ~Vector();

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    // Создаёт копию объекта elem в сырой памяти по адресу buf
    template<typename... Args>
    static void CopyConstruct(T* buf, Args&&... args);

    template<typename... Args>
    static void MoveConstruct(T* buf, Args&&... args);

    template<typename... Args>
    static void ForwardConstruct(T* buf, Args&&... args);

    // Вызывает деструктор объекта по адресу buf
    static void Destroy(T* buf) noexcept;
};


template<typename T>
inline size_t Vector<T>::Size() const noexcept
{
    return size_;
}

template<typename T>
inline size_t Vector<T>::Capacity() const noexcept
{
    return data_.Capacity();
}

template<typename T>
inline const T& Vector<T>::operator[](size_t index) const noexcept
{
    return const_cast<Vector&>(*this)[index];
}

template<typename T>
inline T& Vector<T>::operator[](size_t index) noexcept
{
    assert(index < size_);
    return data_[index];
}

template<typename T>
inline void Vector<T>::Reserve(size_t new_capacity)
{
    if (new_capacity <= data_.Capacity())
    {
        return;
    }
    RawMemory<T> new_data(new_capacity);
    if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
    {
        std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
    }
    else
    {
        std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
    }
    new_data.Swap(data_);
    std::destroy_n(new_data.GetAddress(), size_);
}

template<typename T>
inline Vector<T>& Vector<T>::operator=(const Vector<T>& rhs)
{
    if (this != &rhs) {
        if (rhs.size_ > data_.Capacity())
        {
            Vector rhs_copy(rhs);
            Swap(rhs_copy);
        }
        else
        {
            if (rhs.size_ < size_)
            {
                for (size_t i = 0; i < rhs.size_; ++i)
                {
                    data_[i] = rhs[i];
                }
                std::destroy_n(data_ + rhs.size_, size_ - rhs.size_);
                size_ = rhs.size_;
            }
            else
            {
                for (size_t i = 0; i < size_; ++i)
                {
                    data_[i] = rhs[i];
                }
                std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
                size_ = rhs.size_;
            }
        }
    }
    return *this;
}

template<typename T>
inline Vector<T>& Vector<T>::operator=(Vector<T>&& rhs) noexcept
{
    Swap(rhs);
    return *this;
}

template<typename T>
inline void Vector<T>::Swap(Vector& other) noexcept
{
    data_.Swap(other.data_);
    std::swap(size_, other.size_);
}

template<typename T>
inline Vector<T>::Vector(size_t size)
    : data_(size)
    , size_(size)
{
    std::uninitialized_value_construct_n(data_.GetAddress(), size);
}

template<typename T>
inline Vector<T>::Vector(const Vector& other)
    : data_(other.Size())
    , size_(other.Size())
{
    std::uninitialized_copy_n(other.data_.GetAddress(), other.Size(), data_.GetAddress());
}

template<typename T>
inline Vector<T>::Vector(Vector&& other) noexcept
{
    Swap(other);
}

template<typename T>
inline void Vector<T>::Resize(size_t new_size)
{
    if (new_size <= size_)
    {
        std::destroy_n(data_ + new_size, size_ - new_size);
        size_ = new_size;
    }
    else
    {
        size_t count_new_elem = new_size - size_;
        if (Capacity() < new_size)
        {
            Reserve(new_size);
        }
        std::uninitialized_value_construct_n(data_ + (Capacity() - count_new_elem), count_new_elem);
        size_ = new_size;
    }
}

template<typename T>
inline void Vector<T>::PopBack() noexcept
{
    Destroy(data_ + size_-1);
    --size_;
}

template<typename T>
inline T& Vector<T>::Back() noexcept
{
    return data_[size_ - 1];
}

template<typename T>
inline Vector<T>::~Vector()
{
    std::destroy_n(data_.GetAddress(), size_);
}

template<typename T>
template<typename... Args>
inline void Vector<T>::CopyConstruct(T* buf, Args&&... args)
{
    new (buf) T((args)...);
}

template<typename T>
template<typename... Args>
inline void Vector<T>::MoveConstruct(T* buf, Args&&... args)
{
    new (buf) T(std::move(args)...);
}

template<typename T>
inline void Vector<T>::Destroy(T* buf) noexcept
{
    buf->~T();
}

template<typename T>
template<typename V>
inline void Vector<T>::PushBack(V&& value)
{
    if (size_ == Capacity())
    {
        RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
        try
        {
            ForwardConstruct(new_data + size_, std::forward<V>(value));
        }
        catch (...)
        {
            Destroy(new_data + size_);
            throw;
        }

        if constexpr (!std::is_copy_constructible_v<T> || std::is_nothrow_move_constructible_v<T>)
        {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else
        {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        data_.Swap(new_data);
        std::destroy_n(new_data.GetAddress(), size_);
    }
    else
    {
        try
        {
            ForwardConstruct(data_ + size_, (std::forward<V>(value)));
        }
        catch (...)
        {
            Destroy(data_ + size_);
            throw;
        }
    }
    ++size_;
}

template<typename T>
template<typename ...Args>
inline T& Vector<T>::EmplaceBack(Args && ...args)
{
    if (size_ == Capacity())
    {
        RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
        try
        {
            ForwardConstruct(new_data + size_, (std::forward<Args>(args))...);
        }
        catch (...)
        {
            Destroy(new_data + size_);
            throw;
        }

        if constexpr (!std::is_copy_constructible_v<T> || std::is_nothrow_move_constructible_v<T>)
        {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else
        {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        data_.Swap(new_data);
        std::destroy_n(new_data.GetAddress(), size_);
    }
    else
    {
        try
        {
            ForwardConstruct(data_ + size_, (std::forward<Args>(args))...);
        }
        catch (...)
        {
            Destroy(data_ + size_);
            throw;
        }
    }
    ++size_;
    return data_[size_ - 1];
}

template<typename T>
template<typename ...Args>
inline void Vector<T>::ForwardConstruct(T* buf, Args && ...args)
{
    new (buf) T(std::forward<Args>(args)...);
}