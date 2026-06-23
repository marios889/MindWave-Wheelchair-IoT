import 'package:flutter/material.dart';
import 'package:wakelock_plus/wakelock_plus.dart';
import 'package:wheelchair_smarthome/data/notifiers/notifiers.dart';
import 'package:wheelchair_smarthome/data/services/udp_service.dart';
import 'package:wheelchair_smarthome/views/widgets/Dpad_widget.dart';
import 'package:wheelchair_smarthome/views/widgets/Joystick_widget.dart';
import 'package:wheelchair_smarthome/views/widgets/dialogs/change_wheelchair_ip_widget.dart';

class WheelchairPage extends StatefulWidget {
  const WheelchairPage({super.key});

  @override
  State<WheelchairPage> createState() => _WheelchairPageState();
}

class _WheelchairPageState extends State<WheelchairPage> {
  @override
  void initState() {
    super.initState();
    WakelockPlus.enable();
    WheelchairisConnected.value = false;
  }

  @override
  void dispose() {
    WakelockPlus.disable();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return SingleChildScrollView(
      child: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.start,
          children: [
            ListTile(
              title: Row(
                children: [
                  ValueListenableBuilder(
                    valueListenable: WheelchairisConnected,
                    builder: (context, isOnline, child) {
                      return Container(
                        width: 10.0,
                        height: 10.0,
                        decoration: BoxDecoration(
                          color: isOnline
                              ? Colors.greenAccent[700]
                              : Colors.redAccent[700],
                          shape: BoxShape.circle,
                          boxShadow: [
                            BoxShadow(
                              color: isOnline
                                  ? Colors.greenAccent.withValues(alpha: 0.4)
                                  : Colors.redAccent.withValues(alpha: 0.4),
                              blurRadius: 6.0,
                              spreadRadius: 2.0,
                            ),
                          ],
                        ),
                      );
                    },
                  ),
                  const SizedBox(width: 10.0),
                  const Text("Wheelchair IP"),
                ],
              ),
              trailing: ElevatedButton(
                onPressed: () {
                  showDialog(
                    context: context,
                    builder: (context) {
                      return StatefulBuilder(
                        builder: (context, setState) {
                          return ChangeWheelchairIp();
                        },
                      );
                    },
                  );
                },
                child: const Text("Set IP"),
              ),
              subtitle: ValueListenableBuilder(
                valueListenable: WheelchairNotifier,
                builder: (context, wheelchairIp, child) {
                  return ValueListenableBuilder(
                    valueListenable: WheelchairPortNotifier,
                    builder: (context, wheechairPort, child) {
                      if (!wheelchairIp.contains("Unknown")) {
                        return Text("$wheelchairIp:$wheechairPort");
                      }
                      return Text(
                        "No IP set for Wheelchair",
                        style: TextStyle(color: Colors.red),
                      );
                    },
                  );
                },
              ),
            ),
            const SizedBox(height: 50.0),
            Text(
              "Current Speed",
              style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 4.0),
            ValueListenableBuilder(
              valueListenable: WheelchairSpeedNotifier,
              builder: (context, currentSpeed, child) {
                return Text(
                  "${currentSpeed} km/h",
                  style: TextStyle(fontSize: 20, fontWeight: FontWeight.bold),
                );
              },
            ),

            const SizedBox(height: 30.0),
            DpadWidget(),

            const SizedBox(height: 30.0),
            ValueListenableBuilder<double>(
              valueListenable: WheelchairController
                  .speedKmH, // Listens directly to the 1-10 notifier
              builder: (context, currentSpeed, child) {
                return Column(
                  children: [
                    const SizedBox(height: 18),

                    // 3. Speed Control Buttons
                    Row(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        Column(
                          children: [
                            IconButton(
                              icon: const Icon(
                                Icons.remove_circle_outline,
                                size: 40,
                              ),
                              onPressed: () => UdpService.sendUDP(
                                "D",
                                WheelchairNotifier.value,
                                WheelchairPortNotifier.value,
                              ), // Decreases notifier value
                            ),
                            Text("Decrease Speed"),
                          ],
                        ),
                        const SizedBox(width: 32),
                        Column(
                          children: [
                            IconButton(
                              icon: const Icon(
                                Icons.add_circle_outline,
                                size: 40,
                              ),

                              onPressed: () => UdpService.sendUDP(
                                "I",
                                WheelchairNotifier.value,
                                WheelchairPortNotifier.value,
                              ), // Increases notifier value
                            ),
                            Text("Increase Speed"),
                          ],
                        ),
                      ],
                    ),
                  ],
                );
              },
            ),
          ],
        ),
      ),
    );
  }
}
