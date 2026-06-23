import 'dart:async';

import 'package:wheelchair_smarthome/data/notifiers/devices_state.dart';
import 'package:wheelchair_smarthome/data/services/generic_watchdog.dart';
import 'package:wheelchair_smarthome/data/services/network_event_service.dart';

class NetworkWatchdogManager {
  static DateTime? _lastSensorPacketTime;
  static DateTime? _lastDoorPacketTime;
  static DateTime? _lastPIRPacketTime;
  static final broadcastWatchdog = GenericWatchdog<NetworkBroadcastType>(
    defaultTimeout: Duration(seconds: 5),
  );

  static Timer? _sensorRecoveryTimer;
  static Timer? _doorRecoveryTimer;
  static Timer? _pirRecoveryTimer;
  static const Duration timeoutLimit = Duration(seconds: 5);

  /// 1. Called inside MultiUdpService when raw sensor data lands
  static void registerSensorPacketReceived(int sensorPort) {
    _lastSensorPacketTime = DateTime.now();
    _startSensorRecoveryTimer(sensorPort);
  }

  /// 2. Called inside MultiUdpService when raw door contact data lands
  static void registerDoorPacketReceived(int doorPort) {
    _lastDoorPacketTime = DateTime.now();
    _startDoorRecoveryTimer(doorPort);
  }

  static void registerPIRPacketReceived(int pirPort) {
    _lastPIRPacketTime = DateTime.now();
    _startPIRRecoveryTimer(pirPort);
  }

  /// Evaluates if sensor data is currently actively streaming
  static bool isSensorStreaming() {
    if (_lastSensorPacketTime == null) return false;
    return DateTime.now().difference(_lastSensorPacketTime!) < timeoutLimit;
  }

  /// Evaluates if door data is currently actively streaming
  static bool isDoorStreaming() {
    if (_lastDoorPacketTime == null) return false;
    return DateTime.now().difference(_lastDoorPacketTime!) < timeoutLimit;
  }

  static bool isPirStreaming() {
    if (_lastPIRPacketTime == null) return false;
    return DateTime.now().difference(_lastPIRPacketTime!) < timeoutLimit;
  }

  /// Automatically restarts broadcasts if sensor node goes silent for 10 seconds
  static void _startSensorRecoveryTimer(int sensorPort) {
    _sensorRecoveryTimer?.cancel();
    _sensorRecoveryTimer = Timer(timeoutLimit, () {
      print(
        "🚨 Watchdog: Sensor data stopped for 5s! Forcing recovery broadcast...",
      );

      // Inside MultiUdpService where Sensor data arrives:
      broadcastWatchdog.feed(
        NetworkBroadcastType.sensorDataPull,
        onTimeout: () async {
          print("Re-firing sensor broadcast due to timeout...");

          NetworkEventService.fireBroadcast(
            NetworkBroadcastType.sensorDataPull,
          );
        },
      );
      DevicesState.updateConnectionByPort(sensorPort.toString(), false);
    });
  }

  /// Automatically restarts broadcasts if door node goes silent for 10 seconds
  static void _startDoorRecoveryTimer(int doorPort) {
    _doorRecoveryTimer?.cancel();
    _doorRecoveryTimer = Timer(timeoutLimit, () {
      print(
        "🚨 Watchdog: Door data stopped for 10s! Forcing recovery broadcast...",
      );
      broadcastWatchdog.feed(
        NetworkBroadcastType.doorContactCheck,
        onTimeout: () async {
          print("Re-firing sensor broadcast due to timeout...");

          NetworkEventService.fireBroadcast(
            NetworkBroadcastType.doorContactCheck,
          );
        },
      );
      DevicesState.updateConnectionByPort(doorPort.toString(), false);
    });
  }

  static void _startPIRRecoveryTimer(int pirPort) {
    _pirRecoveryTimer?.cancel();
    _pirRecoveryTimer = Timer(timeoutLimit, () {
      print(
        "🚨 Watchdog: PIR data stopped for 10s! Forcing recovery broadcast...",
      );
      broadcastWatchdog.feed(
        NetworkBroadcastType.pirSensorCheck,
        onTimeout: () async {
          print("Re-firing sensor broadcast due to timeout...");

          NetworkEventService.fireBroadcast(
            NetworkBroadcastType.pirSensorCheck,
          );
        },
      );
      DevicesState.updateConnectionByPort(pirPort.toString(), false);
    });
  }

  /// Clean up timers if logging out or cleaning up application scope
  static void cancelAllWatchdogs() {
    _sensorRecoveryTimer?.cancel();
    _doorRecoveryTimer?.cancel();
  }
}
