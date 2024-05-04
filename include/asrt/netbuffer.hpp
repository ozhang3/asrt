#ifndef F1D7D7CD_EB59_43F9_82D7_24F129C87BB5
#define F1D7D7CD_EB59_43F9_82D7_24F129C87BB5

#include <cstdint>
#include <vector>
#include <array>
#include <span>
#include <algorithm>
#include <string>
#include <string_view>

namespace Buffer
{

    template <typename T>
    concept BufferViewLike = requires (T t) {t.data(); t.size();};

    inline constexpr std::size_t kDynamicExtent = -1;

    class MutableBufferView;
    class ConstBufferView;

    class MutableBufferView
    {
        void *data_{nullptr};
        std::size_t size_{};

    public:
        constexpr MutableBufferView() noexcept = default;
        constexpr MutableBufferView(std::span<std::uint8_t> view) noexcept : data_{view.data()}, size_{view.size()} {}
        constexpr MutableBufferView(void *data, std::size_t size) noexcept : data_{data}, size_{size} {}
        MutableBufferView(MutableBufferView const &) noexcept = default;
        MutableBufferView(MutableBufferView &&) noexcept = default;
        MutableBufferView &operator=(MutableBufferView const &other) noexcept = default;
        MutableBufferView &operator=(MutableBufferView &&other) noexcept = default;
        ~MutableBufferView() noexcept = default;

        struct Iterator 
        {
            using iterator_category = std::contiguous_iterator_tag;
            using difference_type   = std::ptrdiff_t;
            using value_type        = std::uint8_t;
            using pointer           = value_type*;
            using reference         = value_type const&;

            Iterator(pointer p) noexcept : ptr_{p} {}

            reference operator*() const { return *ptr_; }

            // Prefix increment
            Iterator& operator++()
            {
                ptr_++;
                return *this; 
            }  

            // Postfix increment
            Iterator operator++(int) 
            {
                Iterator tmp = *this; 
                ++(*this); 
                return tmp; 
            }

            friend bool operator== (const Iterator& a, const Iterator& b) { return a.ptr_ == b.ptr_; }
            friend bool operator!= (const Iterator& a, const Iterator& b) { return a.ptr_ != b.ptr_; }

        private:
            pointer ptr_;
        };

        Iterator begin() noexcept { return (std::uint8_t*)data_; }
        Iterator end() noexcept { return (std::uint8_t*)data_ + size_; }


        /* Acquire pointer to start of underlying buffer */
        constexpr auto data() const noexcept -> void * { return this->data_; }

        /* Acquire the byte size of underlying buffer */
        constexpr auto size() const noexcept -> decltype(this->size_) { return this->size_; }

        [[nodiscard]] constexpr bool  
        Empty() const noexcept {return size_ == 0;}

        [[nodiscard]] auto 
        SubView(std::size_t offset, std::size_t count = kDynamicExtent) const noexcept
        {
            assert(offset <= size());

            if(count != kDynamicExtent) 
                assert((offset + count) <= size());
            else
                count = size() - offset;

            return MutableBufferView{(std::uint8_t*)data_ + offset, count};
        }

        [[nodiscard]] MutableBufferView 
        first(std::size_t num_bytes) noexcept
        {
            return MutableBufferView{(std::uint8_t*)data_, std::min(num_bytes, size())};
        }
        
        /* Shift view of underlying buffer forward by the specified number of bytes. */
        void Advance(std::size_t n)
        {
            const std::size_t offset = n < size_ ? n : size_;
            this->data_ = static_cast<std::uint8_t *>(this->data_) + offset;
            this->size_ -= offset;
        }

        /* Move (reduce) view of underlying buffer by the specified number of bytes. */
        MutableBufferView &operator+=(std::size_t n) noexcept
        {
            const std::size_t offset = n < size_ ? n : size_;
            this->data_ = static_cast<std::uint8_t *>(this->data_) + offset;
            this->size_ -= offset;
            return *this;
        }

        operator std::span<std::uint8_t const>() const noexcept
        {
            return {(const std::uint8_t*)data_, size_};
        }

        operator std::span<std::uint8_t>() noexcept
        {
            return {(std::uint8_t*)data_, size_};
        }
    };

    class ConstBufferView
    {
        const void *data_{nullptr};
        std::size_t size_{};

    public:
        constexpr ConstBufferView() noexcept = default;
        constexpr ConstBufferView(const void *data, std::size_t size) noexcept 
            : data_{data}, size_{size} {}
        constexpr ConstBufferView(const MutableBufferView& mutable_buff) noexcept
            : data_{mutable_buff.data()}, size_{mutable_buff.size()} {}
        constexpr ConstBufferView(std::span<std::uint8_t> view) noexcept : data_{view.data()}, size_{view.size()} {}
        constexpr ConstBufferView(std::span<std::uint8_t const> view) noexcept : data_{view.data()}, size_{view.size()} {}

        ConstBufferView(ConstBufferView const &) noexcept = default;
        ConstBufferView(ConstBufferView &&) noexcept = default;
        ConstBufferView &operator=(ConstBufferView const &other) noexcept = default;
        ConstBufferView &operator=(ConstBufferView &&other) noexcept = default;
        ~ConstBufferView() noexcept = default;

        struct Iterator 
        {
            using iterator_category = std::contiguous_iterator_tag;
            using difference_type   = std::ptrdiff_t;
            using value_type        = std::uint8_t const;
            using pointer           = value_type*;
            using reference         = value_type const&;

            Iterator(pointer p) noexcept : ptr_{p} {}

            reference operator*() const { return *ptr_; }

            // Prefix increment
            Iterator& operator++()
            {
                ptr_++;
                return *this; 
            }  

            // Postfix increment
            Iterator operator++(int) 
            {
                Iterator tmp = *this; 
                ++(*this); 
                return tmp; 
            }

            friend bool operator== (const Iterator& a, const Iterator& b) { return a.ptr_ == b.ptr_; }
            friend bool operator!= (const Iterator& a, const Iterator& b) { return a.ptr_ != b.ptr_; }

        private:
            pointer ptr_;
        };


        Iterator begin() const noexcept { return (std::uint8_t const*)data_; }
        Iterator end() const noexcept { return (std::uint8_t const*)data_ + size_; }

        /* Acquire pointer to start of underlying buffer */
        constexpr auto data() const noexcept -> const void * { return this->data_; }

        /* Acquire the byte size of underlying buffer */
        constexpr auto size() const noexcept -> decltype(this->size_) { return this->size_; }

        [[nodiscard]] constexpr auto 
        Empty() const noexcept {return size_ == 0;}

        [[nodiscard]] ConstBufferView 
        first(std::size_t num_bytes) noexcept
        {
            return ConstBufferView{(std::uint8_t*)data_, std::min(num_bytes, size())};
        }

        [[nodiscard]] const auto 
        SubView(std::size_t offset, std::size_t count = kDynamicExtent) const noexcept
        {
            assert(offset <= this->size_);

            if(count != kDynamicExtent) 
                assert((offset + count) <= size_);
            else
                count = this->size_ - offset;

            return ConstBufferView{(std::uint8_t*)data_ + offset, count};
        }

        /* Advance view of underlying buffer by the specified number of bytes. */
        void Advance(std::size_t n)
        {
            std::size_t offset = n < size_ ? n : size_;
            this->data_ = static_cast<const std::uint8_t *>(this->data_) + offset;
            this->size_ -= offset;
        }

        /* Move view of underlying buffer by the specified number of bytes. */
        ConstBufferView &operator+=(std::size_t n) noexcept
        {
            std::size_t offset = n < size_ ? n : size_;
            this->data_ = static_cast<const std::uint8_t *>(this->data_) + offset;
            this->size_ -= offset;
            return *this;
        }

        friend ConstBufferView operator+(ConstBufferView buff, const std::size_t n)
        {
            buff += n;
            return buff;
        }

        operator std::span<std::uint8_t const>() const noexcept
        {
            return {(const std::uint8_t*)data_, size_};
        }

    };

    [[nodiscard]] constexpr inline auto 
    make_buffer(void* data, std::size_t size) noexcept -> MutableBufferView
    {
        return {data, size};
    }

    [[nodiscard]] constexpr inline auto 
    make_buffer(const void* data, std::size_t size) noexcept -> ConstBufferView
    {
        return {data, size};
    }

    [[nodiscard]] constexpr inline auto 
    make_buffer(const char* data) noexcept -> ConstBufferView
    {
        return {data, std::char_traits<char>::length(data) + 1};
    }

    [[nodiscard]] constexpr inline auto 
    make_buffer(const char* data, std::size_t max_size) noexcept -> ConstBufferView
    {
        return {data, std::min(max_size, std::char_traits<char>::length(data) + 1)};
    }

    template<typename T, std::size_t N>
    [[nodiscard]] constexpr inline auto 
    make_buffer(T(&arr)[N]) noexcept -> MutableBufferView
    {
        static_assert(std::is_standard_layout<T>::value, "Data must be pod type");
        return {arr, N * sizeof(T)};
    }

    template<typename T, std::size_t N>
    [[nodiscard]] constexpr inline auto 
    make_buffer(T(&arr)[N], std::size_t max_size) noexcept -> MutableBufferView
    {
        static_assert(std::is_standard_layout<T>::value, "Data must be pod type");
        return {arr, std::min(max_size, N * sizeof(T))};    
    }

    template<typename T, std::size_t N>
    [[nodiscard]] constexpr inline auto 
    make_buffer(const T(&arr)[N]) noexcept -> ConstBufferView
    {
        static_assert(std::is_standard_layout<T>::value, "Data must be pod type");
        return {arr, N * sizeof(T)};    
    }

    template<typename T, std::size_t N>
    [[nodiscard]] constexpr inline auto 
    make_buffer(const T(&arr)[N], std::size_t max_size) noexcept -> ConstBufferView
    {
        static_assert(std::is_standard_layout<T>::value, "Data must be pod type");
        return {arr, std::min(max_size, N * sizeof(T))};    
    }

    template<typename T, std::size_t N>
    [[nodiscard]] constexpr inline auto 
    make_buffer(std::array<T, N>& std_array) noexcept -> MutableBufferView
    {
        static_assert(std::is_standard_layout<T>::value, "Data must be pod type");
        return {std_array.data(), std_array.size() * sizeof(T)};
    }

    template<typename T, std::size_t N>
    [[nodiscard]] constexpr inline auto 
    make_buffer(const std::array<T, N>& std_array) noexcept -> ConstBufferView
    {
        static_assert(std::is_standard_layout<T>::value, "Data must be pod type");
        return {std_array.data(), std_array.size() * sizeof(T)};
    }

    template<typename T, std::size_t N>
    [[nodiscard]] constexpr inline auto 
    make_buffer(std::array<T, N>& std_array, std::size_t max_size) noexcept -> MutableBufferView
    {
        static_assert(std::is_standard_layout<T>::value, "Data must be pod type");
        return {std_array.data(), std::min(max_size, std_array.size() * sizeof(T))};
    }

    template<typename T, std::size_t N>
    [[nodiscard]] constexpr inline auto 
    make_buffer(const std::array<T, N>& std_array, std::size_t max_size) noexcept -> ConstBufferView
    {
        static_assert(std::is_standard_layout<T>::value, "Data must be pod type");
        return {std_array.data(), std::min(max_size, std_array.size() * sizeof(T))};
    }

    template<typename T, typename Allocator>
    [[nodiscard]] constexpr inline auto 
    make_buffer(std::vector<T, Allocator>& std_vector) noexcept -> MutableBufferView
    {
        static_assert(std::is_standard_layout<T>::value, "Data must be pod type");
        return {std_vector.size() ? std_vector.data() : nullptr, std_vector.size() * sizeof(T)};
    }

    template<typename T, typename Allocator>
    [[nodiscard]] constexpr inline auto 
    make_buffer(const std::vector<T, Allocator>& std_vector) noexcept -> ConstBufferView
    {
        static_assert(std::is_standard_layout<T>::value, "Data must be pod type");
        return {std_vector.size() ? std_vector.data() : nullptr, std_vector.size() * sizeof(T)};
    }

    template<typename T, typename Allocator>
    [[nodiscard]] constexpr inline auto 
    make_buffer(std::vector<T, Allocator>& std_vector, std::size_t max_size) noexcept -> MutableBufferView
    {
        static_assert(std::is_standard_layout<T>::value, "Data must be pod type");
        return {std_vector.size() ? std_vector.data() : nullptr, std::min(max_size, std_vector.size() * sizeof(T))};
    }

    template<typename T, typename Allocator>
    [[nodiscard]] constexpr inline auto 
    make_buffer(const std::vector<T, Allocator>& std_vector, std::size_t max_size) noexcept -> ConstBufferView
    {
        static_assert(std::is_standard_layout<T>::value, "Data must be pod type");
        return {std_vector.size() ? std_vector.data() : nullptr, std::min(max_size, std_vector.size() * sizeof(T))};
    }

    template <typename T, typename Traits, typename Allocator>
    [[nodiscard]] constexpr inline auto 
    make_buffer(std::basic_string<T, Traits, Allocator>& std_string) noexcept -> MutableBufferView
    {
        return {std_string.size() ? std_string.data() : nullptr, std_string.size() * sizeof(T)};
    }

    template <typename T, typename Traits, typename Allocator>
    [[nodiscard]] constexpr inline auto 
    make_buffer(std::basic_string<T, Traits, Allocator>& std_string, std::size_t max_size) noexcept -> MutableBufferView
    {
        return {std_string.size() ? std_string.data() : nullptr,  std::min(max_size, std_string.size() * sizeof(T))};
    }

    template <typename T, typename Traits, typename Allocator>
    [[nodiscard]] constexpr inline auto 
    make_buffer(const std::basic_string<T, Traits, Allocator>& std_string) noexcept -> ConstBufferView
    {
        return {std_string.size() ? std_string.data() : nullptr, std_string.size() * sizeof(T)};
    }

    template <typename T, typename Traits, typename Allocator>
    [[nodiscard]] constexpr inline auto 
    make_buffer(const std::basic_string<T, Traits, Allocator>& std_string, std::size_t max_size) noexcept -> ConstBufferView
    {
        return {std_string.size() ? std_string.data() : nullptr,  std::min(max_size, std_string.size() * sizeof(T))};
    }

    template <typename T, typename Traits>
    [[nodiscard]] constexpr inline auto 
    make_buffer(std::basic_string_view<T, Traits>& std_string_view) noexcept -> MutableBufferView
    {
        return {std_string_view.size() ? std_string_view.data() : nullptr, std_string_view.size() * sizeof(T)};
    }

    template <typename T, typename Traits>
    [[nodiscard]] constexpr inline auto 
    make_buffer(std::basic_string_view<T, Traits>& std_string_view, std::size_t max_size) noexcept -> MutableBufferView
    {
        return {std_string_view.size() ? std_string_view.data() : nullptr,  std::min(max_size, std_string_view.size() * sizeof(T))};
    }

    template <typename T, typename Traits>
    [[nodiscard]] constexpr inline auto 
    make_buffer(const std::basic_string_view<T, Traits>& std_string_view) noexcept -> ConstBufferView
    {
        return {std_string_view.size() ? std_string_view.data() : nullptr, std_string_view.size() * sizeof(T)};
    }

    template <typename T, typename Traits>
    [[nodiscard]] constexpr inline auto 
    make_buffer(const std::basic_string_view<T, Traits>& std_string_view, std::size_t max_size) noexcept -> ConstBufferView
    {
        return {std_string_view.size() ? std_string_view.data() : nullptr,  std::min(max_size, std_string_view.size() * sizeof(T))};
    }
    
}
#endif /* F1D7D7CD_EB59_43F9_82D7_24F129C87BB5 */
