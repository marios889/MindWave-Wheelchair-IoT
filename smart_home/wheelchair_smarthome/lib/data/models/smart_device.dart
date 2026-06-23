import 'package:flutter/material.dart';

enum DeviceType {
  lamp,
  curtains,
  sensorNode,
  Device,
  DoorContact,
  lamp_device,
  PIRsensor,
}

enum Rooms { LivingRoom, Bedroom, Kitchen, Bathroom }

class SmartDevice {
  final String id;
  final String name;
  final String ip;
  final String port;
  final DeviceType type;
  final String roomName;
  final ValueNotifier<bool> isOn;
  ValueNotifier<bool> isOnline;
  ValueNotifier<double> sliderValue;
  ValueNotifier<bool> isOn30A;
  SmartDevice({
    required this.id,
    required this.name,
    required this.ip,
    required this.port,
    required this.type,
    required this.roomName,
    ValueNotifier<bool>? isOn,
    ValueNotifier<bool>? isOnline,
    ValueNotifier<double>? sliderValue,
    ValueNotifier<bool>? switchRelay,
    ValueNotifier<bool>? isOn30A,
  }) : isOn = isOn ?? ValueNotifier<bool>(false),
       isOnline = isOnline ?? ValueNotifier<bool>(false),
       sliderValue = sliderValue ?? ValueNotifier<double>(10.0),
       isOn30A = isOn30A ?? ValueNotifier<bool>(false);

  // Convert a SmartDevice into a Map (JSON)
  Map<String, dynamic> toMap() {
    return {
      'id': id,
      'name': name,
      'ip': ip,
      'port': port,
      'type': type.name, // Saves enum as String (e.g., "lamp")
      'roomName': roomName,
      'isOn': isOn.value,
      'sliderValue': sliderValue.value,
      'isOn30A': isOn30A.value,
    };
  }

  // Create a SmartDevice from a Map (JSON)
  factory SmartDevice.fromMap(Map<String, dynamic> map) {
    return SmartDevice(
      id: map['id'] ?? '',
      name: map['name'] ?? '',
      ip: map['ip'] ?? '',
      port: map['port'] ?? '',
      type: DeviceType.values.byName(map['type'] ?? 'lamp'),
      roomName: map['roomName'] ?? '',
      isOn: ValueNotifier<bool>(map['isOn'] ?? false),
      sliderValue: ValueNotifier<double>(map['sliderValue'] ?? 10.0),
      isOn30A: ValueNotifier<bool>(map['isOn30A'] ?? false),
    );
  }
}
