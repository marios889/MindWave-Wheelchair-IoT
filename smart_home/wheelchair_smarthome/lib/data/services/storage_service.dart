import 'dart:convert';
import 'package:shared_preferences/shared_preferences.dart';
import '../models/smart_device.dart'; // Ensure your SmartDevice model is imported

class StorageService {
  static const String _storageKey = 'smart_home_devices';

  // 1. Fetch all saved devices statically
  static Future<List<SmartDevice>> getDevices() async {
    final prefs = await SharedPreferences.getInstance();
    final String? devicesJson = prefs.getString(_storageKey);
    if (devicesJson == null) {
      return [
        SmartDevice(
          id: "no",
          name: "No device",
          ip: "No devices saved",
          port: "no",
          type: DeviceType.lamp,
          roomName: "Unknown",
        ),
      ];
    }

    try {
      final List<dynamic> decodedList = jsonDecode(devicesJson);
      return decodedList.map((item) => SmartDevice.fromMap(item)).toList();
    } catch (e) {
      print("Error loading devices: $e");
      return [];
    }
  }

  // 2. Save the entire list of devices statically
  static Future<void> saveDevices(List<SmartDevice> devices) async {
    final prefs = await SharedPreferences.getInstance();
    final List<Map<String, dynamic>> mapList = devices
        .map((d) => d.toMap())
        .toList();
    final String encodedString = jsonEncode(mapList);

    await prefs.setString(_storageKey, encodedString);
  }

  // 3. Add a single new device statically
  static Future<void> addDevice(SmartDevice newDevice) async {
    final List<SmartDevice> currentDevices = await getDevices();
    currentDevices.add(newDevice);
    await saveDevices(currentDevices);
  }

  // 4. Update a device IP statically
  static Future<void> updateDeviceIp(
    String id,
    String newIp,
    String newPort,
  ) async {
    final List<SmartDevice> currentDevices = await getDevices();
    final int index = currentDevices.indexWhere((device) => device.id == id);

    if (index != -1) {
      currentDevices[index] = SmartDevice(
        id: currentDevices[index].id,
        name: currentDevices[index].name,
        ip: newIp,
        port: newPort,
        type: currentDevices[index].type,
        roomName: currentDevices[index].roomName,
      );
      await saveDevices(currentDevices);
    }
  }

  // 5. Update a device Name statically
  static Future<void> updateDeviceName(String id, String newName) async {
    final List<SmartDevice> currentDevices = await getDevices();
    final int index = currentDevices.indexWhere((device) => device.id == id);

    if (index != -1) {
      currentDevices[index] = SmartDevice(
        id: currentDevices[index].id,
        name: newName,
        ip: currentDevices[index].ip,
        port: currentDevices[index].port,
        type: currentDevices[index].type,
        roomName: currentDevices[index].roomName,
      );
      await saveDevices(currentDevices);
    }
  }

  static Future<void> deleteDevice(String id) async {
    // 1. Fetch the latest list of stored devices
    final List<SmartDevice> currentDevices = await getDevices();

    // 2. Remove the device that matches the passed ID
    currentDevices.removeWhere((device) => device.id == id);

    // 3. Save the updated list back to shared_preferences
    await saveDevices(currentDevices);
  }

  static Future<void> deleteAllDevices() async {
    // 1. Fetch the latest list of stored devices
    final List<SmartDevice> currentDevices = await getDevices();

    // 2. Remove the device that matches the passed ID
    currentDevices.clear();

    // 3. Save the updated list back to shared_preferences
    await saveDevices(currentDevices);
  }
}
