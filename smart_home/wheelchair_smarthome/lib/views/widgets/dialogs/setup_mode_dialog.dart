import 'package:flutter/material.dart';
import 'package:wheelchair_smarthome/views/widgets/dialogs/advanced_setup_esp.dart';
import 'package:wheelchair_smarthome/views/widgets/dialogs/simple_setup_esp.dart';

class SetupModeDialog extends StatelessWidget {
  const SetupModeDialog({super.key});

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: const Text("Select Setup Mode"),
      content: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          ListTile(
            leading: const Icon(Icons.wifi, color: Colors.blue),
            title: const Text("Standard Setup"),
            subtitle: const Text("Connect ESP using SSID and Password only."),
            shape: RoundedRectangleBorder(
              borderRadius: BorderRadius.circular(12),
            ),
            onTap: () {
              Navigator.pop(context);
              showDialog(
                context: context,
                builder: (context) {
                  return SimpleSetupEsp();
                },
              );
            },
          ),
          const SizedBox(height: 8),
          ListTile(
            leading: const Icon(
              Icons.settings_ethernet,
              color: Colors.deepPurple,
            ),
            title: const Text("Advanced Setup"),
            subtitle: const Text(
              "Assign static IP, custom listening port, and save to app.",
            ),
            shape: RoundedRectangleBorder(
              borderRadius: BorderRadius.circular(12),
            ),
            onTap: () {
              Navigator.pop(context);
              showDialog(
                context: context,
                builder: (context) {
                  return AdvancedSetupEsp();
                },
              );
            },
          ),
        ],
      ),
    );
  }
}
