import 'package:flutter/material.dart';
import 'package:wheelchair_smarthome/data/notifiers/devices_state.dart';
import 'package:wheelchair_smarthome/data/services/notification_service.dart';
import 'package:wheelchair_smarthome/views/widget_tree.dart';

class SplashScreen extends StatefulWidget {
  const SplashScreen({super.key});

  @override
  State<SplashScreen> createState() => _SplashScreenState();
}

class _SplashScreenState extends State<SplashScreen> {
  @override
  void initState() {
    super.initState();
    _initializeApp();
  }

  // This handles your asynchronous startup tasks in the background
  Future<void> _initializeApp() async {
    try {
      // 1. Load your smart home appliances from local storage
      await DevicesState.loadDevices();

      // 2. Load your wheelchair configuration metrics
      await DevicesState.loadWheelchair();
      await NotificationService.initNotifications();
    } catch (e) {
      print("Error loading local system data: $e");
      // Handle error or fallback initialization here if needed
    }

    await Future.delayed(const Duration(seconds: 5));

    if (mounted) {
      Navigator.pushReplacement(
        context,
        MaterialPageRoute(builder: (context) => const WidgetTree()),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return const Scaffold(
      backgroundColor: Color(0xFF131622),
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(Icons.bolt_rounded, size: 80, color: Colors.deepOrange),
            SizedBox(height: 24),

            Text(
              "SMART HOME & MOBILITY",
              style: TextStyle(
                color: Colors.white,
                fontSize: 20,
                fontWeight: FontWeight.bold,
                letterSpacing: 1.5,
              ),
            ),
          ],
        ),
      ),
    );
  }
}
