import 'package:flutter/material.dart';
import 'package:network_info_plus/network_info_plus.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:wheelchair_smarthome/data/constants.dart';
import 'package:wheelchair_smarthome/data/notifiers/devices_state.dart';
import 'package:wheelchair_smarthome/data/notifiers/notifiers.dart';
import 'package:wheelchair_smarthome/data/services/udp_service.dart';
import 'package:wheelchair_smarthome/views/widgets/cards/settings_Device_card_widget.dart';
import 'package:wheelchair_smarthome/views/widgets/dialogs/setup_mode_dialog.dart';

class SettingsPage extends StatefulWidget {
  const SettingsPage({super.key});

  @override
  State<SettingsPage> createState() => _SettingsPageState();
}

class _SettingsPageState extends State<SettingsPage> {
  @override
  void initState() {
    super.initState();
    _initializeNetworkDiscovery();
  }

  Future<String?> _getPrivateIp() async {
    final info = NetworkInfo();
    return await info.getWifiIP(); // Fetches the local IP (e.g., 192.168.1.X)
  }

  Future<String?> _getGatewayIP() async {
    final info = NetworkInfo();
    return await info
        .getWifiGatewayIP(); // Fetches the Gateway IP (e.g., 192.168.1.X)
  }

  Future<void> _initializeNetworkDiscovery() async {
    try {
      DevicesState.resetAllToOffline();

      String? phoneIp = await _getPrivateIp();
      if (phoneIp != null) {
        await MultiUdpService.startListening(5003);
        for (int i = 0; i < 3; i++) {
          UdpService.sendBroadcastMessage(7777, {
            "port": 5003,
            "command": "connect_stats",
          });
          await Future.delayed(const Duration(milliseconds: 300));
        }
      }
    } catch (e) {
      print("❌ Error starting page-level UDP tracking: $e");
    }
  }

  @override
  Widget build(BuildContext context) {
    return SingleChildScrollView(
      child: Padding(
        padding: const EdgeInsets.all(20.0),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const Text("Connection info"),
            const Divider(color: Colors.teal, thickness: 2.0),
            const SizedBox(height: 10.0),
            Container(
              width: double.infinity,

              decoration: BoxDecoration(
                border: Border.all(width: 2.0, color: Colors.teal),
                borderRadius: BorderRadius.all(Radius.circular(10.0)),
              ),
              child: Column(
                children: [
                  ListTile(
                    title: const Text(
                      "Phone IP",
                      style: TextStyle(fontSize: 16),
                    ),
                    trailing: FutureBuilder(
                      future: _getPrivateIp(),
                      builder: (context, snapshot) {
                        if (snapshot.connectionState ==
                            ConnectionState.waiting) {
                          return CircularProgressIndicator();
                        }
                        final ipAddress =
                            snapshot.data ?? "Disconnected / Unknown";
                        return Text(
                          ipAddress,
                          style: const TextStyle(
                            fontSize: 14,
                            fontWeight: FontWeight.bold,
                            fontFamily: 'monospace', // Clear numbers layout
                          ),
                        );
                      },
                    ),
                  ),
                  ListTile(
                    title: const Text(
                      "Gateway IP",
                      style: TextStyle(fontSize: 16),
                    ),
                    trailing: FutureBuilder(
                      future: _getGatewayIP(),
                      builder: (context, snapshot) {
                        if (snapshot.connectionState ==
                            ConnectionState.waiting) {
                          return CircularProgressIndicator();
                        }
                        final gatewayaddress =
                            snapshot.data ?? "Disconnected / Unknown";
                        return Text(
                          gatewayaddress,
                          style: const TextStyle(
                            fontSize: 14,
                            fontWeight: FontWeight.bold,
                            fontFamily: 'monospace', // Clear numbers layout
                          ),
                        );
                      },
                    ),
                  ),
                  ListTile(
                    title: const Text("Default Outbound Broadcast Port"),
                    trailing: const Text(
                      "7777",
                      style: TextStyle(
                        fontSize: 14,
                        fontWeight: FontWeight.bold,
                        fontFamily: 'monospace', // Clear numbers layout
                      ),
                    ),
                  ),
                  ListTile(
                    title: const Text("Default Inbound Response Port"),
                    trailing: const Text(
                      "5003",
                      style: TextStyle(
                        fontSize: 14,
                        fontWeight: FontWeight.bold,
                        fontFamily: 'monospace', // Clear numbers layout
                      ),
                    ),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 10.0),
            ListTile(
              title: Text("Dark Mode"),
              trailing: ValueListenableBuilder(
                valueListenable: isDarkModeNotifier,
                builder: (context, isDarkMode, child) {
                  return Switch(
                    value: isDarkMode,
                    onChanged: (value) async {
                      isDarkModeNotifier.value = !isDarkModeNotifier.value;
                      final SharedPreferences prefs =
                          await SharedPreferences.getInstance();
                      await prefs.setBool(
                        ThemeChange.themeChange,
                        isDarkModeNotifier.value,
                      );
                    },
                  );
                },
              ),
            ),
            const SizedBox(height: 20.0),
            TextButton(
              onPressed: () {
                showDialog(
                  context: context,
                  builder: (context) {
                    return SetupModeDialog();
                  },
                );
              },
              style: TextButton.styleFrom(
                minimumSize: const Size(double.infinity, 50.0),
                backgroundColor: Colors.teal.withValues(alpha: 0.8),
                foregroundColor: Colors.white,
              ),
              child: Text(
                "SETUP NEW ESP",
                style: const TextStyle(
                  fontSize: 16,
                  letterSpacing: 1.7,
                  fontWeight: FontWeight.bold,
                ),
              ),
            ),
            const SizedBox(height: 20.0),
            Divider(thickness: 1, indent: 16, endIndent: 16),

            SettingsDeviceCard(),
          ],
        ),
      ),
    );
  }
}
