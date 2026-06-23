import 'dart:io';
import 'dart:typed_data';
import 'package:flutter/material.dart';
import 'package:network_info_plus/network_info_plus.dart';
import 'package:udp/udp.dart';
import 'dart:convert';
import 'dart:async';
import 'package:wheelchair_smarthome/data/notifiers/devices_state.dart';
import 'package:wheelchair_smarthome/data/notifiers/notifiers.dart';
import 'package:wheelchair_smarthome/data/services/generic_watchdog.dart';
import 'package:wheelchair_smarthome/data/services/network_event_service.dart';
import 'package:wheelchair_smarthome/data/services/network_watchdog_manager.dart';
import 'package:wheelchair_smarthome/data/services/notification_service.dart';

class UdpService {
  static Future<String?> _getPrivateIp() async {
    final info = NetworkInfo();
    String? ip = await info.getWifiIP();
    return ip; // Fetches the local IP (e.g., 192.168.1.X)
  }

  // One single, global socket dedicated to sending messages out
  static UDP? _outboundSocket;
  static final Map<String, Timer> activeWatchdogs = {};

  /// Initializes the outbound sending socket instance.
  static Future<void> initOutbound() async {
    String? phoneIp = await _getPrivateIp();
    if (phoneIp == null) {
      print("Phone is disconnected. Ensuring socket is closed...");
      await closeSocket();
      return;
    }
    if (_outboundSocket != null) return;
    try {
      _outboundSocket = await UDP.bind(Endpoint.any(port: const Port(0)));
      print("🔌 Global Outbound UDP Socket initialized successfully.");
    } catch (e) {
      print("❌ Error initializing outbound UDP socket: $e");
    }
  }

  // 1. Send plain text string commands (Unicast)
  static Future<void> sendUDP(
    String message,
    String ip,
    int port, {
    bool useWatchdog = false,
    Duration timeoutDuration = const Duration(milliseconds: 1500),
  }) async {
    if (_outboundSocket == null) {
      await initOutbound();
    }
    try {
      if (useWatchdog) {
        startDeviceWatchdog(ip, timeoutDuration);
      }
      await _outboundSocket!.send(
        utf8.encode(message),
        Endpoint.unicast(InternetAddress(ip), port: Port(port)),
      );
      print("📤 Sent Plain Text: '$message' to $ip on Port: $port");
    } catch (e) {
      print("❌ UDP Plain Text error: $e");
      if (useWatchdog) {
        handleTimeoutFailure(ip);
      }
    }
  }

  // 1. Send plain text string commands (Unicast)
  static Future<void> sendIntUDP(
    int value,
    String ip,
    int port,
    bool is32Bit,
  ) async {
    if (_outboundSocket == null) {
      await initOutbound();
    }
    try {
      Uint8List bytes;
      if (!is32Bit) {
        bytes = Uint8List(1);
        bytes[0] = value & 0xFF;
      } else {
        bytes = Uint8List(4);
        ByteData.view(bytes.buffer).setInt32(0, value, Endian.big);
      }
      await _outboundSocket!.send(
        bytes,
        Endpoint.unicast(InternetAddress(ip), port: Port(port)),
      );
      print("📤 Sent Value: '$value' to $ip on Port: $port");
    } catch (e) {
      print("❌ UDP Plain Text error: $e");
    }
  }

  // 2. Send JSON command directly to a specific device (Unicast)
  static Future<void> sendJsonMessage(
    String targetIp,
    int port,
    Map<String, dynamic> jsonMap,
  ) async {
    if (_outboundSocket == null) {
      await initOutbound();
    }
    try {
      String jsonString = jsonEncode(jsonMap);
      await _outboundSocket!.send(
        utf8.encode(jsonString),
        Endpoint.unicast(InternetAddress(targetIp), port: Port(port)),
      );
      print("📤 Sent JSON: $jsonString to $targetIp and Port: $port");
    } catch (e) {
      print("❌ UDP JSON error: $e");
    }
  }

  // 3. Send global broadcast JSON commands to all devices
  static Future<void> sendBroadcastMessage(
    int port,
    Map<String, dynamic> jsonMap,
  ) async {
    if (_outboundSocket == null) {
      await initOutbound();
    }
    try {
      String jsonString = jsonEncode(jsonMap);
      List<int> bytesToSend = utf8.encode(jsonString);

      await _outboundSocket!.send(
        bytesToSend,
        Endpoint.broadcast(port: Port(port)),
      );
      print("📡 Broadcasted JSON to port $port -> $jsonString");
    } catch (e) {
      print("❌ Failed to send UDP broadcast packet: $e");
    }
  }

  static void startDeviceWatchdog(String deviceIp, Duration timeout) {
    activeWatchdogs[deviceIp]?.cancel(); // Terminate older timer instances

    activeWatchdogs[deviceIp] = Timer(timeout, () {
      handleTimeoutFailure(deviceIp);
    });
  }

  static void clearDeviceWatchdog(String deviceIp) {
    if (activeWatchdogs.containsKey(deviceIp)) {
      activeWatchdogs[deviceIp]?.cancel();
      activeWatchdogs.remove(deviceIp);
      print(
        "🎯 Response acknowledged within timeline. Watchdog cleared for: $deviceIp",
      );
    }
  }

  static void handleTimeoutFailure(String deviceIp) {
    print(
      "⚠️ Watchdog Tripped: Device at $deviceIp failed to respond. Forcing offline status.",
    );
    activeWatchdogs.remove(deviceIp);

    // Updates internal application state, forcing UI re-render and component gray-outs
    DevicesState.updateConnectionByIp(deviceIp, false);
  }

  static Future<void> closeSocket() async {
    if (_outboundSocket != null) {
      try {
        _outboundSocket!.close(); // Terminates the UDP instance
        _outboundSocket =
            null; // Resets reference for future initOutbound calls
        print("🔌 UDP Outbound Socket closed successfully.");
      } catch (e) {
        print("❌ Error closing UDP socket: $e");
      }
    }
  }
}

class MultiUdpService {
  // Keep track of all active listening sockets using their Port number as the Key
  static final Map<int, UDP> _activeSockets = {};
  static bool _wasDoorOpen = false;
  static bool _wasMotionDetected = false;

  // A list notifier so your UI can know exactly which ports are running
  static final ValueNotifier<List<int>> activePortsNotifier =
      ValueNotifier<List<int>>([]);

  static Future<void> startListening(int port) async {
    if (_activeSockets.containsKey(port)) {
      print("Already listening to port $port.");
      return;
    }

    try {
      // Bind uniquely to the requested device telemetry incoming port
      UDP newSocket = await UDP.bind(Endpoint.any(port: Port(port)));

      _activeSockets[port] = newSocket;
      activePortsNotifier.value = [...activePortsNotifier.value, port];
      print("📡 Opened subscription inbound socket on Port: $port");

      // Begin streaming packets for this specific port instance
      newSocket.asStream().listen(
        (datagram) {
          if (datagram == null || datagram.data.isEmpty) return;

          try {
            String senderIP = datagram.address.address;

            String rawJsonMessage = utf8.decode(datagram.data);
            Map<String, dynamic> responseData = jsonDecode(rawJsonMessage);

            UdpService.clearDeviceWatchdog(senderIP);
            if (responseData.containsKey('state10A') &&
                responseData.containsKey('state30A')) {
              String isOn = responseData['state10A'];
              String isOn30A = responseData['state30A'];
              print("⚡ Control device checked in from IP: $senderIP");
              DevicesState.updateStatusByIp(senderIP, isOn == "ON");
              DevicesState.update30AByIp(senderIP, isOn30A == "ON");
              DevicesState.updateConnectionByIp(senderIP, true);
            }
            // 1. Is it an ON/OFF Control Node?
            else if (responseData.containsKey('state10A')) {
              String isOn = responseData['state10A'];
              print("⚡ Control device checked in from IP: $senderIP");
              DevicesState.updateStatusByIp(senderIP, isOn == "ON");
              DevicesState.updateConnectionByIp(senderIP, true);
            } else if (responseData.containsKey('state30A')) {
              String isOn = responseData['state30A'];
              print("⚡ Control device checked in from IP: $senderIP");
              DevicesState.updateStatusByIp(senderIP, isOn == "ON");
              DevicesState.updateConnectionByIp(senderIP, true);
            } else if (responseData.containsKey('openPercentage')) {
              double openPercentage = responseData['openPercentage'];
              print("⚡ Curtain device checked in from IP: $senderIP");
              DevicesState.updateOpenPercentage(senderIP, openPercentage);
              DevicesState.updateConnectionByIp(senderIP, true);
            } else if (responseData.containsKey("isOnline")) {
              DevicesState.updateConnectionByIp(senderIP, true);
            }

            if (responseData.containsKey('temp') &&
                responseData.containsKey('humid')) {
              NetworkWatchdogManager.registerSensorPacketReceived(port);

              SensorNodeNotifiers.tempNotifier.value = responseData['temp']
                  .toString();
              SensorNodeNotifiers.humidNotifier.value = responseData['humid']
                  .toString();
              DevicesState.updateConnectionByPort(port.toString(), true);
            } else if (responseData.containsKey('ch4')) {
              String ch4Value = responseData['ch4'].toString();

              NetworkWatchdogManager.registerSensorPacketReceived(port);

              SensorNodeNotifiers.MethaneNotifier.value = ch4Value;

              DevicesState.updateConnectionByPort(port.toString(), true);
            } else if (responseData.containsKey('co2')) {
              String co2Value = responseData['co2'].toString();

              NetworkWatchdogManager.registerSensorPacketReceived(port);

              SensorNodeNotifiers.CO2Notifier.value = co2Value;

              DevicesState.updateConnectionByPort(port.toString(), true);
            } else if (responseData.containsKey('door')) {
              NetworkWatchdogManager.registerDoorPacketReceived(port);
              bool isDoorOpenNow = responseData['door'] ?? false;
              DoorNodeNotifiers.DoorStateNotifier.value = isDoorOpenNow;
              if (isDoorOpenNow) {
                // Condition: It is open NOW, but it was CLOSED during the last packet frame
                if (!_wasDoorOpen) {
                  NotificationService.ShowInstantNotification(
                    id: 6,
                    title: "Security Alert",
                    body: "The door contact sensor has detected an OPEN state!",
                  );

                  // Flip the flag to true so subsequent "open" packets are ignored
                  _wasDoorOpen = true;
                }
              } else {
                // The moment the door closes, reset the flag so it's ready to alert next time
                _wasDoorOpen = false;
              }
              DevicesState.updateConnectionByPort(port.toString(), true);
            } else if (responseData.containsKey('pir')) {
              NetworkWatchdogManager.registerPIRPacketReceived(port);
              bool isMotionDetectedNow =
                  bool.tryParse(
                    responseData['pir'].toString().trim().toLowerCase(),
                  ) ??
                  false;
              PIRNodeNotifiers.PIRStateNotifier.value = isMotionDetectedNow;
              if (isMotionDetectedNow) {
                // Condition: It is open NOW, but it was CLOSED during the last packet frame
                if (!_wasMotionDetected) {
                  NotificationService.ShowInstantNotification(
                    id: 9,
                    title: "Security Alert",
                    body: "Motion has been Detected!",
                  );

                  // Flip the flag to true so subsequent "open" packets are ignored
                  _wasMotionDetected = true;
                }
              } else {
                // The moment the door closes, reset the flag so it's ready to alert next time
                _wasMotionDetected = false;
              }
              DevicesState.updateConnectionByPort(port.toString(), true);
            } else if (responseData.containsKey("speed")) {
              WheelchairSpeedNotifier.value = responseData['speed'].toString();
              WheelchairisConnected.value = true;
            }
          } catch (jsonError) {
            print("Error processing packet on port $port: $jsonError");
          }
        },
        onError: (error) {
          print("Stream error on port $port: $error");
          endListeningToPort(port);
        },
      );
    } catch (e) {
      print("Fatal error starting listener on port $port: $e");
      _activeSockets.remove(port);
    }
  }

  static void endListeningToPort(int port) {
    if (_activeSockets.containsKey(port)) {
      _activeSockets[port]!.close();
      _activeSockets.remove(port);

      activePortsNotifier.value = activePortsNotifier.value
          .where((p) => p != port)
          .toList();
      print("🔌 Port $port closed and cleaned up successfully.");
    }
  }

  static void endAllPorts() {
    List<int> openPorts = _activeSockets.keys.toList();
    for (int port in openPorts) {
      endListeningToPort(port);
    }
  }
}
