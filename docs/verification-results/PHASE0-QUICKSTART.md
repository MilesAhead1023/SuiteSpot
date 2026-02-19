# 🚀 QUICK START: Phase 0 Fixes

**Before you start RLMAPS integration, fix these 3 CRITICAL bugs:**

---

## Fix 1: Thread Safety (15 min) 🔴

**File**: `WorkshopDownloader.h`

```cpp
// CHANGE THIS:
int completedResults = 0;

// TO THIS:
std::atomic<int> completedResults{0};
```

**Why**: Prevents race condition when multiple HTTP callbacks increment counter.

---

## Fix 2: Error Path Increments (20 min) 🔴

**File**: `WorkshopDownloader.cpp::FetchReleaseDetails()`

Add `completedResults++` to 4 missing error paths:

### Path 1: Line ~252 (Cancellation)
```cpp
if (self->stopRequested || self->searchGeneration != generation) {
    LOG("FetchReleaseDetails cancelled for index {}", index);
    self->completedResults++;  // ← ADD THIS
    self->RLMAPS_Searching = false;
    return;
}
```

### Path 2: Line ~347 (Stale generation)
```cpp
if (self->searchGeneration.load() != generation) {
    LOG("FetchReleaseDetails: Stale generation...");
    self->completedResults++;  // ← ADD THIS
    return;
}
```

### Path 3: Line ~391 (Out of bounds)
```cpp
} else {
    LOG("FetchReleaseDetails: Index {} out of bounds...");
    self->completedResults++;  // ← ADD THIS
}
```

### Path 4: Line ~242 (Weak pointer - REVIEW)
```cpp
if (!self) {
    // Document: Should this increment? Object destroyed.
    return;
}
```

**Why**: Without these, search hangs forever if ANY request fails.

---

## Fix 3: Cache Size Limit (45 min) 🔴

**File**: `WorkshopDownloader.cpp`

**Option A - Simple** (Recommended):
```cpp
void WorkshopDownloader::SearchRLMAPS() {
    // Clear cache to prevent unbounded growth
    workshopImageCache.clear();
    
    // ... rest of search logic
}
```

**Option B - Production** (Better but slower):
```cpp
// In header:
static constexpr size_t MAX_CACHE_SIZE = 150;

// Before caching:
if (workshopImageCache.size() >= MAX_CACHE_SIZE) {
    workshopImageCache.clear();  // Or implement LRU
}
```

**Why**: Prevents memory leak from unlimited image caching.

---

## Testing Checklist

After fixes, test these scenarios:

- [ ] Search with 20 maps → All complete, no hang
- [ ] Cancel mid-search → No deadlock
- [ ] Trigger network error → Search still completes
- [ ] Run 20 searches → Memory stays stable

---

## When Complete

✅ All 3 fixes applied  
✅ All tests passing  
✅ No deadlocks observed  

**Then**: Proceed to Phase 1 (RLMAPS integration)

**See**: `implementation-ready-checklist.md` for full Phase 1 tasks

---

**Estimated Time**: 1.5 hours  
**Risk**: CRITICAL bugs, MUST fix  
**Impact**: Prevents deadlocks, race conditions, memory leaks
