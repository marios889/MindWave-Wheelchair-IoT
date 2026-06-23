import 'package:flutter/material.dart';
import 'package:network_info_plus/network_info_plus.dart';
import 'package:wheelchair_smarthome/data/models/smart_device.dart';
import 'package:wheelchair_smarthome/data/notifiers/devices_state.dart';
import 'package:wheelchair_smarthome/utils/network_validator.dart';

class NewDeviceDialogWidget extends StatefulWidget {
  const NewDeviceDialogWidget({super.key, required this.room});
  final String? room;

  @override
  State<NewDeviceDialogWidget> createState() => _NewDeviceDialogWidgetState();
}

class _NewDeviceDialogWidgetState extends State<NewDeviceDialogWidget> {
  TextEditingController DeviceNamecontroller = TextEditingController();
  TextEditingController DeviceIPcontroller = TextEditingController();
  TextEditingController DevicePortcontroller = TextEditingController();
  DeviceType? menuItem = DeviceType.lamp;
  String? errorMessage;

  @override
  void dispose() {
    DeviceNamecontroller.dispose();
    DeviceIPcontroller.dispose();
    DevicePortcontroller.dispose();
    super.dispose();
  }

  @override
  void initState() {
    super.initState();
    DeviceIPcontroller.text = "192.168.1.50";
    DevicePortcontroller.text = "8000";
    DeviceNamecontroller.text = "lamp";
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
          title: const Text("Enter Device Info"),
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
              const SizedBox(height: 10.0),
              TextField(
                controller: DeviceNamecontroller,
                decoration: const InputDecoration(
                  labelText: "Device Name",
                  border: OutlineInputBorder(
                    borderRadius: BorderRadius.all(Radius.circular(10.0)),
                  ),
                ),
              ),
              const SizedBox(height: 20.0),
              if (menuItem != DeviceType.sensorNode &&
                  menuItem != DeviceType.DoorContact &&
                  menuItem != DeviceType.PIRsensor) ...[
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
                  const DropdownMenuItem(
                    value: DeviceType.lamp_device,
                    child: Text("Lamp/Device"),
                  ),
                  const DropdownMenuItem(
                    value: DeviceType.PIRsensor,
                    child: Text("PIRSensor"),
                  ),
                ],
                onChanged: (DeviceType? value) {
                  setState(() {
                    menuItem = value;
                    DeviceNamecontroller.text = value.toString().replaceFirst(
                      "DeviceType.",
                      "",
                    );
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

                DevicesState.addDevice(
                  SmartDevice(
                    id: DateTime.now().millisecondsSinceEpoch.toString(),
                    name: DeviceNamecontroller.text,
                    ip: ip,
                    port: port,
                    type: menuItem!,
                    roomName: widget.room!,
                  ),
                );

                Navigator.pop(context);
              },
              child: const Text("Add Device"),
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
