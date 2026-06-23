import 'package:flutter/material.dart';
import 'package:wheelchair_smarthome/data/models/smart_device.dart';

class EnterOtaModeDialog extends StatefulWidget {
  final SmartDevice device;
  const EnterOtaModeDialog({super.key, required this.device});

  @override
  State<EnterOtaModeDialog> createState() => _EnterOtaModeDialogState();
}

class _EnterOtaModeDialogState extends State<EnterOtaModeDialog> {
  @override
  Widget build(BuildContext context) {
    return Center(
      child: SingleChildScrollView(
        child: AlertDialog(title: Row(children: [])),
      ),
    );
  }
}
