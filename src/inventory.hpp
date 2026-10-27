// inventory.hpp - the slot-based inventory.
//
// A fixed number of slots (set by the backpack), each holding one kind of item and up to that item's stack size
// (materials, ammunition and thrown weapons stack; launchers and backpacks do not). Backpacks only add slots - they
// never change stack sizes. All item facts come from the registry in items.hpp.
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

    explicit Inventory(int capacity = 20) : slots(capacity) {}

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
    // How many more of an item would fit right now (room in partly filled stacks plus empty slots).
    int room(int item) const {
        if (!itemValid(item)) return 0;
        int st = ::item(item).stack;
        long r = 0;
        for (auto& s : slots) {
            if (s.empty()) r += st;
            else if (s.item == item) r += std::max(0, st - s.n);
        }
        return (int)std::min<long>(r, 1 << 30);
    }
    bool canAdd(int item, int n) const { return n >= 0 && room(item) >= n; }

    // Adds up to n; returns how many were actually added (partial adds are normal when the bag is nearly full).
    int add(int item, int n) {
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
        for (auto& p : tot) add(p.first, p.second);
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
