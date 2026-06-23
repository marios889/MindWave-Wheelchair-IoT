import 'package:flutter/material.dart';
import 'package:network_info_plus/network_info_plus.dart';
import 'package:wheelchair_smarthome/data/models/smart_device.dart';
import 'package:wheelchair_smarthome/data/notifiers/devices_state.dart';
import 'package:wheelchair_smarthome/data/services/udp_service.dart';
import 'package:wheelchair_smarthome/utils/network_validator.dart';

class AdvancedSetupEsp extends StatefulWidget {
  const AdvancedSetupEsp({super.key});

  @override
  State<AdvancedSetupEsp> createState() => _AdvancedSetupEspState();
}

class _AdvancedSetupEspState extends State<AdvancedSetupEsp> {
  TextEditingController WIFISSIDController = TextEditingController();
  TextEditingController WIFIPasswordController = TextEditingController();
  TextEditingController DeviceIPcontroller = TextEditingController();
  TextEditingController DevicePortcontroller = TextEditingController();
  DeviceType? menuItem = DeviceType.lamp;
  String? roomName = "Living Room";
  String? errorMessage;
  @override
  void dispose() {
    WIFISSIDController.dispose();
    WIFIPasswordController.dispose();
    DeviceIPcontroller.dispose();
    DevicePortcontroller.dispose();
    super.dispose();
  }

  @override
  void initState() {
    super.initState();
    DeviceIPcontroller.text = "192.168.1.50";
    DevicePortcontroller.text = "8000";
    _prefillCurrentSubnet();
  }

  Future<void> _prefillCurrentSubnet() async {
    try {
      final info = NetworkInfo();
      String? phoneIp = await info.getWifiIP();

      if (phoneIp != null && phoneIp.contains('.')) {
        List<String> parts = phoneIp.split('.');
        if (parts.length == 4) {
          // Extract "X.X.X." and attach a default host suffix like "50"
          String dynamicSubnetIp = "${parts[0]}.${parts[1]}.${parts[2]}.50";

          // 3. Update the text field layout text safely
          setState(() {
            DeviceIPcontroller.text = dynamicSubnetIp;
          });
        }
      }
    } catch (e) {
      print("Could not prefill subnet dynamically: $e");
    }
  }

  @override
  Widget build(BuildContext context) {
    return Center(
      child: SingleChildScrollView(
        child: AlertDialog(
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(16),
          ),
          title: const Text("Enter Advanced ESP Device Info"),
          content: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              if (errorMessage != null) ...[
                const SizedBox(height: 3),
                Text(
                  errorMessage!,
                  style: const TextStyle(color: Colors.red, fontSize: 13),
                ),
              ],

              TextField(
                controller: WIFISSIDController,
                decoration: const InputDecoration(
                  labelText: "WIFI SSID",
                  border: OutlineInputBorder(
                    borderRadius: BorderRadius.all(Radius.circular(10.0)),
                  ),
                ),
              ),
              const SizedBox(height: 20.0),
              TextField(
                controller: WIFIPasswordController,
                decoration: const InputDecoration(
                  labelText: "WIFI Password",
                  border: OutlineInputBorder(
                    borderRadius: BorderRadius.all(Radius.circular(10.0)),
                  ),
                ),
              ),

              if (menuItem != DeviceType.sensorNode &&
                  menuItem != DeviceType.DoorContact) ...[
                const SizedBox(height: 20.0),
                TextField(
                  controller: DeviceIPcontroller,
                  decoration: const InputDecoration(
                    labelText: "IP address",
                    border: OutlineInputBorder(
                      borderRadius: BorderRadius.all(Radius.circular(10.0)),
                    ),
                  ),
                ),
              ],

              const SizedBox(height: 20.0),
              TextField(
                controller: DevicePortcontroller,
                decoration: const InputDecoration(
                  labelText: "Port",
                  border: OutlineInputBorder(
                    borderRadius: BorderRadius.all(Radius.circular(10.0)),
                  ),
                ),
              ),
              const SizedBox(height: 10.0),
              const Text("Type"),
              const SizedBox(height: 6.0),
              DropdownButton(
                value: menuItem,
                items: [
                  const DropdownMenuItem(
                    value: DeviceType.lamp,
                    child: Text("Lamp"),
                  ),
                  const DropdownMenuItem(
                    value: DeviceType.sensorNode,
                    child: Text("Sensor"),
                  ),
                  const DropdownMenuItem(
                    value: DeviceType.curtains,
                    child: Text("Curtains"),
                  ),
                  const DropdownMenuItem(
                    value: DeviceType.Device,
                    child: Text("Device"),
                  ),
                  const DropdownMenuItem(
                    value: DeviceType.DoorContact,
                    child: Text("DoorContact"),
                  ),
                ],
                onChanged: (DeviceType? value) {
                  setState(() {
                    menuItem = value;
                  });
                },
              ),

              const Text("Room"),
              const SizedBox(height: 6.0),
              DropdownButton(
                value: roomName,
                items: [
                  const DropdownMenuItem(
                    value: "Living Room",
                    child: Text("Living Room"),
                  ),
                  const DropdownMenuItem(
                    value: "Bedroom",
                    child: Text("Bedroom"),
                  ),
                  const DropdownMenuItem(
                    value: "Kitchen",
                    child: Text("Kitchen"),
                  ),
                  const DropdownMenuItem(
                    value: "Bathroom",
                    child: Text("Bathroom"),
                  ),
                ],
                onChanged: (String? value) {
                  setState(() {
                    roomName = value;
                  });
                },
              ),
              const SizedBox(height: 10.0),
            ],
          ),
          actions: [
            ElevatedButton(
              onPressed: () {
                final ip = DeviceIPcontroller.text.trim();
                final port = DevicePortcontroller.text.trim();
                if (!NetworkValidator.isValidIp(ip)) {
                  setState(() => errorMessage = "Invalid IP Address format");
                  return;
                }
                if (!NetworkValidator.isValidPort(port)) {
                  setState(
                    () => errorMessage = "Port must be between 1 and 65535",
                  );
                  return;
                }

                UdpService.sendJsonMessage("192.168.4.1", 5003, {
                  "ssid": WIFISSIDController.text,
                  "pass": WIFIPasswordController.text,
                  "ip": DeviceIPcontroller.text,
                  "port": int.parse(DevicePortcontroller.text),
                });

                DevicesState.addDevice(
                  SmartDevice(
                    id: DateTime.now().millisecondsSinceEpoch.toString(),
                    name: menuItem.toString().replaceFirst("DeviceType.", ""),
                    ip: DeviceIPcontroller.text,
                    port: DevicePortcontroller.text,
                    type: menuItem!,
                    roomName: roomName!,
                  ),
                );

                Navigator.pop(context);
              },
              child: const Text("Add New ESP"),
            ),
            ElevatedButton(
              onPressed: () {
                Navigator.pop(context);
              },
              child: const Text("Cancel"),
            ),
          ],
        ),
      ),
    );
  }
}
