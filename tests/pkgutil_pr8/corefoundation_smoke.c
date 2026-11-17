// Reached through CoreFoundation.framework/Headers -> Versions/Current/Headers
// and Versions/Current -> A, both directory symlinks in the macOS SDK.
#include <CoreFoundation/CoreFoundation.h>

CFIndex corefoundation_smoke(CFStringRef s) { return CFStringGetLength(s); }
