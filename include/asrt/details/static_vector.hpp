#ifndef DE6E1DB2_ABBF_4D55_9605_91E47AF30625
#define DE6E1DB2_ABBF_4D55_9605_91E47AF30625

#include <cstdint>
#include <limits>
#include <vector>
#include <queue>
#include <optional>
#include <mutex>
#include <type_traits>
#include <utility>
#include <span>
#include <expected.hpp>
#include <iterator>
#include <ranges>

// #include <spdlog/spdlog.h>
// #include <spdlog/fmt/bin_to_hex.h>

#include "asrt/util.hpp"
#include "asrt/error_code.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsubobject-linkage" /* gcc erroneously producecs warning: //todo
    error: ‘libdiag::util::Collection::valid_items_’ whose type uses the anonymous namespace [-Werror=subobject-linkage] */

namespace asrt{
namespace details{

using namespace tl;
template <typename T> using Optional = std::optional<T>;

template<
    typename Item_, 
    std::size_t Capacity_ = std::dynamic_extent,
    typename Mutex_ = Util::NullMutex>
class StaticVector
{
public:
    static_assert(Capacity_ == std::dynamic_extent ||\
        Capacity_ <= 255,
	  "capacity must not exceed maximum allowed capacity");

    struct entry_type {
        bool valid_{false};
        Mutex_ mtx_;
        Item_ item_;
    };

    using element_type      = std::optional<Item_>;
    using container_type    = std::vector<element_type>;
    using value_type        = Item_;
    using reference         = Item_&;
    using const_reference   = const Item_&;
    using size_type         = typename container_type::size_type;
    using mutex_type        = Mutex_;
    using entry_type        = typename container_type::value_type;
    using ItemId            = std::uint64_t;
    using ErrorCode         = ErrorCode_Ns::ErrorCode;

    template <typename T> using Optional = std::optional<T>;

    template <typename T> using Result = tl::expected<T, ErrorCode>;

    static constexpr bool IsDynamic() {return Capacity_ == std::dynamic_extent;}

    template <std::size_t C = Capacity_>
        requires (C != std::dynamic_extent)
    static constexpr size_type Capacity() {return Capacity_;}

    size_type Capacity() const noexcept {return items_.size();}

    template <std::size_t C = Capacity_> 
        requires (C != std::dynamic_extent)
    Collection() noexcept : items_{Capacity()} 
    { ASRT_LOG_TRACE("Collection size {}, capacity {}", size(), Capacity()); }

    template <std::size_t C = Capacity_> 
        requires (C == std::dynamic_extent)
    Collection(size_type capacity) noexcept : items_{capacity}
    {
        assert(capacity <= detail::kMaxCollectionCapacity); 
        ASRT_LOG_TRACE("Collection size {}, capacity {}", size(), Capacity()); 
    }

    template <typename... Args>
        requires std::is_constructible_v<Item_, Args...>
    [[nodiscard]]
    auto deposit(Args&&... args) noexcept -> Optional<std::pair<ItemId, reference>>
    {
        std::scoped_lock const lock{mutex_};

        if(size_() == Capacity()) 
            return std::nullopt;

        ItemId item_id{allocate_id_()};
        deposit_by_id_(item_id, std::forward<Args>(args)...);
        return std::pair<ItemId, reference>{item_id, get_item_ref_by_id_(item_id)};
    }

    template <typename Func, typename... Args>
        requires std::is_constructible_v<Item_, Args...>
    [[nodiscard]]
    auto DepositAndThen(Func f, Args&&... args) noexcept -> Optional<std::pair<ItemId, reference>>
    {
        std::scoped_lock const lock{mutex_};
        if(size_() == Capacity()) 
            return std::nullopt;

        ItemId item_id{allocate_id_()};
        deposit_by_id_(item_id, std::forward<Args>(args)...);
        reference ref{get_item_ref_by_id_(item_id)};
        f(ref);
        return std::pair<ItemId, reference>{item_id, ref};
    }

    template <typename Func>
    bool action(ItemId item_id, Func f)
    {
        std::scoped_lock const lock{mutex_};
        if(!is_in_use_(item_id)) return false;
        f(get_item_ref_by_id_(item_id));
        return true;
    }

    reference View(ItemId item_id) noexcept
    {
        std::scoped_lock const lock{mutex_};
        assert(is_in_use_(item_id));
        return get_item_ref_by_id_(item_id);
    }

    const_reference View(ItemId item_id) const noexcept
    {
        std::scoped_lock const lock{mutex_};
        assert(is_in_use_(item_id));
        return get_item_ref_by_id_(item_id);
    }

    bool erase(ItemId item_id) noexcept
    {
        std::scoped_lock const lock{mutex_};
        if(not is_in_use_(item_id)) return false;
        erase_by_id_(item_id);
        recyle_id_(item_id);
        return true;
    }

    template <typename Predicate>
        requires requires (Predicate p, reference r) 
            { {p(r)} -> std::convertible_to<bool>; }
    bool erase_if(ItemId item_id, Predicate pred) noexcept
    {
        std::scoped_lock const lock{mutex_};
        if(not is_in_use_(item_id)) return false;
        if(pred(get_item_ref_by_id_(item_id))){
            erase_by_id_(item_id);
            recyle_id_(item_id);
        }
        return true;
    }

    value_type withdraw(ItemId item_id) noexcept
    {
        std::scoped_lock const lock{mutex_};
        assert(is_in_use_(item_id));
        recyle_id_(item_id);

        return retrieve_by_id_(item_id).value();
    }

    size_type size() const noexcept
    {
        std::scoped_lock const lock{mutex_};
        return size_();
    }

    [[nodiscard]]
    bool is_empty() const noexcept
    {
        std::scoped_lock const lock{mutex_};
        return size_() == 0;
    }

    bool is_full() const noexcept
    {
        std::scoped_lock const lock{mutex_};
        return size_() == Capacity();
    }

    bool IsInUse(ItemId item_id) const noexcept
    {
        std::scoped_lock const lock{mutex_};
        return is_in_use_(item_id);
    }

    reference operator[] (ItemId item_id) noexcept
    {
        std::scoped_lock const lock{mutex_};
        assert(is_in_use_(item_id));
        return get_item_ref_by_id_(item_id);
    }

    const_reference operator[] (ItemId item_id) const noexcept
    {
        std::scoped_lock const lock{mutex_};
        assert(is_in_use_(item_id));
        return get_item_ref_by_id_(item_id);        
    }

    // auto begin()
    // {
    //     return valid_items_.begin();
    // }

    // auto end()
    // {
    //     return valid_items_.end();
    // }

    auto get_view() const noexcept
    {
        return valid_items_;
    }

private:

    size_type size_() const
    {
        return last_allocated_id_ + 1 - recycled_ids_.size();
    }

    std::size_t 
    get_item_idx_(ItemId item_id) const noexcept
    {
        return item_id;
    }

    typename container_type::iterator
    get_item_pos_(ItemId item_id) const noexcept
    {
        return this->items_.begin() + get_item_idx_(item_id);
    }

    template <typename... Args>
    void deposit_by_id_(ItemId item_id, Args... args) noexcept
    {
        this->items_[get_item_idx_(item_id)].emplace(std::forward<Args>(args)...);
    }

    void erase_by_id_(ItemId item_id) noexcept
    {
        this->items_[get_item_idx_(item_id)].reset();
        assert(!is_in_use_(item_id));
    }

    entry_type
    retrieve_by_id_(ItemId item_id) noexcept
    {
        const auto temp{std::move(this->items_[get_item_idx_(item_id)])};
        erase_by_id_(item_id);
        return temp;
    }

    reference 
    get_item_ref_by_id_(ItemId item_id) noexcept
    {
        return this->items_[get_item_idx_(item_id)].value();
    }

    const_reference 
    get_item_ref_by_id_(ItemId item_id) const noexcept
    {
        return this->items_[get_item_idx_(item_id)].value();
    }

    bool is_in_use_(ItemId item_id) const noexcept
    {
        return item_id <= last_allocated_id_ &&
            items_[get_item_idx_(item_id)].has_value();
    }

    ItemId allocate_id_() noexcept
    {
        ItemId next_id;
        if(!this->recycled_ids_.empty()){
            next_id = recycled_ids_.front();
            recycled_ids_.pop();
            return next_id;
        }

        return ++last_allocated_id_;
    }

    void recyle_id_(ItemId id) noexcept
    {
        this->recycled_ids_.push(id);
    }

    using ItemEntry = Optional<Item_>;

    static inline constexpr auto is_item_valid {
        [](element_type const& e) -> bool {
            return e.has_value();
        }};
        
    using ValidItemsView = decltype(
        std::ranges::views::filter(
            std::declval<container_type&>(), 
            is_item_valid));

    container_type items_;
    ValidItemsView valid_items_{std::ranges::views::filter(items_, is_item_valid)};
    std::queue<ItemId> recycled_ids_;
    ItemId last_allocated_id_{-1};
    mutex_type mutex_;
};

}
}

#endif /* DE6E1DB2_ABBF_4D55_9605_91E47AF30625 */
