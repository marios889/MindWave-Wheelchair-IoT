import 'package:flutter/material.dart';
import 'package:wheelchair_smarthome/data/models/smart_device.dart';
import 'package:wheelchair_smarthome/data/notifiers/devices_state.dart';
import 'package:wheelchair_smarthome/data/services/udp_service.dart';
import 'package:wheelchair_smarthome/views/widgets/dialogs/change_device_ip_widget.dart';
import 'package:wheelchair_smarthome/views/widgets/dialogs/change_device_name_widget.dart';
import 'package:wheelchair_smarthome/views/widgets/dialogs/enter_ota_mode_dialog.dart';

class PopupmenuDialog {
  static void showPopupMenu(dynamic context, SmartDevice device) async {
    // 1. Find the physical location of the card on the screen to anchor the popup
    final RenderBox renderBox = context.findRenderObject() as RenderBox;
    final Offset offset = renderBox.localToGlobal(Offset.zero);

    // 2. Display the floating Material popover menu
    final String? selectedAction = await showMenu<String>(
      context: context,
      // Place the menu just slightly below where the card top edge lives
      position: RelativeRect.fromLTRB(
        offset.dx + 40,
        offset.dy + 60,
        offset.dx + renderBox.size.width,
        offset.dy + renderBox.size.height,
      ),
      color: Colors.black,
      items: [
        PopupMenuItem<String>(
          value: 'firmware',
          child: Row(
            children: [
              Icon(Icons.update, color: Colors.blueAccent, size: 20),
              const SizedBox(width: 12),
              const Text('Firmware update'),
            ],
          ),
        ),
        if (device.type == DeviceType.curtains)
          PopupMenuItem<String>(
            value: 'reset',
            child: Row(
              children: [
                Icon(Icons.update, color: Colors.green, size: 20),
                const SizedBox(width: 12),
                const Text('Reset'),
              ],
            ),
          ),
        PopupMenuItem<String>(
          value: 'edit name',
          child: Row(
            children: [
              Icon(Icons.edit_attributes, color: Colors.yellow[700], size: 20),
              const SizedBox(width: 12),
              const Text('Edit Name'),
            ],
          ),
        ),
        const PopupMenuItem<String>(
          value: 'edit',
          child: Row(
            children: [
              Icon(Icons.edit_outlined, size: 20, color: Colors.blue),
              SizedBox(width: 12),
              Text('Edit IP'),
            ],
          ),
        ),
        PopupMenuItem<String>(
          value: 'delete',
          child: Row(
            children: [
              Icon(
                Icons.delete_forever_outlined,
                color: Colors.redAccent[700],
                size: 20,
              ),
              const SizedBox(width: 12),
              const Text('Remove Device'),
            ],
          ),
        ),
      ],
      elevation: 8.0,
    );

    // 3. Handle the chosen selection safely
    if (selectedAction == 'edit') {
      showDialog(
        context: context,
        builder: (context) {
          return ChangeDeviceIP(device: device);
        },
      );
    } else if (selectedAction == 'delete') {
      DevicesState.deleteDevice(device.id);
    } else if (selectedAction == 'edit name') {
      showDialog(
        context: context,
        builder: (context) {
          return ChangeDeviceName(device: device);
        },
      );
    } else if (selectedAction == 'firmware') {
      UdpService.sendUDP("OTA", device.ip, int.parse(device.port));
    } else if (selectedAction == 'reset') {
      UdpService.sendUDP("R", device.ip, int.parse(device.port));
    }
  }
}
