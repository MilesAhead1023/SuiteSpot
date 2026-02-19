# Implementation-Ready Checklist
**RLMAPS API Integration - SuiteSpot Plugin**  
**Date**: 2026-02-13  
**Status**: PHASE 0 REQUIRED BEFORE PHASE 1

---

## Phase 0: PRE-IMPLEMENTATION FIXES [MANDATORY] 🔴

**BLOCKING**: Must complete 100% before proceeding to Phase 1

### Task 0.1: Fix completedResults Thread Safety [CRITICAL]
**File**: `WorkshopDownloader.h`  
**Estimated Time**: 15 minutes  
**Priority**: P0 - CRITICAL

- [ ] **Change completedResults to atomic type**
  ```cpp
  // In WorkshopDownloader.h
  // OLD: int completedResults = 0;
  // NEW:
  std::atomic<int> completedResults{0};
  ```

- [ ] **Verify all increment sites**
  - [ ] Line 410 in `WorkshopDownloader.cpp` (success path)
  - [ ] 4 new error paths (see Task 0.2)
  - [ ] Confirm atomic increment syntax: `completedResults++;` or `completedResults.fetch_add(1);`

- [ ] **Test atomic behavior**
  - [ ] Run search with 20+ maps
  - [ ] Verify final count matches expected
  - [ ] Check for race condition (run 10 times)

**Verification**:
```cpp
// Test case: Search with 20 maps should set completedResults = 20
// No dropped increments allowed
```

---

### Task 0.2: Fix Missing completedResults Increments [CRITICAL]
**File**: `WorkshopDownloader.cpp::FetchReleaseDetails()`  
**Estimated Time**: 20 minutes  
**Priority**: P0 - CRITICAL

#### Error Path 1: Weak Pointer Expired (Line ~242)
- [ ] **Add increment before return**
  ```cpp
  auto self = weak_self.lock();
  if (!self) {
      // ADD THIS:
      // Note: Can't increment if self is null, but this should never happen
      // in normal operation. Log if it does.
      LOG("ERROR: weak_self expired in FetchReleaseDetails callback");
      return;
  }
  ```
  **Analysis**: This path may not need increment since object is destroyed.  
  **Action**: Document and confirm with testing.

#### Error Path 2: Search Cancelled (Lines 246-252)
- [ ] **Add increment before RLMAPS_Searching = false**
  ```cpp
  if (self->stopRequested || self->searchGeneration != generation) {
      LOG("FetchReleaseDetails cancelled for index {}", index);
      
      // ADD THIS BEFORE SETTING FLAG:
      self->completedResults++;
      
      self->RLMAPS_Searching = false;
      return;
  }
  ```

#### Error Path 3: Stale Generation (Line ~347)
- [ ] **Add increment before return (inside lock)**
  ```cpp
  {
      std::lock_guard<std::mutex> lock(self->resultsMutex);
      
      if (self->searchGeneration.load() != generation) {
          LOG("FetchReleaseDetails: Stale generation {}, current {}", generation, self->searchGeneration.load());
          
          // ADD THIS BEFORE RETURN:
          self->completedResults++;
          
          return;
      }
      // ... rest of locked section
  }
  ```

#### Error Path 4: Index Out of Bounds (Lines 388-391)
- [ ] **Add increment in else block**
  ```cpp
  } else {
      LOG("FetchReleaseDetails: Index {} out of bounds (size: {})",
          index, self->RLMAPS_MapResultList.size());
      
      // ADD THIS:
      self->completedResults++;
      
  }  // End of lock guard scope
  ```

**Testing Checklist**:
- [ ] Test: Cancel search mid-fetch → Verify `RLMAPS_Searching` becomes false
- [ ] Test: Start new search while old one running → Verify no deadlock
- [ ] Test: All maps fail to fetch → Verify search completes
- [ ] Test: Index out of bounds edge case → Verify increment happens
- [ ] Test: Search 0 results → Verify clean completion

---

### Task 0.3: Implement Cache Size Limit [HIGH]
**File**: `WorkshopDownloader.cpp`  
**Estimated Time**: 45 minutes  
**Priority**: P0 - HIGH

**Option A: Simple Clear on Search** (Recommended for speed)
- [ ] **Add cache clear in SearchRLMAPS() or equivalent**
  ```cpp
  void WorkshopDownloader::SearchRLMAPS() {
      // Clear cache to prevent unbounded growth
      workshopImageCache.clear();
      
      // ... rest of search logic
  }
  ```
  **Pros**: Simple, 1 line, prevents leaks  
  **Cons**: Loses cache between searches (acceptable)

**Option B: LRU Eviction** (Recommended for production)
- [ ] **Define max cache size constant**
  ```cpp
  // In WorkshopDownloader.h
  static constexpr size_t MAX_CACHE_SIZE = 150;
  ```

- [ ] **Implement eviction before insert**
  ```cpp
  void WorkshopDownloader::CacheImage(const std::string& key, ImTextureID texture) {
      // Evict oldest if at limit
      if (workshopImageCache.size() >= MAX_CACHE_SIZE) {
          // Option 1: Clear all (aggressive)
          workshopImageCache.clear();
          
          // Option 2: Remove first element (simple FIFO)
          // workshopImageCache.erase(workshopImageCache.begin());
      }
      
      workshopImageCache[key] = texture;
  }
  ```

- [ ] **Add cache size logging**
  ```cpp
  if (workshopImageCache.size() % 50 == 0) {
      LOG("Workshop image cache size: {}", workshopImageCache.size());
  }
  ```

**Testing**:
- [ ] Test: Perform 10 searches with 20 maps each → Verify cache size ≤ 150
- [ ] Test: Monitor memory usage over 20 searches → Verify stable
- [ ] Test: Check cache hit rate (log hits/misses)

---

### Task 0.4: Audit for Detached Threads [MODERATE]
**Files**: All `.cpp` files  
**Estimated Time**: 20 minutes  
**Priority**: P1 - MODERATE

- [ ] **Search codebase for detached threads**
  ```bash
  grep -rn "\.detach()" *.cpp *.h
  ```

- [ ] **For each detached thread found**:
  - [ ] Document reason for detachment
  - [ ] Verify object lifetime safety
  - [ ] Consider converting to joinable thread
  - [ ] OR add shared_ptr lifetime management

- [ ] **If no detached threads found**:
  - [ ] Document in verification report
  - [ ] Mark task complete

**Expected Result**: Zero detached threads OR all documented as safe

---

### Task 0.5: Verify Search Completion Logic [MODERATE]
**File**: `WorkshopDownloader.cpp`  
**Estimated Time**: 30 minutes  
**Priority**: P1 - MODERATE

- [ ] **Find where completedResults is initialized**
  - [ ] Trace to `SearchRLMAPS()` or equivalent
  - [ ] Verify it's reset to 0 at search start
  - [ ] Confirm it's atomic-safe initialization

- [ ] **Find where completedResults is compared**
  - [ ] Locate `if (completedResults >= expectedResults)` logic
  - [ ] Verify comparison happens in safe context
  - [ ] Check for off-by-one errors

- [ ] **Find where expectedResults is set**
  - [ ] Trace calculation logic
  - [ ] Verify it matches actual fetch count
  - [ ] Confirm it accounts for all code paths

- [ ] **Add logging for debugging**
  ```cpp
  // At search start
  LOG("Search started: expecting {} results", expectedResults);
  
  // At each increment
  LOG("Completed {}/{} results", completedResults.load(), expectedResults);
  
  // At completion check
  if (completedResults >= expectedResults) {
      LOG("Search complete: {}/{}", completedResults.load(), expectedResults);
      RLMAPS_Searching = false;
  }
  ```

**Testing**:
- [ ] Test: Search with 1 map → Verify 1/1 completion
- [ ] Test: Search with 20 maps → Verify 20/20 completion
- [ ] Test: Search with 0 results → Verify 0/0 completion
- [ ] Test: Cancel at 10/20 → Verify 20/20 then complete

---

## Phase 0 Exit Criteria ✅

**All checkboxes below MUST be checked before Phase 1:**

- [ ] **completedResults is atomic** (`std::atomic<int>`)
- [ ] **4 error paths have completedResults++**
  - [ ] Weak pointer expired path
  - [ ] Search cancelled path
  - [ ] Stale generation path
  - [ ] Out of bounds path
- [ ] **Cache has size limit** (150 images OR clear-on-search)
- [ ] **No unsafe detached threads** (audit complete)
- [ ] **Search completion logic verified** (logs added, tested)
- [ ] **All Phase 0 tests passing** (see test matrix below)

### Phase 0 Test Matrix

| Test Case | Expected Result | Status |
|-----------|----------------|--------|
| Search 20 maps, all succeed | completedResults = 20, search completes | ⬜ |
| Search 20 maps, cancel at 10 | completedResults = 20, no deadlock | ⬜ |
| Search with network errors | All fetches increment counter | ⬜ |
| Search 0 results | Clean completion, no hang | ⬜ |
| 5 rapid consecutive searches | No race conditions, correct counts | ⬜ |
| 20 searches without restart | Memory stable, cache ≤ 150 images | ⬜ |
| Out of bounds edge case | Increment happens, log generated | ⬜ |

---

## Phase 1: RLMAPS INTEGRATION [AFTER PHASE 0]

**PROCEED ONLY AFTER ALL PHASE 0 TASKS COMPLETE**

### Task 1.1: Update API Endpoints
**Estimated Time**: 1 hour  
**Priority**: P1

- [ ] Replace placeholder URLs with verified RLMAPS endpoints
  - [ ] Base URL: `https://celab.jetfox.ovh/api/v4`
  - [ ] Projects endpoint: `/api/v4/projects`
  - [ ] Packages endpoint: `/api/v4/projects/{id}/packages`

- [ ] Update error handling for 404 responses
- [ ] Implement retry logic (3 attempts with exponential backoff)
- [ ] Add request timeout (10 seconds)

---

### Task 1.2: Parse RLMAPS JSON Response
**Estimated Time**: 1 hour  
**Priority**: P1

- [ ] Map RLMAPS fields to internal structures
  ```cpp
  // From Gate 6 API testing:
  mapResult.ID = json["id"].get<std::string>();
  mapResult.Name = json["name"].get<std::string>();
  mapResult.Version = json["version"].get<std::string>();
  mapResult.CreatedAt = json["created_at"].get<std::string>();
  ```

- [ ] Extract release information
- [ ] Handle missing/optional fields gracefully
- [ ] Add JSON parsing error handling

**Reference**: `verification-results/gate6-api-testing.md` for full schema

---

### Task 1.3: Image URL Construction
**Estimated Time**: 30 minutes  
**Priority**: P2

- [ ] Implement image URL builder
  ```cpp
  // Format from Gate 6:
  // https://celab.jetfox.ovh/api/v4/projects/{id}/packages/generic/{name}/{version}/{name}.jpg
  std::string BuildImageURL(const std::string& projectId, 
                            const std::string& packageName, 
                            const std::string& version) {
      return "https://celab.jetfox.ovh/api/v4/projects/" + projectId + 
             "/packages/generic/" + packageName + "/" + version + "/" + 
             packageName + ".jpg";
  }
  ```

- [ ] Handle image download failures (404 from Gate 6)
- [ ] Add fallback to placeholder image
- [ ] Cache constructed URLs

**Known Issue**: Images may not be accessible (Gate 6: HTTP 000)  
**Mitigation**: Implement graceful fallback, log errors

---

### Task 1.4: Integration Testing
**Estimated Time**: 2 hours  
**Priority**: P1

- [ ] **Unit Tests**
  - [ ] Test JSON parsing with real RLMAPS data
  - [ ] Test URL construction with various inputs
  - [ ] Test error handling for malformed JSON
  - [ ] Test cache size limit enforcement

- [ ] **Integration Tests**
  - [ ] Search for "fractal" → Verify results
  - [ ] Download map package → Verify file
  - [ ] Load image preview → Verify texture
  - [ ] Cancel search → Verify no deadlock

- [ ] **Performance Tests**
  - [ ] Measure search latency (< 1 second target)
  - [ ] Monitor memory usage (stable over 20 searches)
  - [ ] Verify UI responsiveness (no blocking)

---

### Task 1.5: UI Integration
**Estimated Time**: 1 hour  
**Priority**: P2

- [ ] Update Workshop tab UI
  - [ ] Display RLMAPS results in list
  - [ ] Show map previews (with fallback)
  - [ ] Add version selector dropdown
  - [ ] Update progress indicators

- [ ] Handle loading states
  - [ ] Show spinner while searching
  - [ ] Display "No results" message
  - [ ] Show error messages gracefully

- [ ] Test UI responsiveness
  - [ ] Verify no frame drops during search
  - [ ] Confirm image loading doesn't block rendering
  - [ ] Test with 50+ results

**Reference**: Gate 4 validation confirms no deadlock risk

---

## Phase 1 Exit Criteria ✅

- [ ] RLMAPS API endpoints integrated
- [ ] JSON parsing complete and tested
- [ ] Image URLs constructible (fallback implemented)
- [ ] All Phase 1 tests passing
- [ ] UI displays RLMAPS results correctly
- [ ] No performance regressions
- [ ] Error handling robust
- [ ] Documentation updated

---

## Post-Implementation Verification

### Task V.1: Functional Verification
- [ ] Search for known map → Returns correct results
- [ ] Download map → File appears in correct directory
- [ ] Install map → Game recognizes map
- [ ] Image preview → Displays OR shows fallback
- [ ] Multiple searches → No memory leak
- [ ] Cancel during search → Clean shutdown

### Task V.2: Performance Verification
- [ ] Average search time < 1 second
- [ ] Memory usage stable over 20 searches
- [ ] UI remains responsive (≥ 60 FPS)
- [ ] Cache hit rate > 50% (if using LRU)
- [ ] No HTTP timeout errors

### Task V.3: Error Handling Verification
- [ ] Network failure → Graceful error message
- [ ] Invalid search → Returns 0 results cleanly
- [ ] Malformed JSON → Log error, continue
- [ ] Missing image → Shows placeholder
- [ ] Server 500 → Retry logic activates

---

## Rollback Triggers

**Immediately rollback if:**
- ⚠️ Search deadlocks occur in testing
- ⚠️ Memory leaks detected (> 100 MB growth)
- ⚠️ UI becomes unresponsive (< 30 FPS)
- ⚠️ Crashes during normal operation
- ⚠️ Data corruption in map results

**Rollback Procedure**:
1. Revert all Phase 1 changes
2. Keep Phase 0 fixes (they're general improvements)
3. Document failure mode in issue tracker
4. Re-evaluate implementation approach

---

## Documentation Requirements

- [ ] Update `CLAUDE.md` with RLMAPS integration details
- [ ] Document API endpoints in code comments
- [ ] Add JSON schema to header file
- [ ] Update user-facing documentation
- [ ] Create troubleshooting guide for common errors

---

## Code Review Checklist

- [ ] All Phase 0 fixes reviewed and approved
- [ ] Thread safety verified (no data races)
- [ ] Memory management verified (no leaks)
- [ ] Error handling comprehensive (all paths covered)
- [ ] Code follows SuiteSpot conventions
- [ ] Logging is appropriate (not excessive)
- [ ] Comments explain WHY not just WHAT
- [ ] No hardcoded credentials or secrets

---

## Success Criteria

**Definition of Done**:
1. ✅ All Phase 0 tasks complete (100%)
2. ✅ All Phase 1 tasks complete (100%)
3. ✅ All tests passing (100%)
4. ✅ Code review approved
5. ✅ Documentation updated
6. ✅ No performance regressions
7. ✅ No new bugs introduced

**Confidence Level**: HIGH (85%)  
**Estimated Total Time**: 8-11 hours  
**Risk Level**: LOW (after Phase 0 fixes)

---

## Task Dependencies

```
Phase 0.1 (Atomic) ─┐
                    ├─> Phase 0.2 (Error Paths) ─┐
Phase 0.3 (Cache) ──┘                            ├─> Phase 0 Exit ─> Phase 1 Start
Phase 0.4 (Audit) ───────────────────────────────┤
Phase 0.5 (Verify) ──────────────────────────────┘

Phase 1.1 (API) ────┐
Phase 1.2 (Parse) ──┼─> Phase 1.3 (Images) ─> Phase 1.4 (Tests) ─> Phase 1.5 (UI) ─> Done
```

---

## Notes & Reminders

- **Don't skip Phase 0**: Bugs MUST be fixed before integration
- **Test incrementally**: Don't wait until end to test
- **Log extensively**: Debugging is easier with good logs
- **Commit frequently**: Small commits are easier to revert
- **Monitor memory**: Use task manager or profiler
- **Ask for help**: If stuck > 1 hour, escalate

---

**Generated**: 2026-02-13 02:50 UTC  
**Status**: READY FOR PHASE 0  
**Next Update**: After Phase 0 completion
