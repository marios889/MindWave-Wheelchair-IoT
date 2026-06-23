import 'package:flutter/material.dart';
import 'package:network_info_plus/network_info_plus.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:wheelchair_smarthome/data/constants.dart';
import 'package:wheelchair_smarthome/data/notifiers/devices_state.dart';
import 'package:wheelchair_smarthome/data/notifiers/notifiers.dart';
import 'package:wheelchair_smarthome/data/services/network_event_service.dart';
import 'package:wheelchair_smarthome/data/services/udp_service.dart';
import 'package:wheelchair_smarthome/views/splash_screen.dart';
import 'package:connectivity_plus/connectivity_plus.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({super.key});

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  @override
  void initState() {
    getThemeData();
    // 1. Initialize your outbound socket for the first time
    UdpService.initOutbound();

    // 2. Start the persistent network monitor
    monitorNetwork();
    super.initState();
  }

  Future<void> getThemeData() async {
    final SharedPreferences prefs = await SharedPreferences.getInstance();
    final bool? repeat = prefs.getBool(ThemeChange.themeChange);
    isDarkModeNotifier.value = repeat ?? false;
  }

  void monitorNetwork() {
    Connectivity().onConnectivityChanged.listen((
      List<ConnectivityResult> results,
    ) async {
      final hasConnection = results.any(
        (result) => result != ConnectivityResult.none,
      );

      if (!hasConnection) {
        print("🚨 Phone went completely OFFLINE!");
        UdpService.closeSocket();
        MultiUdpService.endAllPorts();
        DevicesState.resetAllToOffline();
      } else {
        if (results.contains(ConnectivityResult.wifi)) {
          print("🟢 Phone re-connected to Wi-Fi.");

          // 1. Safe boot outbound socket channels
          await UdpService.initOutbound();

          // 2. Fetch the phone's live IP address
          final info = NetworkInfo();
          String? activePhoneIp = await info.getWifiIP();

          if (activePhoneIp != null) {
            // 3. Automatically remap all your ESP hardware address hooks!
            await DevicesState.updateAllDevicesSubnet(activePhoneIp);
            await DevicesState.updateWheelchairSubnet(activePhoneIp);
            NetworkEventService.fireBroadcast(
              NetworkBroadcastType.fullRoomStats,
            );
            NetworkEventService.fireBroadcast(
              NetworkBroadcastType.sensorDataPull,
            );
            NetworkEventService.fireBroadcast(
              NetworkBroadcastType.doorContactCheck,
            );
          }
        }
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    return ValueListenableBuilder(
      valueListenable: isDarkModeNotifier,
      builder: (context, isDarkMode, child) {
        return MaterialApp(
          debugShowCheckedModeBanner: false,
          theme: ThemeData(
            colorScheme: .fromSeed(
              seedColor: Colors.teal,
              brightness: isDarkMode ? Brightness.dark : Brightness.light,
            ),
            navigationBarTheme: NavigationBarThemeData(
              backgroundColor: isDarkMode ? null : Colors.white,
              indicatorColor: isDarkMode
                  ? Colors.teal.withValues(alpha: 0.3)
                  : Colors.teal.withValues(alpha: 0.15),
              labelTextStyle: WidgetStateProperty.resolveWith((states) {
                if (states.contains(WidgetState.selected)) {
                  return TextStyle(
                    color: isDarkMode ? Colors.teal[200] : Colors.teal[700],
                    fontWeight: FontWeight.bold,
                    fontSize: 12,
                  );
                }
                return TextStyle(
                  color: isDarkMode ? Colors.white60 : Colors.black54,
                  fontSize: 12,
                );
              }),

              // Customizes icon colors globally based on active selection state
              iconTheme: WidgetStateProperty.resolveWith((states) {
                if (states.contains(WidgetState.selected)) {
                  return IconThemeData(
                    color: isDarkMode ? Colors.teal[200] : Colors.teal[700],
                  );
                }
                return IconThemeData(
                  color: isDarkMode ? Colors.white60 : Colors.black54,
                );
              }),
            ),
          ),
          home: const SplashScreen(),
        );
      },
    );
  }
}
