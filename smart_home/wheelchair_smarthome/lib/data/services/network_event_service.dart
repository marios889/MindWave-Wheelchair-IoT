import 'package:flutter/foundation.dart';

enum NetworkBroadcastType {
  fullRoomStats, // Wakes up all standard appliances (Lamps, Switches)
  sensorDataPull, // Requests real-time telemetry from ESP sensor nodes
  doorContactCheck, // Targets magnetic reed switches/door nodes specifically
  pirSensorCheck,
}

class NetworkEventService {
  // Pass the broadcast type through a ValueNotifier
  static final ValueNotifier<NetworkBroadcastType?> networkTrigger =
      ValueNotifier<NetworkBroadcastType?>(null);
  // Helper to trigger a specific network action category
  static void fireBroadcast(NetworkBroadcastType type) {
    networkTrigger.value = type;
  }
}
