import 'package:flutter/material.dart';
import 'package:wheelchair_smarthome/data/models/room_config.dart';
import 'package:wheelchair_smarthome/data/notifiers/devices_state.dart';
import 'package:wheelchair_smarthome/views/pages/devices_page.dart';

class RoomCard extends StatelessWidget {
  const RoomCard({super.key, required this.room});
  final RoomConfig room;
  @override
  Widget build(BuildContext context) {
    return InkWell(
      onTap: () {
        Navigator.push(
          context,
          MaterialPageRoute(
            builder: (context) {
              return DevicesPage(room: room);
            },
          ),
        );
      },
      borderRadius: BorderRadius.circular(24.0),
      child: Container(
        padding: const EdgeInsets.all(16.0),
        decoration: BoxDecoration(
          borderRadius: BorderRadius.circular(24.0),
          gradient: LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: [
              room.gradientColors[0].withValues(alpha: 0.15),
              room.gradientColors[1].withValues(alpha: 0.05),
            ],
          ),
          border: Border.all(
            color: room.gradientColors[0].withValues(alpha: 0.3),
            width: 1.5,
          ),
        ),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Container(
              padding: const EdgeInsets.all(10),
              decoration: BoxDecoration(
                color: room.gradientColors[0].withValues(alpha: 0.2),
                shape: BoxShape.circle,
              ),
              child: Icon(room.icon, color: room.gradientColors[1], size: 28),
            ),
            Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  room.name,
                  style: const TextStyle(
                    fontSize: 18,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                const SizedBox(height: 4),
                ValueListenableBuilder(
                  valueListenable: DevicesState.devicesNotifier,
                  builder: (context, devices, child) {
                    int deviceCount = devices
                        .where((device) => device.roomName == room.name)
                        .toList()
                        .length;
                    return Text(
                      "$deviceCount connected",
                      style: TextStyle(
                        fontSize: 13,
                        color: Theme.of(
                          context,
                        ).colorScheme.onSurfaceVariant.withValues(alpha: 0.7),
                      ),
                    );
                  },
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}
