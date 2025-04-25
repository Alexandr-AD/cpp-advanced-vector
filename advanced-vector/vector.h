#pragma once
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>

template <typename T>
class RawMemory
{
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity)), capacity_(capacity)
    {
    }

    ~RawMemory()
    {
        Deallocate(buffer_);
        capacity_ = 0;
    }

    RawMemory(const RawMemory &) = delete;
    RawMemory &operator=(const RawMemory &rhs) = delete;
    RawMemory(RawMemory &&other) noexcept
        : buffer_(std::move(other.buffer_)),
          capacity_(other.capacity_)
    {
        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }
    RawMemory &operator=(RawMemory &&rhs) noexcept
    {
        if (this != &rhs)
        {
            Deallocate(buffer_);
            buffer_ = std::move(rhs.buffer_);
            capacity_ = std::move(rhs.capacity_);

            rhs.buffer_ = nullptr;
            rhs.capacity_ = 0;
        }
        return *this;
    }

    T *operator+(size_t offset) noexcept
    {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T *operator+(size_t offset) const noexcept
    {
        return const_cast<RawMemory &>(*this) + offset;
    }

    const T &operator[](size_t index) const noexcept
    {
        return const_cast<RawMemory &>(*this)[index];
    }

    T &operator[](size_t index) noexcept
    {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory &other) noexcept
    {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T *GetAddress() const noexcept
    {
        return buffer_;
    }

    T *GetAddress() noexcept
    {
        return buffer_;
    }

    size_t Capacity() const
    {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T *Allocate(size_t n)
    {
        return n != 0 ? static_cast<T *>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T *buf) noexcept
    {
        operator delete(buf);
    }

    T *buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector
{
public:
    Vector() = default;

    explicit Vector(size_t size)
        : data_(size), size_(size) //
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }
    Vector(const Vector &other)
        : data_(other.size_), size_(other.size_) //
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }
    Vector(Vector &&other) noexcept
        : data_(std::move(other.data_)), size_(other.size_)
    {
        other.size_ = 0;
    }

    ~Vector()
    {
        std::destroy_n(data_.GetAddress(), size_);
    }
    size_t Size() const noexcept
    {
        return size_;
    }

    Vector &operator=(const Vector &rhs)
    {
        if (this != &rhs)
        {
            if (rhs.size_ > data_.Capacity())
            {
                // Если не хватает capacity, просто создаем новую копию
                Vector(rhs).Swap(*this);
            }
            else
            {
                // Копируем элементы из rhs
                const size_t min_size = std::min(size_, rhs.size_);

                // Копируем общую часть
                for (size_t i = 0; i < min_size; ++i)
                {
                    data_[i] = rhs.data_[i];
                }

                if (rhs.size_ > size_)
                {
                    // Если rhs больше - копируем оставшиеся элементы
                    std::uninitialized_copy_n(
                        rhs.data_.GetAddress() + size_,
                        rhs.size_ - size_,
                        data_.GetAddress() + size_);
                }
                else
                {
                    // Если rhs меньше - уничтожаем лишние элементы
                    std::destroy_n(
                        data_.GetAddress() + rhs.size_,
                        size_ - rhs.size_);
                }

                size_ = rhs.size_;
            }
        }
        return *this;
    }
    Vector &operator=(Vector &&rhs) noexcept
    {
        Swap(rhs);

        return *this;
    }

    void Swap(Vector &other) noexcept
    {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    size_t Capacity() const noexcept
    {
        return data_.Capacity();
    }
    void Reserve(size_t new_capacity)
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
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size)
    {
        if (size_ >= new_size)
        {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
            size_ = new_size;
        }
        else
        {
            Reserve(new_size);
            std::uninitialized_default_construct_n(data_.GetAddress() + size_, new_size - size_);
            size_ = new_size;
        }
    }
    void PushBack(const T &value)
    {
        EmplaceBack(value);
    }
    void PushBack(T &&value)
    {
        EmplaceBack(std::move(value));
    }
    void PopBack() noexcept
    {
        assert(size_ > 0);
        std::destroy_at(data_.GetAddress() + size_ - 1);
        --size_;
    }
    template <typename... Args>
    T &EmplaceBack(Args &&...args)
    {
        return *Emplace(end(), std::forward<Args>(args)...);
    }
    const T &operator[](size_t index) const noexcept
    {
        return const_cast<Vector &>(*this)[index];
    }

    T &operator[](size_t index) noexcept
    {
        assert(index < size_);
        return *(data_.GetAddress() + index);
    }

    using iterator = T *;
    using const_iterator = const T *;

    iterator begin() noexcept
    {
        return data_.GetAddress();
    }
    const_iterator begin() const noexcept
    {
        return data_.GetAddress();
    }
    const_iterator cbegin() const noexcept
    {
        return data_.GetAddress();
    }
    iterator end() noexcept
    {
        return data_.GetAddress() + size_;
    }
    const_iterator end() const noexcept
    {
        return data_.GetAddress() + size_;
    }
    const_iterator cend() const noexcept
    {
        return data_.GetAddress() + size_;
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args &&...args)
    {
        const size_t offset = pos - begin();
        if (size_ == Capacity())
        {
            const size_t new_capacity = std::max(size_t(1), Capacity() * 2);
            RawMemory<T> new_data(new_capacity);

            T *new_element_ptr = new_data.GetAddress() + offset;
            std::construct_at(new_element_ptr, std::forward<Args>(args)...);
            try
            {
                // переносим элементы до позиции вставки
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
                {
                    std::uninitialized_move_n(begin(), offset, new_data.GetAddress());
                }
                else
                {
                    std::uninitialized_copy_n(begin(), offset, new_data.GetAddress());
                }
                try
                {
                    // переносим элементы после позиции вставки
                    if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
                    {
                        std::uninitialized_move_n(begin() + offset /* + 1 */, size_ - offset, new_data.GetAddress() + offset + 1);
                    }
                    else
                    {
                        std::uninitialized_copy_n(begin() + offset /* + 1 */, size_ - offset, new_data.GetAddress() + offset + 1);
                    }
                }
                catch (const std::exception &e)
                {
                    // Уничтожаем перенесенные элементы ПОСЛЕ позиции (если успели)
                    // std::destroy_n(new_data.GetAddress() + offset + 1, size_ - offset);
                    std::destroy_n(new_data.GetAddress(), offset + 1);
                    throw;
                    // Уничтожаем перенесенные элементы ДО позиции
                    // std::destroy_n(new_data.GetAddress(), offset);

                    // Уничтожаем вставленный элемент
                    // new_element_ptr->~T();
                    // throw; 
                }
            }
            catch (const std::exception &e)
            {
                // // Уничтожаем перенесенные элементы ДО позиции
                // std::destroy_n(new_data.GetAddress(), offset); 
                std::destroy_at(new_data.GetAddress() + offset);
                // // Уничтожаем вставленный элемент
                // new_element_ptr->~T();

                throw;
            }
            std::destroy_n(begin(), size_);
            data_.Swap(new_data);
        }
        else
        {
            if (pos == end())
            {
                std::construct_at(data_ + offset, std::forward<Args>(args)...);
            }
            else
            {

                T tmp(std::forward<Args>(args)...);
                try
                {
                    new (end()) T(std::move(data_[size_ - 1]));
                    std::move_backward(begin() + offset, end() - 1, begin() + size_); 
                }
                catch (...)
                {
                    std::destroy_at(end());
                    // end()->~T(); 
                    // Плюс при исключении в перемещении ниже вектор будет в некорректном состоянии.
                    throw;
                }

                // *(begin() + offset) = std::move(tmp);
                data_[offset] = std::move(tmp);
            }
        }
        ++size_;
        return begin() + offset;
    }
    iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/
    {

        assert(pos >= begin() && pos < end());
        const size_t offset = pos - begin();
        std::move(data_.GetAddress() + offset + 1, data_.GetAddress() + size_, data_.GetAddress() + offset);

        std::destroy_at(data_.GetAddress() + size_ - 1);
        --size_;
        return data_.GetAddress() + offset;
    }
    iterator Insert(const_iterator pos, const T &value)
    {
        return Emplace(pos, value);
    }
    iterator Insert(const_iterator pos, T &&value)
    {
        return Emplace(pos, std::move(value));
    }

private:
    // private:
    RawMemory<T> data_;
    size_t size_ = 0;
};