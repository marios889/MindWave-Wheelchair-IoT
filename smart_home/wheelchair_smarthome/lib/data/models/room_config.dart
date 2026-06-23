import 'package:flutter/material.dart';

class RoomConfig {
  final String name;
  final IconData icon;
  final List<Color> gradientColors;

  RoomConfig({
    required this.name,
    required this.icon,
    required this.gradientColors,
  });
}

// The exact list of your 4 target rooms with modern color palettes
final List<RoomConfig> standardRooms = [
  RoomConfig(
    name: "Living Room",
    icon: Icons.chair_outlined,
    gradientColors: [Colors.teal[300]!, Colors.teal[600]!],
  ),
  RoomConfig(
    name: "Bedroom",
    icon: Icons.bed_outlined,
    gradientColors: [Colors.indigo[300]!, Colors.indigo[600]!],
  ),
  RoomConfig(
    name: "Kitchen",
    icon: Icons.countertops_outlined,
    gradientColors: [Colors.orange[300]!, Colors.orange[600]!],
  ),
  RoomConfig(
    name: "Bathroom",
    icon: Icons.bathtub_outlined,
    gradientColors: [Colors.cyan[300]!, Colors.cyan[600]!],
  ),
];
