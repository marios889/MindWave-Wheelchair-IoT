import 'dart:async';
import 'package:flutter/material.dart';
import 'package:network_info_plus/network_info_plus.dart';
import 'package:wheelchair_smarthome/data/models/room_config.dart';
import 'package:wheelchair_smarthome/data/models/smart_device.dart';
import 'package:wheelchair_smarthome/data/notifiers/devices_state.dart';
import 'package:wheelchair_smarthome/data/services/network_event_service.dart';
import 'package:wheelchair_smarthome/data/services/network_watchdog_manager.dart';
import 'package:wheelchair_smarthome/data/services/udp_service.dart';
import 'package:wheelchair_smarthome/views/widgets/cards/DoorContact_card.dart';
import 'package:wheelchair_smarthome/views/widgets/cards/Lamp_Device_card.dart';
import 'package:wheelchair_smarthome/views/widgets/cards/PIRsensor_card.dart';
import 'package:wheelchair_smarthome/views/widgets/cards/curtain_card.dart';
import 'package:wheelchair_smarthome/views/widgets/cards/modern_device_card_widget.dart';
import 'package:wheelchair_smarthome/views/widgets/dialogs/new_device_dialog_widget.dart';
import 'package:wheelchair_smarthome/views/widgets/cards/sensor_node_card_widget.dart';

class DevicesPage extends StatefulWidget {
  const DevicesPage({super.key, required this.room});
  final RoomConfig room;

  @override
  State<DevicesPage> createState() => _DevicesPageState();
}

class _DevicesPageState extends State<DevicesPage> {
  // Timer? _statusPollTimer;
  @override
  void initState() {
    super.initState();
    if (DevicesState.checkForDevices(widget.room.name)) {
      _initializeNetworkDiscovery();
    }

    // _startPeriodicNetworkPings();
    NetworkEventService.networkTrigger.addListener(_handleNetworkEvents);
  }

  // void _startPeriodicNetworkPings() {
  //   // 💡 3. Setup the periodic executor loop interval
  //   _statusPollTimer = Timer.periodic(const Duration(seconds: 30), (timer) {
  //     if (mounted) {
  //       print("🕒 30-Second Loop: Requesting device status refresh...");
  //       NetworkEventService.fireBroadcast(NetworkBroadcastType.fullRoomStats);
  //     }
  //   });
  // }

  void _handleNetworkEvents() {
    if (NetworkEventService.networkTrigger.value ==
        NetworkBroadcastType.fullRoomStats) {
      _initializeNetworkDiscovery();
    }
  }

  Future<String?> _getPrivateIp() async {
    final info = NetworkInfo();
    String? ip = await info.getWifiIP();
    return ip; // Fetches the local IP (e.g., 192.168.1.X)
  }

  Future<void> _initializeNetworkDiscovery() async {
    try {
      // 1. Reset all saved device green dots to grey/false instantly on entry
      DevicesState.resetAllToOffline();
      DevicesState.resetAllStates();

      // 2. Open up your app's inbound gateway pipeline subscription

      // 3. Blast the broadcast out to wake up the ESPs in the room
      String? phoneIp = await _getPrivateIp();
      if (phoneIp != null) {
        await MultiUdpService.startListening(5003);
        for (int i = 0; i < 3; i++) {
          UdpService.sendBroadcastMessage(7777, {
            "port": 5003,
            "command": "full_stats",
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
    NetworkEventService.networkTrigger.removeListener(_handleNetworkEvents);
    // _statusPollTimer?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text(widget.room.name), centerTitle: true),
      body: RefreshIndicator(
        color: Theme.of(context).colorScheme.primary,
        backgroundColor: Theme.of(context).colorScheme.surface,
        onRefresh: () async {
          // 1. Reset the UI state immediately so the user gets instant visual feedback
          DevicesState.resetAllToOffline();
          DevicesState.resetAllStates();
          // _statusPollTimer?.cancel();
          // _startPeriodicNetworkPings();

          // 2. Fire the generic room stats broadcast (Wakes up Lamps, Curtains, etc.)
          // This triggers the local _initializeNetworkDiscovery() method on this page
          if (DevicesState.checkForDevices(widget.room.name)) {
            NetworkEventService.fireBroadcast(
              NetworkBroadcastType.fullRoomStats,
            );
          }

          if (!NetworkWatchdogManager.isSensorStreaming()) {
            print(
              "🔄 Refresh: Sensor data inactive. Firing broadcast pull request.",
            );
            NetworkEventService.fireBroadcast(
              NetworkBroadcastType.sensorDataPull,
            );
          } else {
            print(
              "🛡️ Watchdog: Sensor data actively flowing. Suppressing broadcast request.",
            );
          }

          // 💡 4. Smart Watchdog Evaluation for Door Contacts
          if (!NetworkWatchdogManager.isDoorStreaming()) {
            print(
              "🔄 Refresh: Door data inactive. Firing broadcast check request.",
            );
            NetworkEventService.fireBroadcast(
              NetworkBroadcastType.doorContactCheck,
            );
          } else {
            print(
              "🛡️ Watchdog: Door data actively flowing. Suppressing broadcast request.",
            );
          }
          // 💡 5. Smart Watchdog Evaluation for PIR Sensor
          if (!NetworkWatchdogManager.isPirStreaming()) {
            print(
              "🔄 Refresh: Door data inactive. Firing broadcast check request.",
            );
            NetworkEventService.fireBroadcast(
              NetworkBroadcastType.pirSensorCheck,
            );
          } else {
            print(
              "🛡️ Watchdog: Door data actively flowing. Suppressing broadcast request.",
            );
          }

          // Provide a slight fallback delay so the indicator doesn't snap away too instantly
          await Future.delayed(const Duration(milliseconds: 800));
        },
        child: SingleChildScrollView(
          physics: const AlwaysScrollableScrollPhysics(),
          child: ValueListenableBuilder(
            valueListenable: DevicesState.devicesNotifier,
            builder: (context, currentDevices, child) {
              List<SmartDevice> roomDevices = currentDevices
                  .where((device) => device.roomName == widget.room.name)
                  .toList();

              final sensorDevices = roomDevices
                  .where((d) => d.type == DeviceType.sensorNode)
                  .toList();
              final controlDevices = roomDevices
                  .where(
                    (d) =>
                        d.type != DeviceType.sensorNode &&
                        d.type != DeviceType.curtains,
                  )
                  .toList();
              final curtainDevices = roomDevices.where(
                (d) => d.type == DeviceType.curtains,
              );
              if (roomDevices.isEmpty) {
                return Container(
                  alignment: Alignment.center,
                  height:
                      MediaQuery.of(context).size.height *
                      0.7, // Takes up screen space so you can swipe down on it
                  child: const Text("No devices added yet."),
                );
              } else {
                return Padding(
                  padding: const EdgeInsets.all(15.0),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      // 2. FULL-ROW SENSOR CARDS CONTAINER
                      if (sensorDevices.isNotEmpty) ...[
                        ...sensorDevices.map((sensor) {
                          return Padding(
                            padding: const EdgeInsets.only(bottom: 16.0),
                            child: FullWidthSensorCard(
                              device: sensor,
                            ), // ◄ The widescreen card blueprint
                          );
                        }),
                      ],
                      if (curtainDevices.isNotEmpty) ...[
                        ...curtainDevices.map((curtain) {
                          return Padding(
                            padding: const EdgeInsets.only(bottom: 16.0),
                            child: CurtainCard(
                              device: curtain,
                            ), // ◄ The widescreen card blueprint
                          );
                        }),
                      ],
                      // 3. TWO-COLUMN GRID FOR STANDARD CONTROL DEVICES
                      GridView.builder(
                        shrinkWrap:
                            true, // Crucial: lets GridView safely live inside a SingleChildScrollView
                        physics:
                            const NeverScrollableScrollPhysics(), // Disables nested scroll fights
                        gridDelegate:
                            const SliverGridDelegateWithFixedCrossAxisCount(
                              crossAxisCount: 2,
                              crossAxisSpacing: 12,
                              mainAxisSpacing: 12,
                              childAspectRatio: 0.80,
                            ),
                        itemCount: controlDevices.length,
                        itemBuilder: (context, index) {
                          return controlDevices[index].type ==
                                  DeviceType.DoorContact
                              ? DoorContactDevice(device: controlDevices[index])
                              : controlDevices[index].type ==
                                    DeviceType.PIRsensor
                              ? PirsensorCard(device: controlDevices[index])
                              : controlDevices[index].type == DeviceType.lamp ||
                                    controlDevices[index].type ==
                                        DeviceType.Device
                              ? ModernDeviceCard(device: controlDevices[index])
                              : LampDeviceCard(device: controlDevices[index]);
                        },
                      ),
                    ],
                  ),
                );
              }
            },
          ),
        ),
      ),
      floatingActionButtonLocation: FloatingActionButtonLocation.centerFloat,

      floatingActionButton: Padding(
        // Increase the bottom value to push the button higher up the screen
        padding: const EdgeInsets.only(bottom: 20.0),
        child: SizedBox(
          width: 60, // Standard FAB is 56, increasing this makes it bigger
          height: 60,
          child: FloatingActionButton(
            onPressed: () {
              showDialog(
                context: context,
                builder: (context) {
                  return NewDeviceDialogWidget(room: widget.room.name);
                },
              );
            },
            shape: const CircleBorder(),
            // Make the internal plus icon scale up beautifully with the larger button
            child: const Icon(Icons.add, size: 30),
          ),
        ),
      ),
    );
  }
}
