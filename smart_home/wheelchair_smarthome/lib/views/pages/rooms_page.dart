import 'package:flutter/material.dart';
import 'package:wheelchair_smarthome/data/models/room_config.dart';
import 'package:wheelchair_smarthome/views/widgets/cards/RoomCard_widget.dart';

class RoomsPage extends StatelessWidget {
  const RoomsPage({super.key});

  @override
  Widget build(BuildContext context) {
    // We return a Padding or a Container directly instead of a full Scaffold page
    return Padding(
      padding: const EdgeInsets.all(16.0),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Text(
            "My Home",
            style: TextStyle(
              fontWeight: FontWeight.bold,
              fontSize: 26,
              letterSpacing: -0.5,
            ),
          ),
          const SizedBox(height: 16),

          // Expanded ensures the GridView fills the remaining available screen space cleanly
          Expanded(
            child: GridView.count(
              crossAxisCount: 2,
              crossAxisSpacing: 16,
              mainAxisSpacing: 16,
              childAspectRatio: 1.1,
              children: standardRooms.map((room) {
                return RoomCard(room: room);
              }).toList(),
            ),
          ),
        ],
      ),
    );
  }
}
