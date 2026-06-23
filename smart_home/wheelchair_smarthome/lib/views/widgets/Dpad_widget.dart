import 'package:flutter/material.dart';
import 'package:wheelchair_smarthome/data/notifiers/notifiers.dart';
import 'package:wheelchair_smarthome/data/services/udp_service.dart';

const double dpadSize = 300.0;
const double buttonSize = dpadSize / 3;

class DpadWidget extends StatefulWidget {
  const DpadWidget({super.key});

  @override
  State<DpadWidget> createState() => _DpadWidgetState();
}

class _DpadWidgetState extends State<DpadWidget> {
  String _activeDirection = '';

  // Defined glowing and default styles for reuse
  final Color _glowColor = Colors.blueAccent;
  final Color _defaultColor = Colors.grey[700]!;

  @override
  Widget build(BuildContext context) {
    return Container(
      width: dpadSize,
      height: dpadSize,
      decoration: BoxDecoration(
        color: Colors.grey[900],
        shape: BoxShape.circle,
        // Optional: Subtle static shadow for the whole D-pad base
        boxShadow: [
          BoxShadow(
            color: Colors.black.withValues(alpha: 0.3),
            blurRadius: 10,
            spreadRadius: 2,
          ),
        ],
      ),
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          // Row 1: Top / Forward
          Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              _buildDirectionalButton(Icons.arrow_upward, 'F', buttonSize),
            ],
          ),

          // Row 2: Left, Center (Stop), Right
          Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              _buildDirectionalButton(Icons.arrow_back, 'L', buttonSize),
              _buildCenterButton(buttonSize), // Center stays standard for STOP
              _buildDirectionalButton(Icons.arrow_forward, 'R', buttonSize),
            ],
          ),

          // Row 3: Bottom / Backward
          Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              _buildDirectionalButton(Icons.arrow_downward, 'B', buttonSize),
            ],
          ),
        ],
      ),
    );
  }

  Widget _buildDirectionalButton(IconData icon, String command, double size) {
    // Check if THIS specific button is the one active
    final bool isActive = _activeDirection == command;

    return GestureDetector(
      // 1. Press Down: Trigger Command + Start Glow
      onTapDown: (_) async {
        setState(() => _activeDirection = command); // Start Glow

        for (int i = 0; i < 3; i++) {
          UdpService.sendUDP(
            command,
            WheelchairNotifier.value,
            WheelchairPortNotifier.value,
          );
          await Future.delayed(const Duration(milliseconds: 100));
        }
        MultiUdpService.startListening(5003);
        if (WheelchairSpeedNotifier.value == "N/A") {
          WheelchairisConnected.value = false;
        }
      },
      // 2. Release: Trigger STOP Command + End Glow
      onTapUp: (_) => _handleRelease(),
      // 3. Cancel (finger slides off): Trigger STOP Command + End Glow
      onTapCancel: () => _handleRelease(),

      child: AnimatedContainer(
        duration: const Duration(milliseconds: 100), // Smooth glow transition
        width: size,
        height: size,
        decoration: BoxDecoration(
          shape: BoxShape.circle,
          // APPLY GLOW EFFECT HERE
          color: isActive
              ? _glowColor.withValues(alpha: 0.15)
              : Colors.transparent,
          boxShadow: isActive
              ? [
                  BoxShadow(
                    color: _glowColor.withValues(alpha: 0.8),
                    blurRadius: 20,
                    spreadRadius: 5,
                  ),
                ]
              : [], // No shadow when inactive
        ),
        child: Icon(
          icon,
          size: 36,
          // Change icon color when active
          color: isActive ? Colors.white : _glowColor,
        ),
      ),
    );
  }

  Widget _buildCenterButton(double size) {
    // The center STOP button doesn't glow; it's a static safety button.
    return SizedBox(
      width: size,
      height: size,
      child: Container(
        margin: const EdgeInsets.all(8),
        decoration: BoxDecoration(
          color: Colors.redAccent.withValues(alpha: 0.1),
          shape: BoxShape.circle,
          border: Border.all(
            color: Colors.redAccent.withValues(alpha: 0.5),
            width: 2,
          ),
        ),
        child: IconButton(
          icon: const Icon(
            Icons.stop_circle,
            size: 29,
            color: Colors.redAccent,
          ),
          onPressed: () async {
            for (int i = 0; i < 3; i++) {
              UdpService.sendUDP(
                "S",
                WheelchairNotifier.value,
                WheelchairPortNotifier.value,
              );
              await Future.delayed(const Duration(milliseconds: 100));
            }
          }, // send Stop Command,
        ),
      ),
    );
  }

  void _handleRelease() async {
    setState(() => _activeDirection = ''); // End Glow
    for (int i = 0; i < 3; i++) {
      UdpService.sendUDP(
        "S",
        WheelchairNotifier.value,
        WheelchairPortNotifier.value,
      );
      await Future.delayed(const Duration(milliseconds: 100));
    }

    WheelchairSpeedNotifier.value = "N/A";
    MultiUdpService.endListeningToPort(5003);
  }
}
