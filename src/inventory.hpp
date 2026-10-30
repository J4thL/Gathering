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
#include "items.hpp"

struct Slot {
    int item = -1;  // registry id, -1 = empty
    int n = 0;
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
    float weightLeft() const { return maxWeight - totalWeight(); }
    bool overweight() const { return totalWeight() > maxWeight + 1e-3f; }
    float stackWeight(int slot) const { return slots[slot].empty() ? 0.f : ::item(slots[slot].item).weight * slots[slot].n; }
    // how many of an item the weight limit alone still allows
    int weightRoom(int item) const {
        const float w = ::item(item).weight;
        if (w <= 0.f) return 1 << 30;
        const float left = weightLeft();
        return left <= 0.f ? 0 : (int)std::min<double>((left + 1e-3) / w, 1 << 30);
    }
    // how many of an item the slots alone still allow
    int slotRoom(int item) const {
        if (!itemValid(item)) return 0;
        int st = ::item(item).stack;
        long r = 0;
        for (auto& s : slots) {
            if (s.empty()) r += st;
            else if (s.item == item) r += std::max(0, st - s.n);
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

    // Total number of one item across all stacks.
    int count(int item) const {
        long t = 0;
        for (auto& s : slots)
            if (!s.empty() && s.item == item) t += s.n;
        return (int)t;
    }
    // How many more of an item would fit right now: limited by the free slots / partly filled stacks AND by the weight limit.
    int room(int item) const { return itemValid(item) ? std::min(slotRoom(item), weightRoom(item)) : 0; }
    // why an add of n would fall short: 0 fits, 1 all slots used, 2 too heavy
    int blockReason(int item, int n) const {
        if (!itemValid(item) || n <= 0) return 0;
        if (slotRoom(item) < n) return 1;
        return weightRoom(item) < n ? 2 : 0;
    }
    bool canAdd(int item, int n) const { return n >= 0 && room(item) >= n; }

    // Adds up to n; returns how many were actually added (partial adds are normal when the pack is full or too heavy).
    // Nothing is ever discarded: the caller keeps what did not fit. Use forceAdd for things that must not be refused.
    int add(int item, int n) {
        if (!itemValid(item) || n <= 0) return 0;
        n = std::min(n, weightRoom(item));
        return addIgnoringWeight(item, n);
    }
    int addIgnoringWeight(int item, int n) {
        if (!itemValid(item) || n <= 0) return 0;
        const int st = ::item(item).stack;
        int left = n;
        for (auto& s : slots) {  // top up existing stacks first
            if (left <= 0) break;
            if (!s.empty() && s.item == item && s.n < st) {
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
                left -= t;
            }
        }
        return n - left;
    }
    // Removes up to n (taking from the emptiest stack first so stacks stay full); returns how many were removed.
    int remove(int item, int n) {
        if (n <= 0) return 0;
        int left = n;
        while (left > 0) {
            Slot* best = nullptr;
            for (auto& s : slots)
                if (!s.empty() && s.item == item && (!best || s.n < best->n)) best = &s;
            if (!best) break;
            int t = std::min(left, best->n);
            best->n -= t;
            left -= t;
            if (best->n <= 0) *best = Slot();
        }
        return n - left;
    }
    // Replace the contents of an item with exactly n (as far as space allows). Used by tests and the save loader.
    int set(int item, int n) {
        remove(item, count(item));
        return add(item, n);
    }
    // Change the number of slots. Growing always works; shrinking only if the items still fit, otherwise it is refused.
    bool setCapacity(int cap) {
        if (cap >= capacity()) {
            slots.resize(cap);
            return true;
        }
        compact();
        if (usedSlots() > cap) return false;
        slots.resize(cap);
        return true;
    }
    // Merge partial stacks and move everything to the front, ordered by item id.
    void compact() {
        std::vector<std::pair<int, int>> tot;  // (item, count)
        for (auto& s : slots)
            if (!s.empty()) {
                auto it = std::find_if(tot.begin(), tot.end(), [&](auto& p) { return p.first == s.item; });
                if (it == tot.end()) tot.push_back({s.item, s.n});
                else it->second += s.n;
            }
        std::sort(tot.begin(), tot.end());
        for (auto& s : slots) s = Slot();
        for (auto& p : tot) addIgnoringWeight(p.first, p.second);  // re-sorting must never drop anything, even when overweight
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
