import 'package:flutter/material.dart';
import 'package:network_info_plus/network_info_plus.dart';
import 'package:wheelchair_smarthome/views/widgets/dialogs/setup_mode_dialog.dart';

class InstrucNewEsp extends StatefulWidget {
  const InstrucNewEsp({super.key});

  @override
  State<InstrucNewEsp> createState() => _InstrucNewEspState();
}

class _InstrucNewEspState extends State<InstrucNewEsp> {
  String? errorMessage;

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
      title: const Row(
        children: [
          Icon(Icons.wifi_find, color: Colors.blueGrey),
          SizedBox(width: 8),
          Text("Connect to ESP Node"),
        ],
      ),
      content: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Text(
            "Please follow these instructions to configure your new ESP:",
            style: TextStyle(fontWeight: FontWeight.w500),
          ),
          const SizedBox(height: 16),
          if (errorMessage != null) ...[
            const SizedBox(height: 16),
            Container(
              padding: const EdgeInsets.all(10),
              decoration: BoxDecoration(
                color: Colors.red.shade50,
                borderRadius: BorderRadius.circular(8),
                border: Border.all(color: Colors.red.shade200),
              ),
              child: Row(
                children: [
                  Icon(
                    Icons.error_outline,
                    color: Colors.red.shade700,
                    size: 20,
                  ),
                  const SizedBox(width: 8),
                  Expanded(
                    child: Text(
                      errorMessage!,
                      style: TextStyle(
                        color: Colors.red.shade900,
                        fontSize: 14,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                  ),
                ],
              ),
            ),
          ],
          const SizedBox(height: 16),
          ListTile(
            leading: CircleAvatar(
              backgroundColor: Colors.blueGrey.withValues(alpha: 0.1),
              child: const Text(
                "1",
                style: TextStyle(fontWeight: FontWeight.bold),
              ),
            ),
            title: const Text("Open your phone's Wi-Fi Settings panel."),
            dense: true,
          ),
          ListTile(
            leading: CircleAvatar(
              backgroundColor: Colors.blueGrey.withValues(alpha: 0.1),
              child: const Text(
                "2",
                style: TextStyle(fontWeight: FontWeight.bold),
              ),
            ),
            title: RichText(
              text: const TextSpan(
                style: TextStyle(fontSize: 15),
                children: [
                  TextSpan(text: "Connect to the network that starts with "),
                  TextSpan(
                    text: "ESP_",
                    style: TextStyle(fontWeight: FontWeight.bold),
                  ),
                  TextSpan(text: " (e.g., ESP_SmartHome_Node)."),
                ],
              ),
            ),
            dense: true,
          ),
          ListTile(
            leading: CircleAvatar(
              backgroundColor: Colors.blueGrey.withValues(alpha: 0.1),
              child: const Text(
                "3",
                style: TextStyle(fontWeight: FontWeight.bold),
              ),
            ),
            title: const Text(
              "Once connected to the ESP Access Point, return to this app and tap Next.",
            ),
            dense: true,
          ),
        ],
      ),
      actions: [
        TextButton(
          onPressed: () {
            Navigator.of(context).pop(); // Closes the dialog
          },
          child: const Text("CANCEL", style: TextStyle(color: Colors.grey)),
        ),

        ElevatedButton(
          onPressed: () async {
            // Trigger the network utility check
            bool isConnected = await checkEspWifi();

            if (isConnected) {
              Navigator.pop(context);

              showDialog(
                context: context,
                builder: (context) {
                  return SetupModeDialog();
                },
              );
            } else {
              // Update the state inside the dialog to reveal the warning banner
              setState(() {
                errorMessage =
                    "Connection missing. Please select your 'ESP_' network in your phone settings.";
              });
            }
          },
          child: const Text("I UNDERSTAND"),
        ),
      ],
    );
  }

  Future<bool> checkEspWifi() async {
    final info = NetworkInfo();

    try {
      // 1. Fetch the network info strings from the package
      String? wifiIP = await info.getWifiIP();

      print("Checking network -> IP: $wifiIP");

      // 2.Validate by IP address range
      if (wifiIP != null && wifiIP.startsWith('192.168.4.')) {
        return true;
      }

      return false;
    } catch (e) {
      print("Error reading network info: $e");
      return false;
    }
  }
}
