import 'dart:async';
import 'dart:math' as math;
import 'package:flutter/material.dart';
import 'package:wheelchair_smarthome/data/notifiers/notifiers.dart';
import 'package:wheelchair_smarthome/data/services/udp_service.dart';

class JoystickWidget extends StatefulWidget {
  const JoystickWidget({super.key});

  @override
  State<JoystickWidget> createState() => _JoystickWidgetState();
}

class _JoystickWidgetState extends State<JoystickWidget> {
  // Joystick Dimensions
  final double _baseSize = 260.0;
  final double _knobSize = 80.0;

  // State Trackers
  Offset _knobPosition = Offset.zero;
  Timer? _transmissionTimer;
  bool _isDragging = false;

  @override
  void dispose() {
    _transmissionTimer?.cancel();
    super.dispose();
  }

  /// Calculates the normalized values between -1.0 and 1.0 for X and Y axes
  void _updateCoordinates(Offset localPosition) {
    final double maxRadius = (_baseSize - _knobSize) / 2;

    // Calculate center offset point
    final Offset center = Offset(_baseSize / 2, _baseSize / 2);
    final Offset direction = localPosition - center;

    // Clamp coordinates tightly within the physical circular boundary
    double distance = direction.distance;
    if (distance > maxRadius) {
      distance = maxRadius;
    }

    // Update position of the visual handle
    setState(() {
      _knobPosition = distance == 0
          ? Offset.zero
          : Offset.fromDirection(direction.direction, distance);
    });
  }

  void _startTransmission() {
    _isDragging = true;
    MultiUdpService.startListening(5003);

    // Stream coordinates reliably every 100 milliseconds
    _transmissionTimer = Timer.periodic(const Duration(milliseconds: 200), (
      timer,
    ) {
      final double maxRadius = (_baseSize - _knobSize) / 2;

      // Normalize values: X (Left -1.0 to Right 1.0), Y (Forward -1.0 to Backward 1.0)
      // Note: Inverted Y so pushing forward equals a positive value
      double normalizedX = double.parse(
        (_knobPosition.dx / maxRadius).toStringAsFixed(2),
      );
      double normalizedY = double.parse(
        (-(_knobPosition.dy / maxRadius)).toStringAsFixed(2),
      );

      // Construct payload string (e.g., "JOY:0.45,-0.82")
      String joystickPayload = "JOY:$normalizedX,$normalizedY";

      UdpService.sendUDP(
        joystickPayload,
        WheelchairNotifier.value,
        WheelchairPortNotifier.value,
      );
    });
  }

  void _stopTransmission() async {
    _isDragging = false;
    _transmissionTimer?.cancel();

    // Snap visual joystick knob beautifully back to center absolute zero
    setState(() {
      _knobPosition = Offset.zero;
    });

    // Send explicit safety STOP packets using your robust 3-packet safe method
    for (int i = 0; i < 3; i++) {
      UdpService.sendUDP(
        "S",
        WheelchairNotifier.value,
        WheelchairPortNotifier.value,
      );
      await Future.delayed(const Duration(milliseconds: 100));
    }

    if (WheelchairSpeedNotifier.value == "N/A") {
      WheelchairisConnected.value = false;
    }

    WheelchairSpeedNotifier.value = "N/A";
    MultiUdpService.endListeningToPort(5003);
  }

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onPanStart: (details) {
        _startTransmission();
        _updateCoordinates(details.localPosition);
      },
      onPanUpdate: (details) {
        _updateCoordinates(details.localPosition);
      },
      onPanEnd: (_) => _stopTransmission(),
      onPanCancel: () => _stopTransmission(),
      child: Container(
        width: _baseSize,
        height: _baseSize,
        decoration: BoxDecoration(
          color: Colors.grey[900],
          shape: BoxShape.circle,
          border: Border.all(
            color: _isDragging
                ? Colors.blueAccent.withValues(alpha: 0.5)
                : Colors.grey[800]!,
            width: 3,
          ),
          boxShadow: [
            BoxShadow(
              color: _isDragging
                  ? Colors.blueAccent.withValues(alpha: 0.25)
                  : Colors.black.withValues(alpha: 0.4),
              blurRadius: _isDragging ? 25 : 15,
              spreadRadius: _isDragging ? 4 : 2,
            ),
          ],
        ),
        child: Stack(
          alignment: Alignment.center,
          children: [
            // Visual Crosshairs for orientation aid
            Center(
              child: Container(
                width: _baseSize,
                height: 1,
                color: Colors.grey[800]!.withValues(alpha: 0.5),
              ),
            ),
            Center(
              child: Container(
                width: 1,
                height: _baseSize,
                color: Colors.grey[800]!.withValues(alpha: 0.5),
              ),
            ),

            // The Moveable Joystick Thumb Knob
            Transform.translate(
              offset: _knobPosition,
              child: AnimatedContainer(
                duration: _isDragging
                    ? Duration.zero
                    : const Duration(milliseconds: 200),
                curve: Curves.easeOutBack,
                width: _knobSize,
                height: _knobSize,
                decoration: BoxDecoration(
                  shape: BoxShape.circle,
                  gradient: LinearGradient(
                    colors: _isDragging
                        ? [Colors.blueAccent, Colors.blue[700]!]
                        : [Colors.grey[700]!, Colors.grey[600]!],
                    begin: Alignment.topLeft,
                    end: Alignment.bottomRight,
                  ),
                  boxShadow: [
                    BoxShadow(
                      color: Colors.black.withValues(alpha: 0.5),
                      blurRadius: 8,
                      offset: const Offset(0, 4),
                    ),
                  ],
                ),
                child: Icon(
                  Icons.drag_indicator,
                  color: _isDragging ? Colors.white : Colors.grey[300],
                  size: 28,
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
