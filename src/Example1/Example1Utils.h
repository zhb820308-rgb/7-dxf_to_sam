#ifndef Example1Utils_h
#define Example1Utils_h

/// @brief Register the Example1 Python module with the SAM runtime.
/// Increments reference count; safe to call multiple times.
/// @param count [in/out] Module reference counter.
void Example1Initialize(int& count);

/// @brief Deregister the Example1 Python module.
/// Decrements reference count; cleans up when count reaches zero.
/// @param count [in/out] Module reference counter.
void Example1Finalize(int& count);

#endif
