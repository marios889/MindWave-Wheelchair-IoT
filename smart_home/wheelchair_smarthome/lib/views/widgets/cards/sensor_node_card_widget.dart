import 'package:flutter/material.dart';
import 'package:network_info_plus/network_info_plus.dart';
import 'package:wheelchair_smarthome/data/models/smart_device.dart';
import 'package:wheelchair_smarthome/data/notifiers/notifiers.dart';
import 'package:wheelchair_smarthome/data/services/network_event_service.dart';
import 'package:wheelchair_smarthome/data/services/network_watchdog_manager.dart';
import 'package:wheelchair_smarthome/data/services/notification_service.dart';
import 'package:wheelchair_smarthome/data/services/udp_service.dart';
import 'package:wheelchair_smarthome/views/widgets/dialogs/popupmenu_dialog.dart';

class FullWidthSensorCard extends StatefulWidget {
  final SmartDevice device;

  const FullWidthSensorCard({super.key, required this.device});

  @override
  State<FullWidthSensorCard> createState() => _FullWidthSensorCardState();
}

class _FullWidthSensorCardState extends State<FullWidthSensorCard> {
  @override
  void initState() {
    super.initState();
    if (!NetworkWatchdogManager.isSensorStreaming()) {
      _sendDedicatedSensorPing();
    } else {
      print(
        "🛡️ Watchdog: Sensor packets already incoming on page entry. Suppressed ping.",
      );
    }

    NetworkEventService.networkTrigger.addListener(_onNetworkBroadcast);
  }

  void _onNetworkBroadcast() {
    if (NetworkEventService.networkTrigger.value ==
        NetworkBroadcastType.sensorDataPull) {
      _sendDedicatedSensorPing();
    }
  }

  Future<String?> _getPrivateIp() async {
    final info = NetworkInfo();
    String? ip = await info.getWifiIP();
    return ip; // Fetches the local IP (e.g., 192.168.1.X)
  }

  Future<void> _sendDedicatedSensorPing() async {
    try {
      String? phoneIp = await _getPrivateIp();
      if (phoneIp != null) {
        await MultiUdpService.startListening(int.parse(widget.device.port));
        for (int i = 0; i < 3; i++) {
          UdpService.sendBroadcastMessage(7777, {
            "port": int.parse(widget.device.port),
            "command": "send_sensor_data",
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
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            // HEADER CLUSTER
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Row(
                  mainAxisAlignment: MainAxisAlignment.start,
                  children: [
                    const Icon(
                      Icons.sensors_rounded,
                      color: Colors.cyanAccent,
                      size: 26,
                    ),
                    const SizedBox(width: 8.0),
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

                    const SizedBox(width: 7.0),
                    Text(
                      widget.device.name,
                      style: const TextStyle(
                        color: Colors.white,
                        fontSize: 16,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                  ],
                ),

                Text(
                  "Port: ${widget.device.port}",
                  style: const TextStyle(
                    color: Colors.white,
                    fontSize: 16,
                    fontWeight: FontWeight.bold,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 20),

            // SIDE-BY-SIDE READING TELEMETRY DATA
            Row(
              children: [
                // TEMPERATURE FIELD BLOCK
                Expanded(
                  child: Row(
                    children: [
                      const Icon(
                        Icons.thermostat_rounded,
                        color: Colors.orangeAccent,
                        size: 32,
                      ),
                      const SizedBox(width: 8),
                      Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          ValueListenableBuilder(
                            valueListenable: SensorNodeNotifiers.tempNotifier,
                            builder: (context, tempVal, child) {
                              return Text(
                                "$tempVal °C",
                                style: const TextStyle(
                                  color: Colors.white,
                                  fontSize: 24,
                                  fontWeight: FontWeight.bold,
                                ),
                              );
                            },
                          ),
                          const Text(
                            "Temperature",
                            style: TextStyle(
                              color: Colors.white70,
                              fontSize: 15,
                              fontWeight: FontWeight.bold,
                            ),
                          ),
                        ],
                      ),
                    ],
                  ),
                ),

                // VERTICAL DIVIDER BAR
                Container(height: 40, width: 1, color: Colors.white10),
                const SizedBox(width: 16),

                // HUMIDITY FIELD BLOCK
                Expanded(
                  child: Row(
                    children: [
                      const Icon(
                        Icons.water_drop_rounded,
                        color: Colors.blueAccent,
                        size: 30,
                      ),
                      const SizedBox(width: 8),
                      Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          ValueListenableBuilder(
                            valueListenable: SensorNodeNotifiers.humidNotifier,
                            builder: (context, humidvalue, child) {
                              return Text(
                                "$humidvalue %",
                                style: const TextStyle(
                                  color: Colors.white,
                                  fontSize: 24,
                                  fontWeight: FontWeight.bold,
                                ),
                              );
                            },
                          ),
                          const Text(
                            "Humidity",
                            style: TextStyle(
                              color: Colors.white70,
                              fontSize: 15,
                              fontWeight: FontWeight.bold,
                            ),
                          ),
                        ],
                      ),
                    ],
                  ),
                ),
              ],
            ),
            //---------------------------------------------------------
            const SizedBox(height: 30.0),
            Row(
              children: [
                // METHANE FIELD BLOCK
                Expanded(
                  child: Row(
                    children: [
                      const Icon(
                        Icons.gas_meter_sharp,
                        color: Colors.red,
                        size: 32,
                      ),
                      const SizedBox(width: 8),
                      Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          ValueListenableBuilder(
                            valueListenable:
                                SensorNodeNotifiers.MethaneNotifier,
                            builder: (context, methanevalue, child) {
                              int methaneval = int.tryParse(methanevalue) ?? 0;
                              double methanevaldouble =
                                  double.tryParse(methanevalue) ?? 0.0;
                              if (methaneval >= 15) {
                                NotificationService.ShowInstantNotification(
                                  id: 10,
                                  title: "Alert",
                                  body: "Methane Value is too High",
                                );
                              }
                              if (methanevaldouble >= 15.0) {
                                NotificationService.ShowInstantNotification(
                                  id: 10,
                                  title: "Alert",
                                  body: "Methane Value is too High",
                                );
                              }
                              return Text(
                                "$methanevalue %",
                                style: const TextStyle(
                                  color: Colors.white,
                                  fontSize: 24,
                                  fontWeight: FontWeight.bold,
                                ),
                              );
                            },
                          ),
                          const Text(
                            "Methane",
                            style: TextStyle(
                              color: Colors.white70,
                              fontSize: 15,
                              fontWeight: FontWeight.bold,
                            ),
                          ),
                        ],
                      ),
                    ],
                  ),
                ),

                // VERTICAL DIVIDER BAR
                Container(height: 40, width: 1, color: Colors.white10),
                const SizedBox(width: 16),

                // Co2 FIELD BLOCK
                Expanded(
                  child: Row(
                    children: [
                      const Icon(
                        Icons.gas_meter,
                        color: Colors.cyanAccent,
                        size: 30,
                      ),
                      const SizedBox(width: 8),
                      Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          ValueListenableBuilder(
                            valueListenable: SensorNodeNotifiers.CO2Notifier,
                            builder: (context, co2value, child) {
                              int co2val = int.tryParse(co2value) ?? 0;
                              double co2valdouble =
                                  double.tryParse(co2value) ?? 0.0;
                              if (co2val >= 50) {
                                NotificationService.ShowInstantNotification(
                                  id: 11,
                                  title: "Alert",
                                  body: "Co2 Value is too High!",
                                );
                              }
                              if (co2valdouble >= 50.0) {
                                NotificationService.ShowInstantNotification(
                                  id: 11,
                                  title: "Alert",
                                  body: "Co2 Value is too High!",
                                );
                              }
                              return Text(
                                "$co2value %",
                                style: const TextStyle(
                                  color: Colors.white,
                                  fontSize: 24,
                                  fontWeight: FontWeight.bold,
                                ),
                              );
                            },
                          ),
                          const Text(
                            "CO2",
                            style: TextStyle(
                              color: Colors.white70,
                              fontSize: 15,
                              fontWeight: FontWeight.bold,
                            ),
                          ),
                        ],
                      ),
                    ],
                  ),
                ),
              ],
            ),
            SizedBox(height: 20.0),
            Align(
              alignment: Alignment.bottomRight,
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
                        horizontal: 24,
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
                      isThisPortRunning ? "STOP LISTENING" : "START LISTENING",
                      style: const TextStyle(fontWeight: FontWeight.bold),
                    ),
                  );
                },
              ),
            ),
          ],
        ),
      ),
    );
  }
}
