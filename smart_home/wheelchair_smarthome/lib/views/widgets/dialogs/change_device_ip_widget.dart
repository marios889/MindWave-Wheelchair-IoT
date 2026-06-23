import 'package:flutter/material.dart';
import 'package:network_info_plus/network_info_plus.dart';
import 'package:wheelchair_smarthome/data/models/smart_device.dart';
import 'package:wheelchair_smarthome/data/notifiers/devices_state.dart';
import 'package:wheelchair_smarthome/utils/network_validator.dart';

class ChangeDeviceIP extends StatefulWidget {
  const ChangeDeviceIP({super.key, required this.device});
  final SmartDevice device;
  @override
  State<ChangeDeviceIP> createState() => _ChangeDeviceIPState();
}

class _ChangeDeviceIPState extends State<ChangeDeviceIP> {
  TextEditingController IPcontroller = TextEditingController();
  TextEditingController Portcontroller = TextEditingController();
  String? errorMessage;
  @override
  void dispose() {
    IPcontroller.dispose();
    Portcontroller.dispose();
    super.dispose();
  }

  @override
  void initState() {
    super.initState();
    IPcontroller.text = widget.device.ip;
    Portcontroller.text = widget.device.port;
    _alignToCurrentNetworkSubnet();
  }

  Future<void> _alignToCurrentNetworkSubnet() async {
    final info = NetworkInfo();
    String? phoneIp = await info.getWifiIP();

    if (phoneIp != null) {
      List<String> phoneParts = phoneIp.split('.');
      List<String> deviceParts = IPcontroller.text.split('.');

      if (phoneParts.length == 4 && deviceParts.length == 4) {
        // Construct: Phone Subnet (3 octets) + Existing Device Host ID (last octet)
        String matchedIp =
            "${phoneParts[0]}.${phoneParts[1]}.${phoneParts[2]}.${deviceParts[3]}";

        setState(() {
          IPcontroller.text = matchedIp;
        });
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Center(
      child: SingleChildScrollView(
        child: AlertDialog(
          title: const Text("Change Device IP"),
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

              if (widget.device.type != DeviceType.sensorNode &&
                  widget.device.type != DeviceType.DoorContact &&
                  widget.device.type != DeviceType.PIRsensor) ...[
                const SizedBox(height: 20.0),
                TextField(
                  controller: IPcontroller,
                  decoration: const InputDecoration(
                    border: OutlineInputBorder(),
                    labelText: "IP address",
                  ),
                ),
              ],

              const SizedBox(height: 20.0),
              TextField(
                controller: Portcontroller,
                decoration: const InputDecoration(
                  border: OutlineInputBorder(),
                  labelText: "Port",
                ),
              ),
            ],
          ),
          actions: [
            ElevatedButton(
              onPressed: () async {
                final ip = IPcontroller.text.trim();
                final port = Portcontroller.text.trim();
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
                DevicesState.updateDeviceIp(widget.device.id, ip, port);
                Navigator.pop(context);
              },
              child: const Text("Change"),
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
