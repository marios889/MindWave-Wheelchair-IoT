import 'dart:async';

class GenericWatchdog<K> {
  final Map<K, Timer> _activeTimers = {};
  final Map<K, DateTime> _lastCheckInTimes = {};
  final Duration defaultTimeout;

  /// Instantiate with a default timeout configuration (e.g., 10 seconds)
  GenericWatchdog({this.defaultTimeout = const Duration(seconds: 10)});

  /// Feeds the watchdog for a specific key.
  /// Resets the countdown clock. If it expires, [onTimeout] runs.
  void feed(
    K key, {
    required void Function() onTimeout,
    Duration? customTimeout,
  }) {
    _lastCheckInTimes[key] = DateTime.now();
    _activeTimers[key]?.cancel();

    final effectiveTimeout = customTimeout ?? defaultTimeout;

    _activeTimers[key] = Timer(effectiveTimeout, () {
      _activeTimers.remove(key);
      onTimeout();
    });
  }

  /// Evaluates if data associated with this key is currently actively streaming
  bool isStreaming(K key, {Duration? customTimeout}) {
    final lastTime = _lastCheckInTimes[key];
    if (lastTime == null) return false;

    final effectiveTimeout = customTimeout ?? defaultTimeout;
    return DateTime.now().difference(lastTime) < effectiveTimeout;
  }

  /// Cancels and completely clears a specific tracking key
  void cancel(K key) {
    _activeTimers[key]?.cancel();
    _activeTimers.remove(key);
    _lastCheckInTimes.remove(key);
  }

  /// Clears all timers out of memory (Essential for dispose cycles)
  void dispose() {
    for (var timer in _activeTimers.values) {
      timer.cancel();
    }
    _activeTimers.clear();
    _lastCheckInTimes.clear();
  }
}
