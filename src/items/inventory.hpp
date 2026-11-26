// inventory.hpp - the slot-based inventory.
//
// Always INV_SLOTS (100) slots, each holding one kind of item and up to that item's stack size (999 for everything that
// stacks; launchers, tools, bags and carried gear take a slot each). What limits the hunter is weight: the inventory has
// a maximum carrying weight (set from the worn bag) and add() never takes more than fits. Weights are never stored, they are
// always computed from the registry. All item facts come from items.hpp.
#pragma once
#include <algorithm>
#include <string>
#include <vector>
#include "items/items.hpp"
#include "items/quality.hpp"

constexpr float OVERLOAD_MAX = 1.5f;   // the pack can be loaded to this multiple of the carry limit...
constexpr float OVERLOAD_SLOW = 0.5f;  // ...which halves the walking speed (the slow-down grows evenly from the limit)
struct Slot {
    int item = -1;  // registry id, -1 = empty
    int n = 0;
    int q = 0;      // quality tier (items/quality.hpp); a stack only ever holds one tier
    bool empty() const { return item < 0 || n <= 0; }
};

class Inventory {
public:
    std::vector<Slot> slots;

    float maxWeight;  // kg that can be carried (the worn bag sets it; chests have no limit)

    explicit Inventory(int capacity = INV_SLOTS, float maxW = (float)BASE_CARRY_KG) : slots(capacity), maxWeight(maxW) {}

    // ---- weight
    float totalWeight() const {
        double t = 0;
        for (auto& s : slots)
            if (!s.empty()) t += (double)::item(s.item).weight * s.n;
        return (float)t;
    }
    // The limit can be exceeded up to OVERLOAD_MAX times (the hunter is slowed, see slowFactor); past that nothing more is picked up.
    float weightCap() const { return maxWeight * OVERLOAD_MAX; }
    float weightLeft() const { return weightCap() - totalWeight(); }
    bool overweight() const { return totalWeight() > maxWeight + 1e-3f; }
    // walking speed multiplier for the load: 1 up to the limit, falling to OVERLOAD_SLOW at the hard cap
    float slowFactor() const {
        if (!overweight() || maxWeight <= 0.f) return 1.f;
        const float over = (totalWeight() - maxWeight) / (maxWeight * (OVERLOAD_MAX - 1.f));
        return 1.f - (1.f - OVERLOAD_SLOW) * std::min(1.f, over);
    }
    float stackWeight(int slot) const { return slots[slot].empty() ? 0.f : ::item(slots[slot].item).weight * slots[slot].n; }
    // how many of an item the weight limit alone still allows
    int weightRoom(int item) const {
        const float w = ::item(item).weight;
        if (w <= 0.f) return 1 << 30;
        const float left = weightLeft();
        return left <= 0.f ? 0 : (int)std::min<double>((left + 1e-3) / w, 1 << 30);
    }
    // how many of an item the slots alone still allow
    int slotRoom(int item, int q = Q_NORMAL) const {
        if (!itemValid(item)) return 0;
        int st = ::item(item).stack;
        long r = 0;
        for (auto& s : slots) {
            if (s.empty()) r += st;
            else if (s.item == item && s.q == q) r += std::max(0, st - s.n);
        }
        return (int)std::min<long>(r, 1 << 30);
    }

    int capacity() const { return (int)slots.size(); }
    int usedSlots() const {
        int u = 0;
        for (auto& s : slots) u += s.empty() ? 0 : 1;
        return u;
    }
    int freeSlots() const { return capacity() - usedSlots(); }

    // Total number of one item across all stacks (every quality).
    int count(int item) const {
        long t = 0;
        for (auto& s : slots)
            if (!s.empty() && s.item == item) t += s.n;
        return (int)t;
    }
    // ... of one quality only
    int countQ(int item, int q) const {
        long t = 0;
        for (auto& s : slots)
            if (!s.empty() && s.item == item && s.q == q) t += s.n;
        return (int)t;
    }
    // the best quality of an item held (-1 = none)
    int bestQuality(int item) const {
        int b = -1;
        for (auto& s : slots)
            if (!s.empty() && s.item == item && s.q > b) b = s.q;
        return b;
    }
    // How many more of an item would fit right now: limited by the free slots / partly filled stacks AND by the weight limit.
    int room(int item, int q = Q_NORMAL) const { return itemValid(item) ? std::min(slotRoom(item, q), weightRoom(item)) : 0; }
    // why an add of n would fall short: 0 fits, 1 all slots used, 2 too heavy
    int blockReason(int item, int n, int q = Q_NORMAL) const {
        if (!itemValid(item) || n <= 0) return 0;
        if (slotRoom(item, q) < n) return 1;
        return weightRoom(item) < n ? 2 : 0;
    }
    bool canAdd(int item, int n, int q = Q_NORMAL) const { return n >= 0 && room(item, q) >= n; }

    // Adds up to n; returns how many were actually added (partial adds are normal when the pack is full or too heavy).
    // Nothing is ever discarded: the caller keeps what did not fit. Use forceAdd for things that must not be refused.
    int add(int item, int n, int q = Q_NORMAL) {
        if (!itemValid(item) || n <= 0) return 0;
        n = std::min(n, weightRoom(item));
        return addIgnoringWeight(item, n, q);
    }
    int addIgnoringWeight(int item, int n, int q = Q_NORMAL) {
        if (!itemValid(item) || n <= 0) return 0;
        q = clampQuality(q);
        const int st = ::item(item).stack;
        int left = n;
        for (auto& s : slots) {  // top up existing stacks of the same quality first
            if (left <= 0) break;
            if (!s.empty() && s.item == item && s.q == q && s.n < st) {
                int t = std::min(left, st - s.n);
                s.n += t;
                left -= t;
            }
        }
        for (auto& s : slots) {  // then use empty slots
            if (left <= 0) break;
            if (s.empty()) {
                int t = std::min(left, st);
                s.item = item;
                s.n = t;
                s.q = q;
                left -= t;
            }
        }
        return n - left;
    }
    // Removes up to n, the lowest quality first (the best pieces are kept) and then the emptiest stack first so stacks stay full;
    // returns how many were removed.
    int remove(int item, int n) {
        if (n <= 0) return 0;
        int left = n;
        while (left > 0) {
            Slot* best = nullptr;
            for (auto& s : slots)
                if (!s.empty() && s.item == item && (!best || s.q < best->q || (s.q == best->q && s.n < best->n))) best = &s;
            if (!best) break;
            int t = std::min(left, best->n);
            best->n -= t;
            left -= t;
            if (best->n <= 0) *best = Slot();
        }
        return n - left;
    }
    // Takes one piece of the BEST quality held (the one a hunter reaches for: the sharpest knife, the strongest salve); returns its
    // quality, or -1 when there is none.
    int takeBest(int item) {
        Slot* best = nullptr;
        for (auto& s : slots)
            if (!s.empty() && s.item == item && (!best || s.q > best->q || (s.q == best->q && s.n < best->n))) best = &s;
        if (!best) return -1;
        const int q = best->q;
        if (--best->n <= 0) *best = Slot();
        return q;
    }
    // Removes up to n pieces of exactly one quality.
    int removeQ(int item, int q, int n) {
        int left = n;
        for (auto& s : slots) {
            if (left <= 0) break;
            if (s.empty() || s.item != item || s.q != q) continue;
            const int t = std::min(left, s.n);
            s.n -= t;
            left -= t;
            if (s.n <= 0) s = Slot();
        }
        return n - left;
    }
    // Replace the contents of an item with exactly n (as far as space allows). Used by tests and the save loader.
    int set(int item, int n) {
        remove(item, count(item));
        return add(item, n);
    }
    // Drag and drop / split: move n items from one slot to another. Onto an empty slot or the same (item, quality) it moves up to n (a
    // merge stops at the stack limit); onto a different item it swaps, but only when the whole stack is moved. Returns how many moved.
    int moveSlot(int from, int to, int n) {
        if (from == to || from < 0 || to < 0 || from >= capacity() || to >= capacity() || slots[from].empty() || n <= 0) return 0;
        Slot& a = slots[from];
        Slot& b = slots[to];
        n = std::min(n, a.n);
        if (b.empty()) {
            b = Slot{a.item, n, a.q};
        } else if (b.item == a.item && b.q == a.q) {
            n = std::min(n, ::item(a.item).stack - b.n);
            if (n <= 0) return 0;
            b.n += n;
        } else if (n == a.n) {
            std::swap(a, b);
            return n;
        } else {
            return 0;
        }
        a.n -= n;
        if (a.n <= 0) a = Slot();
        return n;
    }
    // Merge partial stacks and move everything to the front, ordered by item id.
    void compact() {
        struct T { int item, q, n; };
        std::vector<T> tot;  // one entry per (item, quality)
        for (auto& s : slots)
            if (!s.empty()) {
                auto it = std::find_if(tot.begin(), tot.end(), [&](const T& p) { return p.item == s.item && p.q == s.q; });
                if (it == tot.end()) tot.push_back({s.item, s.q, s.n});
                else it->n += s.n;
            }
        std::sort(tot.begin(), tot.end(), [](const T& a, const T& b) { return a.item != b.item ? a.item < b.item : a.q < b.q; });
        for (auto& s : slots) s = Slot();
        for (auto& p : tot) addIgnoringWeight(p.item, p.n, p.q);  // re-sorting must never drop anything, even when overweight
    }
    void clear() {
        for (auto& s : slots) s = Slot();
    }
};

// ---- stable save keys: the lower-case item name ("raw_meat", "flint_knife"). Item ids can shift when new items
// are added to the registry; names do not, so saves are written with names.
inline std::string itemKey(int id) {
    std::string k = item(id).name;
    for (auto& c : k) c = c == ' ' ? '_' : (char)tolower((unsigned char)c);
    return k;
}
inline int itemFromKey(const std::string& k) {
    for (int i = 0; i < ITEM_COUNT; i++)
        if (itemKey(i) == k) return i;
    return -1;
}
