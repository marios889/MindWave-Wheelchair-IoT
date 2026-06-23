import 'package:flutter/material.dart';
import 'package:network_info_plus/network_info_plus.dart';
import 'package:wheelchair_smarthome/data/models/smart_device.dart';
import 'package:wheelchair_smarthome/data/notifiers/notifiers.dart';
import 'package:wheelchair_smarthome/data/services/network_event_service.dart';
import 'package:wheelchair_smarthome/data/services/network_watchdog_manager.dart';
import 'package:wheelchair_smarthome/data/services/udp_service.dart';
import 'package:wheelchair_smarthome/views/widgets/dialogs/popupmenu_dialog.dart';

class DoorContactDevice extends StatefulWidget {
  final SmartDevice device;

  const DoorContactDevice({super.key, required this.device});

  @override
  State<DoorContactDevice> createState() => _FullWidthSensorCardState();
}

class _FullWidthSensorCardState extends State<DoorContactDevice> {
  @override
  void initState() {
    super.initState();
    if (!NetworkWatchdogManager.isDoorStreaming()) {
      _sendDedicatedDoorPing();
    } else {
      print(
        "🛡️ Watchdog: Door packets already incoming on page entry. Suppressed ping.",
      );
    }

    NetworkEventService.networkTrigger.addListener(_onNetworkBroadcast);
  }

  void _onNetworkBroadcast() {
    if (NetworkEventService.networkTrigger.value ==
        NetworkBroadcastType.doorContactCheck) {
      _sendDedicatedDoorPing();
    }
  }

  Future<String?> _getPrivateIp() async {
    final info = NetworkInfo();
    String? ip = await info.getWifiIP();
    return ip; // Fetches the local IP (e.g., 192.168.1.X)
  }

  Future<void> _sendDedicatedDoorPing() async {
    try {
      String? phoneIp = await _getPrivateIp();
      if (phoneIp != null) {
        await MultiUdpService.startListening(int.parse(widget.device.port));
        for (int i = 0; i < 3; i++) {
          UdpService.sendBroadcastMessage(7777, {
            "port": int.parse(widget.device.port),
            "command": "door_data",
          });
          await Future.delayed(const Duration(milliseconds: 300));
        }
      }
    } catch (e) {
      print("❌ Error starting page-level UDP tracking: $e");
    }
  }

  @override
  void dispose() {
    NetworkEventService.networkTrigger.removeListener(_onNetworkBroadcast);
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onLongPress: () => PopupmenuDialog.showPopupMenu(context, widget.device),
      child: Container(
        padding: const EdgeInsets.all(20.0),
        decoration: BoxDecoration(
          color: const Color(0xFF1E2232), // Dark card surface style
          borderRadius: BorderRadius.circular(24.0),
          border: Border.all(
            color: Colors.cyan.withValues(alpha: 0.2),
            width: 1.5,
          ),
        ),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            // HEADER CLUSTER
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Container(
                  decoration: BoxDecoration(shape: BoxShape.circle),
                  child: SizedBox(
                    width: 50.0,
                    height: 50.0,
                    child: Image.asset(
                      "assets/images/smart-door.png",
                      fit: BoxFit.cover,
                    ),
                  ),
                ),
                Text(
                  "Port: ${widget.device.port}",
                  style: const TextStyle(
                    color: Colors.white,
                    fontSize: 14,
                    fontWeight: FontWeight.bold,
                  ),
                ),
              ],
            ),

            Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Text(
                  "Status: ",
                  style: const TextStyle(
                    color: Colors.white,
                    fontSize: 16,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                ValueListenableBuilder(
                  valueListenable: DoorNodeNotifiers.DoorStateNotifier,
                  builder: (context, state, child) {
                    return Text(
                      state ? "Opened" : "Closed",
                      style: TextStyle(
                        color: state ? Colors.greenAccent : Colors.red,
                        fontSize: 16,
                        fontWeight: FontWeight.bold,
                      ),
                    );
                  },
                ),
              ],
            ),
            Center(
              child: ValueListenableBuilder(
                valueListenable: MultiUdpService.activePortsNotifier,
                builder: (context, activeList, child) {
                  bool isThisPortRunning = activeList.contains(
                    int.parse(widget.device.port),
                  );

                  return ElevatedButton.icon(
                    style: ElevatedButton.styleFrom(
                      backgroundColor: isThisPortRunning
                          ? Colors.redAccent
                          : Colors.green,
                      foregroundColor: Colors.white,
                      padding: const EdgeInsets.symmetric(
                        horizontal: 14,
                        vertical: 12,
                      ),
                    ),
                    // The smart toggle conditional expression:
                    onPressed: () {
                      if (isThisPortRunning) {
                        MultiUdpService.endListeningToPort(
                          int.parse(widget.device.port),
                        );
                      } else {
                        MultiUdpService.startListening(
                          int.parse(widget.device.port),
                        );
                      }
                    },
                    icon: Icon(
                      isThisPortRunning ? Icons.stop_circle : Icons.play_circle,
                    ),
                    label: Text(
                      isThisPortRunning ? "STOP" : "LISTEN",
                      style: const TextStyle(fontWeight: FontWeight.bold),
                    ),
                  );
                },
              ),
            ),
            Row(
              children: [
                ValueListenableBuilder(
                  valueListenable: widget.device.isOnline,
                  builder: (context, isOnline, child) {
                    return Container(
                      width: 10.0,
                      height: 10.0,
                      decoration: BoxDecoration(
                        color: isOnline
                            ? Colors.greenAccent[700]
                            : Colors.redAccent[700],
                        shape: BoxShape.circle,
                        boxShadow: [
                          BoxShadow(
                            color: isOnline
                                ? Colors.greenAccent.withValues(alpha: 0.4)
                                : Colors.redAccent.withValues(alpha: 0.4),
                            blurRadius: 6.0,
                            spreadRadius: 2.0,
                          ),
                        ],
                      ),
                    );
                  },
                ),

                const SizedBox(width: 10.0),
                Text(
                  widget.device.name,
                  style: const TextStyle(
                    color: Colors.white,
                    fontSize: 15,
                    fontWeight: FontWeight.bold,
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}
