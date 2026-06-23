import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:wheelchair_smarthome/data/models/room_config.dart';
import 'package:wheelchair_smarthome/data/models/smart_device.dart';
import 'package:wheelchair_smarthome/data/notifiers/notifiers.dart';
import 'package:wheelchair_smarthome/data/services/storage_service.dart';

class DevicesState {
  // 1. The central notifier that holds the live list of devices
  static final ValueNotifier<List<SmartDevice>> devicesNotifier =
      ValueNotifier<List<SmartDevice>>([]);

  static Future<void> loadWheelchair() async {
    final prefs = await SharedPreferences.getInstance();
    WheelchairNotifier.value = prefs.getString("wheelchair") ?? "Unknown";
    WheelchairPortNotifier.value = prefs.getInt("wheelchair_port") ?? 8000;
  }

  static void toggleDeviceStatus(String id, bool newStatus) {
    final currentList = devicesNotifier.value;
    final deviceIndex = currentList.indexWhere((d) => d.id == id);

    if (deviceIndex != -1) {
      currentList[deviceIndex].isOn.value = newStatus;

      StorageService.saveDevices(currentList); // Commit instantly to storage
    }
  }

  static void toggleDevice30AStatus(String id, bool newStatus) {
    final currentList = devicesNotifier.value;
    final deviceIndex = currentList.indexWhere((d) => d.id == id);

    if (deviceIndex != -1) {
      currentList[deviceIndex].isOn30A.value = newStatus;

      StorageService.saveDevices(currentList); // Commit instantly to storage
    }
  }

  static void changeOpenPercentage(String id, double newValue) {
    final currentList = devicesNotifier.value;
    final deviceIndex = currentList.indexWhere((d) => d.id == id);

    if (deviceIndex != -1) {
      currentList[deviceIndex].sliderValue.value = newValue;

      StorageService.saveDevices(currentList); // Commit instantly to storage
    }
  }

  // 2. Load initially from SharedPreferences (Call this in main.dart or your home page)
  static Future<void> loadDevices() async {
    devicesNotifier.value = await StorageService.getDevices();
  }

  // 3. Add device, save to disk, and notify UI instantly
  static Future<void> addDevice(SmartDevice device) async {
    await StorageService.addDevice(device);
    // Assigning a new list forces the ValueNotifier to trigger a UI rebuild
    devicesNotifier.value = [...devicesNotifier.value, device];
  }

  // 4. Delete device, update disk, and notify UI instantly
  static Future<void> deleteDevice(String id) async {
    await StorageService.deleteDevice(id);
    devicesNotifier.value = devicesNotifier.value
        .where((d) => d.id != id)
        .toList();
  }

  static Future<void> clearAllDevices() async {
    await StorageService.deleteAllDevices();
    devicesNotifier.value = devicesNotifier.value.toList();
  }

  // 5. Update IP, save to disk, and notify UI instantly
  static Future<void> updateDeviceIp(
    String id,
    String newIp,
    String newPort,
  ) async {
    await StorageService.updateDeviceIp(id, newIp, newPort);
    devicesNotifier.value = [
      for (final device in devicesNotifier.value)
        if (device.id == id)
          SmartDevice(
            id: device.id,
            name: device.name,
            ip: newIp,
            port: newPort,
            type: device.type,
            roomName: device.roomName,
            isOn: device.isOn,
            isOnline: device.isOnline,
          )
        else
          device,
    ];
  }

  // 5. Update Name
  static Future<void> updateDeviceName(String id, String newName) async {
    await StorageService.updateDeviceName(id, newName);
    devicesNotifier.value = [
      for (final device in devicesNotifier.value)
        if (device.id == id)
          SmartDevice(
            id: device.id,
            name: newName,
            ip: device.ip,
            port: device.port,
            type: device.type,
            roomName: device.roomName,
            isOn: device.isOn,
            isOnline: device.isOnline,
          )
        else
          device,
    ];
  }

  /// Resets everything to offline when entering the page
  static void resetAllToOffline() {
    for (final device in devicesNotifier.value) {
      device.isOnline.value =
          false; // Updates seamlessly without shifting list references
      device.sliderValue.value = 10.0;
    }
  }

  static Future<void> updateWheelchairSubnet(String currentPhoneIp) async {
    List<String> ipParts = currentPhoneIp.split('.');
    if (ipParts.length != 4) {
      print("❌ Invalid phone IP formatting. Cannot update subnets.");
      return;
    }
    String newSubnet = "${ipParts[0]}.${ipParts[1]}.${ipParts[2]}.";
    print("🎯 Target local network subnet detected: $newSubnet");
    List<String> wheelchairParts = WheelchairNotifier.value.split(".");
    String lastOctet = wheelchairParts[3];
    String updatedWheelchairIp = "$newSubnet$lastOctet";
    WheelchairNotifier.value = updatedWheelchairIp;
    final prefs = await SharedPreferences.getInstance();
    prefs.setString("wheelchair", updatedWheelchairIp);
  }

  /// Updates all saved devices to match the phone's current network subnet
  static Future<void> updateAllDevicesSubnet(String currentPhoneIp) async {
    // 1. Extract the first three octets (e.g., "192.168.1.45" -> "192.168.1.")
    List<String> ipParts = currentPhoneIp.split('.');
    if (ipParts.length != 4) {
      print("❌ Invalid phone IP formatting. Cannot update subnets.");
      return;
    }
    String newSubnet = "${ipParts[0]}.${ipParts[1]}.${ipParts[2]}.";
    print("🎯 Target local network subnet detected: $newSubnet");

    // 2. Map through current devices and update their IPs
    final updatedList = devicesNotifier.value.map((device) {
      List<String> deviceParts = device.ip.split('.');

      // If the device IP is malformed, skip editing it
      if (deviceParts.length != 4) return device;

      // Keep the last octet (the host ID, e.g., "50") and append it to the new subnet
      String lastOctet = deviceParts[3];
      String updatedDeviceIp = "$newSubnet$lastOctet";

      return SmartDevice(
        id: device.id,
        name: device.name,
        ip: updatedDeviceIp, // 💡 Assigned the fresh network IP address!
        port: device.port,
        type: device.type,
        roomName: device.roomName,
        isOn: device.isOn,
        isOnline: device.isOnline,
      );
    }).toList();

    // 3. Commit the updated list to runtime memory and local storage disk
    devicesNotifier.value = updatedList;
    await StorageService.saveDevices(updatedList);
    print("🔄 Successfully migrated all device IPs to the new subnet layout.");
  }

  static void resetAllStates() {
    for (final device in devicesNotifier.value) {
      device.isOn.value =
          false; // Updates seamlessly without shifting list references
    }
  }

  static void updateConnectionByIp(String networkIp, bool connection) {
    final currentDevices = devicesNotifier.value;

    // Find the device matching the hardware sender IP
    final index = currentDevices.indexWhere((d) => d.ip == networkIp);

    if (index != -1) {
      // Flips the status dot instantly! Only widgets listening to THIS
      // specific device's isOnline notifier will re-render.
      currentDevices[index].isOnline.value = connection;
      print("🟢 Granular update: Device at $networkIp is now online.");
    }
  }

  static void updateStatusByIp(String networkIp, bool status) {
    final currentDevices = devicesNotifier.value;

    // Find the device matching the hardware sender IP
    final index = currentDevices.indexWhere((d) => d.ip == networkIp);

    if (index != -1) {
      // Flips the status dot instantly! Only widgets listening to THIS
      // specific device's isOnline notifier will re-render.
      currentDevices[index].isOn.value = status;
      print("🟢 Granular update: Device at $networkIp state is $status");
    }
  }

  static void update30AByIp(String networkIp, bool status) {
    final currentDevices = devicesNotifier.value;

    // Find the device matching the hardware sender IP
    final index = currentDevices.indexWhere(
      (d) => d.ip == networkIp && d.type == DeviceType.lamp_device,
    );

    if (index != -1) {
      // Flips the status dot instantly! Only widgets listening to THIS
      // specific device's isOnline notifier will re-render.
      currentDevices[index].isOn30A.value = status;
      print("🟢 Granular update: Device at $networkIp state is $status");
    }
  }

  static void updateOpenPercentage(String networkIp, double value) {
    final currentDevices = devicesNotifier.value;

    // Find the device matching the hardware sender IP
    final index = currentDevices.indexWhere((d) => d.ip == networkIp);

    if (index != -1) {
      // Flips the status dot instantly! Only widgets listening to THIS
      // specific device's isOnline notifier will re-render.
      currentDevices[index].sliderValue.value = value;
      print(
        "🟢 Granular update: Device at $networkIp is at $value percentage.",
      );
    }
  }

  static void updateConnectionByPort(String port, bool status) {
    final currentDevices = devicesNotifier.value;

    // Find the device matching the hardware sender IP
    final index = currentDevices.indexWhere(
      (d) =>
          d.port == port &&
          (d.type == DeviceType.sensorNode ||
              d.type == DeviceType.DoorContact ||
              d.type == DeviceType.PIRsensor),
    );

    if (index != -1) {
      // Flips the status dot instantly! Only widgets listening to THIS
      // specific device's isOnline notifier will re-render.
      currentDevices[index].isOnline.value = status;
      print("Sensor Device that sends data to port $port is now Online");
    }
  }

  static bool checkForDevices(String room) {
    final currentDevices = devicesNotifier.value;

    // Find the device matching the hardware sender IP
    final index = currentDevices.indexWhere(
      (d) =>
          d.roomName == room &&
          (d.type == DeviceType.lamp ||
              d.type == DeviceType.lamp_device ||
              d.type == DeviceType.Device ||
              d.type == DeviceType.curtains),
    );

    if (index != -1) {
      return true;
    }
    return false;
  }
}
