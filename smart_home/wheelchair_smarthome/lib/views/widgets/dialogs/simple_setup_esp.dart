import 'package:flutter/material.dart';
import 'package:wheelchair_smarthome/data/services/udp_service.dart';

class SimpleSetupEsp extends StatefulWidget {
  const SimpleSetupEsp({super.key});

  @override
  State<SimpleSetupEsp> createState() => _SimpleSetupEspState();
}

class _SimpleSetupEspState extends State<SimpleSetupEsp> {
  TextEditingController WIFISSIDController = TextEditingController();
  TextEditingController WIFIPasswordController = TextEditingController();
  @override
  void dispose() {
    WIFISSIDController.dispose();
    WIFIPasswordController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: Text("Enter Simple ESP WIFI Setup"),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
      content: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
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
        ],
      ),
      actions: [
        ElevatedButton(
          onPressed: () {
            UdpService.sendJsonMessage("192.168.4.1", 5003, {
              "ssid": WIFISSIDController.text,
              "pass": WIFIPasswordController.text,
            });
          },
          child: Text("Setup"),
        ),
        ElevatedButton(
          onPressed: () {
            Navigator.pop(context);
          },
          child: Text("Cancel"),
        ),
      ],
    );
  }
}
