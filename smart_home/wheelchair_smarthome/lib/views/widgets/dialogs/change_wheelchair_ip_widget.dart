import 'package:flutter/material.dart';
import 'package:network_info_plus/network_info_plus.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:wheelchair_smarthome/data/notifiers/notifiers.dart';
import 'package:wheelchair_smarthome/utils/network_validator.dart';

class ChangeWheelchairIp extends StatefulWidget {
  const ChangeWheelchairIp({super.key});

  @override
  State<ChangeWheelchairIp> createState() => _ChangeWheelchairIpState();
}

class _ChangeWheelchairIpState extends State<ChangeWheelchairIp> {
  TextEditingController IPcontroller = TextEditingController();
  TextEditingController PortController = TextEditingController();
  String? errorMessage;
  @override
  void initState() {
    super.initState();
    IPcontroller.text = WheelchairNotifier.value;
    PortController.text = WheelchairPortNotifier.value.toString();
    _alignToCurrentNetworkSubnet();
  }

  @override
  void dispose() {
    IPcontroller.dispose();
    PortController.dispose();
    super.dispose();
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
    return AlertDialog(
      title: const Text("Change Wheelchair IP"),
      content: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          TextField(
            controller: IPcontroller,
            decoration: const InputDecoration(
              border: OutlineInputBorder(),
              labelText: "IP address",
            ),
          ),
          const SizedBox(height: 20.0),
          TextField(
            controller: PortController,
            decoration: const InputDecoration(
              border: OutlineInputBorder(),
              labelText: "Port",
            ),
          ),
          if (errorMessage != null) ...[
            const SizedBox(height: 12),
            Text(
              errorMessage!,
              style: const TextStyle(color: Colors.red, fontSize: 13),
            ),
          ],
        ],
      ),
      actions: [
        ElevatedButton(
          onPressed: () async {
            final ip = IPcontroller.text.trim();
            final port = PortController.text.trim();
            if (!NetworkValidator.isValidIp(ip)) {
              setState(() => errorMessage = "Invalid IP Address format");
              return;
            }
            if (!NetworkValidator.isValidPort(port)) {
              setState(() => errorMessage = "Port must be between 1 and 65535");
              return;
            }
            final prefs = await SharedPreferences.getInstance();
            prefs.setString("wheelchair", IPcontroller.text);
            prefs.setInt("wheelchair_port", int.parse(PortController.text));
            WheelchairNotifier.value = IPcontroller.text;
            WheelchairPortNotifier.value = int.parse(PortController.text);
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
    );
  }
}
