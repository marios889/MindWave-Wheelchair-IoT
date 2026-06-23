import 'package:flutter/material.dart';
import 'package:wheelchair_smarthome/data/models/smart_device.dart';
import 'package:wheelchair_smarthome/data/notifiers/devices_state.dart';
import 'package:wheelchair_smarthome/views/widgets/dialogs/change_device_ip_widget.dart';
import 'package:wheelchair_smarthome/views/widgets/dialogs/change_device_name_widget.dart';

class SettingsDeviceCard extends StatelessWidget {
  const SettingsDeviceCard({super.key});
  final bool isOnline = false;
  @override
  Widget build(BuildContext context) {
    return ValueListenableBuilder(
      valueListenable: DevicesState.devicesNotifier,
      builder: (context, currentDevices, child) {
        if (currentDevices.isEmpty) {
          return const Center(child: Text("No devices added yet."));
        } else {
          final Map<String, List<SmartDevice>> groupedRooms = {};
          for (var device in currentDevices) {
            groupedRooms.putIfAbsent(device.roomName, () => []).add(device);
          }

          final roomNames = groupedRooms.keys.toList();

          return ListView.builder(
            shrinkWrap:
                true, // 1. Tells the list to hug its children's height instead of expanding infinitely
            physics:
                const NeverScrollableScrollPhysics(), // 2. Stops this list from fighting its parent scrollable
            itemCount: roomNames.length,
            itemBuilder: (context, index) {
              final currentRoomName = roomNames[index];
              final devicesInThisRoom = groupedRooms[currentRoomName]!;

              return Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                mainAxisSize: MainAxisSize.min,
                children: [
                  // THE ROOM HEADER TITLE CAPSULE
                  Padding(
                    padding: const EdgeInsets.symmetric(
                      horizontal: 16.0,
                      vertical: 10.0,
                    ),
                    child: Text(
                      currentRoomName.toUpperCase(),
                      style: TextStyle(
                        fontSize: 15,
                        fontWeight: FontWeight.bold,
                        color: Theme.of(context).colorScheme.primary,
                        letterSpacing: 1.4,
                      ),
                    ),
                  ),

                  // UNROLL DEVICES
                  ...devicesInThisRoom.map((device) {
                    return ListTile(
                      contentPadding: const EdgeInsets.symmetric(
                        horizontal: 16.0,
                        vertical: 4.0,
                      ),
                      leading: Icon(
                        size: 33,
                        device.type == DeviceType.lamp
                            ? Icons.lightbulb
                            : device.type == DeviceType.curtains
                            ? Icons.curtains
                            : device.type == DeviceType.Device
                            ? Icons.power
                            : device.type == DeviceType.DoorContact
                            ? Icons.door_front_door
                            : device.type == DeviceType.lamp_device
                            ? Icons.lightbulb_sharp
                            : device.type == DeviceType.sensorNode
                            ? Icons.sensors
                            : Icons.directions_walk,
                      ),
                      title: Row(
                        children: [
                          ValueListenableBuilder(
                            valueListenable: device.isOnline,
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
                                          ? Colors.greenAccent.withValues(
                                              alpha: 0.4,
                                            )
                                          : Colors.redAccent.withValues(
                                              alpha: 0.4,
                                            ),
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
                            device.name,
                            style: const TextStyle(
                              color: Colors.amber,
                              fontSize: 18.0,
                              fontWeight: FontWeight.bold,
                            ),
                          ),
                        ],
                      ),
                      subtitle: Text(
                        device.type == DeviceType.sensorNode ||
                                device.type == DeviceType.DoorContact ||
                                device.type == DeviceType.PIRsensor
                            ? "Port : ${device.port}"
                            : "IP : ${device.ip}:${device.port}",
                      ),

                      trailing: PopupMenuButton<String>(
                        icon: const Icon(Icons.more_vert),
                        onSelected: (String choice) {
                          if (choice == 'edit') {
                            showDialog(
                              context: context,
                              builder: (context) {
                                return ChangeDeviceIP(device: device);
                              },
                            );
                          } else if (choice == 'delete') {
                            DevicesState.deleteDevice(device.id);
                          } else if (choice == 'edit Name') {
                            showDialog(
                              context: context,
                              builder: (context) {
                                return ChangeDeviceName(device: device);
                              },
                            );
                          }
                        },
                        itemBuilder: (BuildContext context) {
                          return [
                            const PopupMenuItem<String>(
                              value: 'edit Name',
                              child: Row(
                                children: [
                                  Icon(Icons.edit_note, color: Colors.amber),
                                  SizedBox(width: 8),
                                  Text('Edit Name'),
                                ],
                              ),
                            ),
                            const PopupMenuItem<String>(
                              value: 'edit',
                              child: Row(
                                children: [
                                  Icon(Icons.edit, color: Colors.blue),
                                  SizedBox(width: 8),
                                  Text('Edit IP'),
                                ],
                              ),
                            ),
                            const PopupMenuItem<String>(
                              value: 'delete',
                              child: Row(
                                children: [
                                  Icon(Icons.delete, color: Colors.red),
                                  SizedBox(width: 8),
                                  Text('Delete Device'),
                                ],
                              ),
                            ),
                          ];
                        },
                      ),
                    );
                  }),

                  // VISUAL DIVIDER: Separates room clusters cleanly
                  if (index < roomNames.length - 1)
                    const Divider(thickness: 1, indent: 16, endIndent: 16),
                ],
              );
            },
          );
        }
      },
    );
  }
}
