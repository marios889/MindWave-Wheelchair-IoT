import 'package:flutter/material.dart';
import 'package:wheelchair_smarthome/data/models/smart_device.dart';
import 'package:wheelchair_smarthome/data/notifiers/devices_state.dart';

class ChangeDeviceName extends StatefulWidget {
  const ChangeDeviceName({super.key, required this.device});
  final SmartDevice device;
  @override
  State<ChangeDeviceName> createState() => _ChangeDeviceNameState();
}

class _ChangeDeviceNameState extends State<ChangeDeviceName> {
  TextEditingController NameController = TextEditingController();
  String? errorMessage;
  @override
  void dispose() {
    NameController.dispose();
    super.dispose();
  }

  @override
  void initState() {
    NameController.text = widget.device.name;
    super.initState();
  }

  @override
  Widget build(BuildContext context) {
    return Center(
      child: SingleChildScrollView(
        child: AlertDialog(
          title: const Text("Change Device Name"),
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
                controller: NameController,
                decoration: const InputDecoration(
                  border: OutlineInputBorder(),
                  labelText: "Name",
                ),
              ),
            ],
          ),
          actions: [
            ElevatedButton(
              onPressed: () async {
                DevicesState.updateDeviceName(
                  widget.device.id,
                  NameController.text,
                );
                Navigator.pop(context);
              },
              child: const Text("Change Name"),
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
